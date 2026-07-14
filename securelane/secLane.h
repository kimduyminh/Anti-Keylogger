/*
 * secLane.h - Shared definitions between the SecLane keyboard class
 * filter driver (secLane.c) and its user-mode consumer app.
 *
 * Intentionally contains ONLY the wire-format contract (struct + IOCTL
 * code) - no driver-only globals/types here, so this same file can be
 * included unmodified by both the kernel driver and a plain usermode
 * Win32 app (both ntddk.h and windows.h provide pshpack1.h/poppack.h).
 */
#pragma once

#include <pshpack1.h>
#include <windef.h>
typedef struct _SECLANE_KEY_REPORT {
    USHORT MakeCode; // raw Set-1 scan code, same value as
                      // KEYBOARD_INPUT_DATA.MakeCode in kbdclass's pipeline
                      // (USB HID keyboards are translated to Set-1-style
                      // codes by kbdhid.sys before reaching this point, so
                      // this value is consistent regardless of physical
                      // keyboard type - PS/2, USB, or a VM's synthetic one).
    USHORT Flags;     // bit0 (0x01) = KEY_BREAK: 1 = key released, 0 = key
                      // pressed. All other bits are always 0 here (E0/E1
                      // extended-key bits are stripped by the driver so
                      // the phantom key's up/down pairing stays clean).
} SECLANE_KEY_REPORT, * PSECLANE_KEY_REPORT;
#include <poppack.h>

// Must match the control device created by the driver
// (FILE_DEVICE_UNKNOWN, \\.\SecLane).
#define IOCTL_SECLANE_GET_KEY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)