#include "secLane.h"

NTSTATUS KbFilter_AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject) {
    PDEVICE_OBJECT filterDeviceObject = NULL;

    NTSTATUS status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        NULL,
        FILE_DEVICE_KEYBOARD,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &filterDeviceObject
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("SecLane: AddDevice IoCreateDevice failed 0x%X\n", status);
        return status;
    }

    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)filterDeviceObject->DeviceExtension;
    RtlZeroMemory(ext, sizeof(DEVICE_EXTENSION));
    ext->Self = filterDeviceObject;
    ext->Pdo = PhysicalDeviceObject;

    ext->NextLowerDevice = IoAttachDeviceToDeviceStack(filterDeviceObject, PhysicalDeviceObject);
    if (!ext->NextLowerDevice) {
        IoDeleteDevice(filterDeviceObject);
        DbgPrint("SecLane: AddDevice IoAttachDeviceToDeviceStack failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    if (ext->NextLowerDevice->Flags & DO_BUFFERED_IO) {
        filterDeviceObject->Flags |= DO_BUFFERED_IO;
    }
    if (ext->NextLowerDevice->Flags & DO_DIRECT_IO) {
        filterDeviceObject->Flags |= DO_DIRECT_IO;
    }
    if (ext->NextLowerDevice->Flags & DO_POWER_PAGABLE) {
        filterDeviceObject->Flags |= DO_POWER_PAGABLE;
    }
    filterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("SecLane: AddDevice attached filter=%p below lower=%p (Pdo=%p)\n",
        filterDeviceObject, ext->NextLowerDevice, PhysicalDeviceObject);

    return STATUS_SUCCESS;
}


static VOID AddReportToQueueAndComplete(PSECLANE_KEY_REPORT Report) {
    PREPORT_ENTRY entry = (PREPORT_ENTRY)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(REPORT_ENTRY), 'RlcS');
    if (!entry) return;
    entry->Report = *Report;

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_IoctlLock, &oldIrql);

    PIRP pendingIrp = NULL;
    if (!IsListEmpty(&g_PendingIoctlList)) {
        PLIST_ENTRY head = g_PendingIoctlList.Flink;
        PIRP candidate = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        if (IoSetCancelRoutine(candidate, NULL) != NULL) {
            RemoveEntryList(head);
            pendingIrp = candidate;
        }
    }
    KeReleaseSpinLock(&g_IoctlLock, oldIrql);

    if (pendingIrp) {
        PSECLANE_KEY_REPORT outBuf = (PSECLANE_KEY_REPORT)pendingIrp->AssociatedIrp.SystemBuffer;
        *outBuf = *Report;
        pendingIrp->IoStatus.Information = sizeof(SECLANE_KEY_REPORT);
        pendingIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
        ExFreePool(entry);
        return;
    }

    KeAcquireSpinLock(&g_ReportLock, &oldIrql);
    InsertTailList(&g_ReportList, &entry->ListEntry);
    KeReleaseSpinLock(&g_ReportLock, oldIrql);
}

static BOOLEAN TryCompletePendingIoctl(VOID) {
    KIRQL oldIrql;
    KeAcquireSpinLock(&g_ReportLock, &oldIrql);
    if (IsListEmpty(&g_ReportList)) {
        KeReleaseSpinLock(&g_ReportLock, oldIrql);
        return FALSE;
    }
    PLIST_ENTRY listEntry = RemoveHeadList(&g_ReportList);
    KeReleaseSpinLock(&g_ReportLock, oldIrql);

    PREPORT_ENTRY entry = CONTAINING_RECORD(listEntry, REPORT_ENTRY, ListEntry);

    KeAcquireSpinLock(&g_IoctlLock, &oldIrql);
    PIRP pendingIrp = NULL;
    if (!IsListEmpty(&g_PendingIoctlList)) {
        PLIST_ENTRY head = g_PendingIoctlList.Flink;
        PIRP candidate = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        if (IoSetCancelRoutine(candidate, NULL) != NULL) {
            RemoveEntryList(head);
            pendingIrp = candidate;
        }
    }
    KeReleaseSpinLock(&g_IoctlLock, oldIrql);

    if (pendingIrp) {
        PSECLANE_KEY_REPORT outBuf = (PSECLANE_KEY_REPORT)pendingIrp->AssociatedIrp.SystemBuffer;
        *outBuf = entry->Report;
        pendingIrp->IoStatus.Information = sizeof(SECLANE_KEY_REPORT);
        pendingIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
        ExFreePool(entry);
        return TRUE;
    }
    else {
        KeAcquireSpinLock(&g_ReportLock, &oldIrql);
        InsertHeadList(&g_ReportList, &entry->ListEntry);
        KeReleaseSpinLock(&g_ReportLock, oldIrql);
        return FALSE;
    }
}

VOID SecLaneCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_IoctlLock, &oldIrql);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    KeReleaseSpinLock(&g_IoctlLock, oldIrql);

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}