// app.c - usermode consumer for the SecLane class-filter driver.
// Converts the raw Set-1 scan code into a readable US-layout key name
// before printing.
//
// KNOWN LIMITATION: the driver currently strips the E0/E1 extended-key
// bits before forwarding a report to this app (KbFilter_ServiceCallback
// only keeps the KEY_BREAK bit in Flags). That means this table CANNOT
// tell apart a base key from its E0-extended twin that shares the same
// MakeCode - e.g. Right Ctrl (E0 1D) prints the same as Left Ctrl (1D),
// the arrow-key cluster (E0 48/4B/4D/50) prints the same as the numpad
// digits (48/4B/4D/50) when Num Lock is off, etc. Table below is
// annotated with a "*" on every ambiguous entry. Fixing this properly
// means having the driver forward the E0/E1 bits too (2 more bits in
// SECLANE_KEY_REPORT.Flags, trivial change) - ask if you want that.
#include <windows.h>
#include <stdio.h>
#include "secLane.h"

static const char* ScanCodeToName(USHORT makeCode) {
    switch (makeCode) {
    case 0x01: return "Esc";
    case 0x02: return "1";
    case 0x03: return "2";
    case 0x04: return "3";
    case 0x05: return "4";
    case 0x06: return "5";
    case 0x07: return "6";
    case 0x08: return "7";
    case 0x09: return "8";
    case 0x0A: return "9";
    case 0x0B: return "0";
    case 0x0C: return "-";
    case 0x0D: return "=";
    case 0x0E: return "Backspace";
    case 0x0F: return "Tab";
    case 0x10: return "Q";
    case 0x11: return "W";
    case 0x12: return "E";
    case 0x13: return "R";
    case 0x14: return "T";
    case 0x15: return "Y";
    case 0x16: return "U";
    case 0x17: return "I";
    case 0x18: return "O";
    case 0x19: return "P";
    case 0x1A: return "[";
    case 0x1B: return "]";
    case 0x1C: return "Enter*";        // * shared with Numpad Enter (E0 1C)
    case 0x1D: return "Ctrl*";         // * shared with Right Ctrl (E0 1D)
    case 0x1E: return "A";
    case 0x1F: return "S";
    case 0x20: return "D";
    case 0x21: return "F";
    case 0x22: return "G";
    case 0x23: return "H";
    case 0x24: return "J";
    case 0x25: return "K";
    case 0x26: return "L";
    case 0x27: return ";";
    case 0x28: return "'";
    case 0x29: return "`";
    case 0x2A: return "Left Shift";
    case 0x2B: return "\\";
    case 0x2C: return "Z";
    case 0x2D: return "X";
    case 0x2E: return "C";
    case 0x2F: return "V";
    case 0x30: return "B";
    case 0x31: return "N";
    case 0x32: return "M";
    case 0x33: return ",";
    case 0x34: return ".";
    case 0x35: return "/*";           // * shared with Numpad / (E0 35)
    case 0x36: return "Right Shift";
    case 0x37: return "Numpad *";
    case 0x38: return "Alt*";         // * shared with Right Alt (E0 38)
    case 0x39: return "Space";
    case 0x3A: return "Caps Lock";
    case 0x3B: return "F1";
    case 0x3C: return "F2";
    case 0x3D: return "F3";
    case 0x3E: return "F4";
    case 0x3F: return "F5";
    case 0x40: return "F6";
    case 0x41: return "F7";
    case 0x42: return "F8";
    case 0x43: return "F9";
    case 0x44: return "F10";
    case 0x45: return "Num Lock";
    case 0x46: return "Scroll Lock";
    case 0x47: return "Numpad 7 / Home*";
    case 0x48: return "Numpad 8 / Up*";
    case 0x49: return "Numpad 9 / PgUp*";
    case 0x4A: return "Numpad -";
    case 0x4B: return "Numpad 4 / Left*";
    case 0x4C: return "Numpad 5";
    case 0x4D: return "Numpad 6 / Right*";
    case 0x4E: return "Numpad +";
    case 0x4F: return "Numpad 1 / End*";
    case 0x50: return "Numpad 2 / Down*";
    case 0x51: return "Numpad 3 / PgDn*";
    case 0x52: return "Numpad 0 / Insert*";
    case 0x53: return "Numpad . / Delete*";
    case 0x57: return "F11";
    case 0x58: return "F12";
    default:   return NULL; // fall back to printing the raw hex code
    }
}

int main() {
    HANDLE hDevice = CreateFile(L"\\\\.\\SecLane", GENERIC_READ, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Error opening device: %lu\n", GetLastError());
        return 1;
    }
    printf("Secure keyboard sniffer started. Press keys...\n");
    printf("(Names ending in '*' are ambiguous - shared with an E0-extended\n");
    printf(" key the driver doesn't currently distinguish. See comment in\n");
    printf(" app.c if you want that fixed.)\n\n");

    SECLANE_KEY_REPORT report;
    DWORD bytesReturned;

    while (TRUE) {
        BOOL ret = DeviceIoControl(hDevice, IOCTL_SECLANE_GET_KEY, NULL, 0,
            &report, sizeof(report), &bytesReturned, NULL);
        if (ret && bytesReturned == sizeof(report)) {
            BOOLEAN isBreak = (report.Flags & 0x01) != 0; // KEY_BREAK
            const char* name = ScanCodeToName(report.MakeCode);
            if (name) {
                printf("[0x%02X] %-24s %s\n", report.MakeCode, name, isBreak ? "UP" : "DOWN");
            }
            else {
                printf("[0x%02X] (unknown)               %s\n", report.MakeCode, isBreak ? "UP" : "DOWN");
            }
        }
        else {
            printf("IOCTL failed or cancelled: %lu\n", GetLastError());
        }
    }

    CloseHandle(hDevice);
    return 0;
}