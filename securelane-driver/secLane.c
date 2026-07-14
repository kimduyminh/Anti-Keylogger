#include "secLane.h"

VOID KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
);

NTSTATUS ControlCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS ControlDeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION st = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = st->Parameters.DeviceIoControl.IoControlCode;

    switch (code) {
    case IOCTL_SECLANE_GET_KEY:
        if (st->Parameters.DeviceIoControl.OutputBufferLength < sizeof(SECLANE_KEY_REPORT)) {
            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_BUFFER_TOO_SMALL;
        }
        if (TryCompletePendingIoctl()) {
            return STATUS_SUCCESS;
        }
        IoMarkIrpPending(Irp);
        {
            KIRQL oldIrql;
            KeAcquireSpinLock(&g_IoctlLock, &oldIrql);
            IoSetCancelRoutine(Irp, SecLaneCancelRoutine);
            if (Irp->Cancel) {
                if (IoSetCancelRoutine(Irp, NULL) != NULL) {
                    KeReleaseSpinLock(&g_IoctlLock, oldIrql);
                    Irp->IoStatus.Status = STATUS_CANCELLED;
                    Irp->IoStatus.Information = 0;
                    IoCompleteRequest(Irp, IO_NO_INCREMENT);
                    return STATUS_CANCELLED;
                }
                KeReleaseSpinLock(&g_IoctlLock, oldIrql);
                return STATUS_PENDING;
            }
            InsertTailList(&g_PendingIoctlList, &Irp->Tail.Overlay.ListEntry);
            KeReleaseSpinLock(&g_IoctlLock, oldIrql);
        }
        return STATUS_PENDING;
    default:
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }
}

NTSTATUS KbFilter_GenericForward(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->NextLowerDevice, Irp);
}

NTSTATUS KbFilter_Power(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(ext->NextLowerDevice, Irp);
}

NTSTATUS KbFilter_Pnp(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->MinorFunction == IRP_MN_REMOVE_DEVICE) {
        IoSkipCurrentIrpStackLocation(Irp);
        NTSTATUS status = IoCallDriver(ext->NextLowerDevice, Irp);
        IoDetachDevice(ext->NextLowerDevice);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->NextLowerDevice, Irp);
}

NTSTATUS SecLane_Create(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        return ControlCreateClose(DeviceObject, Irp);
    }
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS SecLane_Close(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        return ControlCreateClose(DeviceObject, Irp);
    }
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS SecLane_Read(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    return KbFilter_GenericForward(DeviceObject, Irp);
}

NTSTATUS SecLane_Write(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    return KbFilter_GenericForward(DeviceObject, Irp);
}

NTSTATUS SecLane_DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        return ControlDeviceIoControl(DeviceObject, Irp);
    }
    return KbFilter_GenericForward(DeviceObject, Irp);
}

NTSTATUS SecLane_InternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    return KbFilter_InternalDeviceControl(DeviceObject, Irp);
}

NTSTATUS SecLane_Pnp(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
    return KbFilter_Pnp(DeviceObject, Irp);
}

NTSTATUS SecLane_Power(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        PoStartNextPowerIrp(Irp);
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
    return KbFilter_Power(DeviceObject, Irp);
}

NTSTATUS SecLane_SystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == g_ControlDevice) {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
    return KbFilter_GenericForward(DeviceObject, Irp);
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    if (g_SymLinkCreated) {
        IoDeleteSymbolicLink(&g_SymLinkName);
        g_SymLinkCreated = FALSE;
    }
    if (g_ControlDevice) {
        IoDeleteDevice(g_ControlDevice);
        g_ControlDevice = NULL;
    }

    DbgPrint("SecLane Unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    g_DriverObject = DriverObject;

    KeInitializeSpinLock(&g_ReportLock);
    KeInitializeSpinLock(&g_IoctlLock);
    InitializeListHead(&g_ReportList);
    InitializeListHead(&g_PendingIoctlList);

    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\SecLane");
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\DosDevices\\SecLane");
    g_SymLinkName = symLink;

    PDEVICE_OBJECT controlDev;
    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        &devName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &controlDev
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("SecLane: failed to create control device 0x%X\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SecLane: failed to create symbolic link 0x%X\n", status);
        IoDeleteDevice(controlDev);
        return status;
    }

    controlDev->Flags |= DO_BUFFERED_IO;
    controlDev->Flags &= ~DO_DEVICE_INITIALIZING;
    g_ControlDevice = controlDev;
    g_SymLinkCreated = TRUE;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = SecLane_Create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = SecLane_Close;
    DriverObject->MajorFunction[IRP_MJ_READ] = SecLane_Read;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = SecLane_Write;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SecLane_DeviceControl;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = SecLane_InternalDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = SecLane_Pnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = SecLane_Power;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = SecLane_SystemControl;

    DriverObject->DriverExtension->AddDevice = KbFilter_AddDevice;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("SecLane Driver Loaded (class filter mode)\n");
    return STATUS_SUCCESS;
}