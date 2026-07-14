# SecLane

A Windows kernel-mode keyboard class filter driver PoC that defeats
user-mode keyloggers by intercepting real keystrokes before they reach
the OS, replacing them with a phantom key for every other consumer, and
delivering the real scan codes to a single trusted user-mode application
through a separate, authenticated-later channel.

> **Status:** security research. Not production-ready.
> See [Known Limitations](#known-limitations) before relying on this for
> anything beyond a lab demo.

## How it works

SecLane installs itself as an **upper filter on the keyboard's own device
instance**, positioned below `kbdclass.sys` and above the keyboard port
driver (`kbdhid.sys` / `i8042prt.sys`):

```
kbdclass.sys  (top - Windows' keyboard class driver)
     |
SecLane.sys   (this driver)
     |
port driver   (kbdhid.sys / i8042prt.sys)
     |
hardware      (PS/2, USB HID, or a VM's synthetic keyboard)
```

Every keystroke - regardless of whether it comes from a physical PS/2 or
USB keyboard, or a VM's synthetic/RDP-redirected one - passes through
this exact point on its way to Windows.

### The interception mechanism

1. On startup, `kbdclass.sys` sends `IOCTL_INTERNAL_KEYBOARD_CONNECT`
   down the stack to register a callback function pointer
   (`ClassService`) that the port driver should invoke for every
   keystroke.
2. Because SecLane sits directly below `kbdclass.sys`, this IOCTL passes
   through it first. SecLane saves the real callback pointer and splices
   in its own (`KbFilter_ServiceCallback`) before forwarding the IOCTL
   down.
3. From then on, the port driver calls SecLane's callback directly for
   every keystroke (a plain function call, not an IRP). SecLane:
   - copies the real scan code into an internal queue for the user-mode
     app to read later,
   - overwrites the scan code with a phantom key (`'S'`, US Set-1 scan
     code `0x1F`) before forwarding,
   - calls the real, saved `kbdclass` callback with the now-phantom data.

Everything above SecLane in the stack - Win32k, raw input APIs, any
user-mode keylogger - only ever sees the phantom key. The real scan code
is only obtainable via SecLane's own control device.

### The user-mode channel

SecLane also creates a standalone control device (`\Device\SecLane`,
symbolic link `\\.\SecLane`), independent of the keyboard stack above.
A trusted app opens it and calls `DeviceIoControl` with
`IOCTL_SECLANE_GET_KEY` to retrieve real keystrokes as they happen
(cancel-safe pending-IRP queue under the hood, so a killed client doesn't
leave orphaned IRPs behind).

## Project structure

| File | Purpose |
|---|---|
| `secLane.c` | The kernel driver (WDM keyboard class filter) |
| `secLane.h` | Shared wire-format struct + IOCTL code (driver + app) |
| `securelane.inf` | Driver install package (registers the upper filter) |
| `app.c` | Minimal user-mode test client (prints real key names) |

## Building

Standard WDM driver build via Visual Studio + WDK:

1. Open the driver project, build `x64/Release` (or `Debug`).
2. Output: `securelane-driver.sys`.

Test-signing must be enabled on the target machine
(`bcdedit /set testsigning on`, reboot) since this isn't
WHQL/attestation-signed.

## Installing

This is a **PnP class filter**, not a standalone service - it cannot be
started with `sc start`. Install via INF:

```
pnputil.exe /add-driver securelane.inf /install
```

`securelane.inf` matches keyboards by **compatible ID**
(`HID_DEVICE_UP:0001_U:0006` for any HID boot keyboard, plus `*PNP0303`
for PS/2), so it should match most real keyboards without edits. If your
test device doesn't match either (some VM synthetic keyboards are a
known exception), find its exact Hardware ID via Device Manager →
Properties → Details → *Hardware Ids*, and add it as its own line under
`[Standard.NTamd64]`.

`UpperFilters` changes only take effect the next time the device
(re)starts - disable/re-enable it in Device Manager, or reboot.

## Testing

Build and run `app.c` (link against `secLane.h`) with the driver
installed and attached (confirm via DbgView: look for `SecLane: AddDevice
attached filter=...`). Typing anywhere on the system should show only the
phantom `'S'` key (to every other app, including keyloggers), while the
app prints the real key names as you type.

## Known Limitations

- **No consumer authentication.** Any process that can open
  `\\.\SecLane` gets the real keystrokes - there's no ACL or signature
  check on the caller yet. This needs to be solved before this is usable
  in a real product.
- **Simplified `IRP_MN_REMOVE_DEVICE` handling.** Forward-then-detach,
  no `IoAcquireRemoveLock` scheme - a theoretical race exists if a
  keystroke arrives at the exact moment the device is removed.
- **E0/E1 extended-key bits are stripped** before reaching the user-mode
  app, so keys that share a base scan code with an extended variant
  (Right Ctrl/Alt, the arrow-key cluster vs. Numpad) aren't
  distinguishable on the wire.
- **Single global report queue** - multiple physical keyboards would
  interleave into the same queue.
- Not fuzz-tested, not verifier-tested, not audited for production use.

## License

TODO - add a license before making this repository public.
