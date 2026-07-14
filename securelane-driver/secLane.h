#pragma once

#include <ntddk.h>
#include <pshpack1.h>

#ifndef IOCTL_INTERNAL_KEYBOARD_CONNECT
#define IOCTL_INTERNAL_KEYBOARD_CONNECT \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0080, METHOD_NEITHER, FILE_ANY_ACCESS)
#endif

#ifndef IOCTL_INTERNAL_KEYBOARD_DISCONNECT
#define IOCTL_INTERNAL_KEYBOARD_DISCONNECT \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0081, METHOD_NEITHER, FILE_ANY_ACCESS)
#endif

#ifndef KEY_MAKE
#define KEY_MAKE  0x00
#define KEY_BREAK 0x01
#define KEY_E0    0x02
#define KEY_E1    0x04
#endif

#ifndef _KEYBOARD_INPUT_DATA_DEFINED
#define _KEYBOARD_INPUT_DATA_DEFINED
typedef struct _KEYBOARD_INPUT_DATA {
    USHORT UnitId;
    USHORT MakeCode;
    USHORT Flags;
    USHORT Reserved;
    ULONG  ExtraInformation;
} KEYBOARD_INPUT_DATA, * PKEYBOARD_INPUT_DATA;
#endif

#ifndef _SECLANE_CONNECT_DATA_DEFINED
#define _SECLANE_CONNECT_DATA_DEFINED
typedef VOID
(*PSERVICE_CALLBACK_ROUTINE) (
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

typedef struct _CONNECT_DATA {
    PDEVICE_OBJECT ClassDeviceObject;
    PSERVICE_CALLBACK_ROUTINE ClassService;
} CONNECT_DATA, * PCONNECT_DATA;
#endif

PDRIVER_OBJECT g_DriverObject = NULL;
PDEVICE_OBJECT g_ControlDevice = NULL;
BOOLEAN g_SymLinkCreated = FALSE;
UNICODE_STRING g_SymLinkName;

KSPIN_LOCK g_ReportLock;
KSPIN_LOCK g_IoctlLock;

LIST_ENTRY g_ReportList;
LIST_ENTRY g_PendingIoctlList;

typedef struct _REPORT_ENTRY {
    LIST_ENTRY ListEntry;
    SECLANE_KEY_REPORT Report;
} REPORT_ENTRY, * PREPORT_ENTRY;

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT Self;
    PDEVICE_OBJECT NextLowerDevice;
    PDEVICE_OBJECT Pdo;
    CONNECT_DATA UpperConnectData;
    BOOLEAN Connected;
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;


typedef struct _SECLANE_KEY_REPORT {
    USHORT MakeCode;
    USHORT Flags;
} SECLANE_KEY_REPORT, * PSECLANE_KEY_REPORT;
#include <poppack.h>
#define IOCTL_SECLANE_GET_KEY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
DRIVER_ADD_DEVICE KbFilter_AddDevice;

NTSTATUS SecLane_Create(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_Close(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_Read(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_Write(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_InternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_Pnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_Power(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SecLane_SystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS ControlCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ControlDeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID SecLaneCancelRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static VOID AddReportToQueueAndComplete(PSECLANE_KEY_REPORT Report);
static BOOLEAN TryCompletePendingIoctl(VOID);

NTSTATUS KbFilter_Pnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS KbFilter_Power(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS KbFilter_InternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS KbFilter_GenericForward(PDEVICE_OBJECT DeviceObject, PIRP Irp);
