#include "secLane.h"

NTSTATUS KbFilter_InternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(CONNECT_DATA)) {
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_INVALID_PARAMETER;
        }
        if (ext->Connected) {
            Irp->IoStatus.Status = STATUS_SHARING_VIOLATION;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SHARING_VIOLATION;
        }
        {
            PCONNECT_DATA connectData =
                (PCONNECT_DATA)stack->Parameters.DeviceIoControl.Type3InputBuffer;
            if (!connectData) {
                Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return STATUS_INVALID_PARAMETER;
            }

            ext->UpperConnectData = *connectData;
            ext->Connected = TRUE;

            connectData->ClassDeviceObject = ext->Self;
            connectData->ClassService = KbFilter_ServiceCallback;
        }
        break;
    default:
        break;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->NextLowerDevice, Irp);
}


VOID KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    for (PKEYBOARD_INPUT_DATA cur = InputDataStart; cur < InputDataEnd; cur++) {
        SECLANE_KEY_REPORT rpt;
        rpt.MakeCode = cur->MakeCode;
        rpt.Flags = (USHORT)(cur->Flags & KEY_BREAK);
        AddReportToQueueAndComplete(&rpt);
        // Encoded with S
        cur->MakeCode = 0x1F;
        cur->Flags = (USHORT)(cur->Flags & KEY_BREAK);
    }

    if (ext->Connected && ext->UpperConnectData.ClassService) {
        (*(PSERVICE_CALLBACK_ROUTINE)ext->UpperConnectData.ClassService)(
            ext->UpperConnectData.ClassDeviceObject,
            InputDataStart,
            InputDataEnd,
            InputDataConsumed
            );
    }
}