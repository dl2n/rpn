# Calculator design and test seams

The reference implementation is `cf3a8b903afc298fef72a422e47c327fa2714399`.
See [Testing](testing.md) for running checks and updating the baseline.

## What exists today

The application is native Win32 C. `rpn.rc` supplies the dialogs, menus, and
keypad controls. `WndProc` in `rpn.c` translates keyboard input and routes it
alongside button and menu commands. The Win32 adapters handle help and
clipboard; numeric commands and mode changes go through `CalculatorCommand`.
It routes to the integer or floating-point handler using a `CALCULATOR_STATE`
that owns both stacks and the shared flags. The synchronous error callback
preserves each mode's ordering: integer division reports before ending entry,
while float commits before operating on X or reporting an error. Editing commands
work on the entry text; sign and exponent insertion reject a full
buffer rather than overwriting neighboring state. Drop ends entry
without committing it.
On a mode change, the Win32 adapter posts a message that recreates the
dialog without committing entry or altering either stack. Up/Down and
View > Keypad resize that window and update the menu; when
collapsed, the keypad is clipped below the display, not removed.

| State | Current behavior |
| --- | --- |
| Integer | Four 64-bit registers, with word-size and signedness rules. Binary, octal, decimal, and hex are radices of the same calculator. Entry accumulates directly in X. |
| Floating point | Four `double` registers, plus entry text, angle type, precision/display settings, and inverse state. The entry text is converted when committed. |
| Shared | Active mode, entry and error flags, help-key mode, window location, and keypad visibility. The two stacks survive mode switches independently. |

`ComposeDisplayLines` produces the five strings (status, T, Z, Y, X) from
calculator state without drawing. One GDI adapter paints both modes and
starts the timer that clears the status error marker. During float entry,
the X row shows the raw entry text. If a number cannot be formatted into
its row, composition reports failure and leaves that row blank.

Registry IO in `rpn_reg.c` runs on startup and exit against
`HKCU\Software\BruceK\RPN`. The key stores the two four-register binary records
in X/Y/Z/T order and DWORDs for radix, last radix, word size, integer type,
calculator mode, window X/Y, keypad visibility, and float precision, display,
and angle settings. Active entry text, entry/inverse flags, and transient
errors are not stored. `SaveRPNStateToKey` and `RestoreRPNStateFromKey` work
with a caller-owned key; the startup and exit wrappers own the production
path. Missing values keep startup defaults without an error. Bad types,
sizes, or unsupported values default only the affected fields and trigger
the status error marker. An invalid stack record is discarded as a unit;
valid records are kept byte for byte. The registry still records
`FloatDisplayMode` and `SignificantDigits` as DWORDs for compatibility.
Startup and restore use Auto with ten significant digits, ignoring overrides.

## Intended seams

1. Make calculation commands runnable against explicit state without a
   window. Preserve synchronous error presentation where its ordering
   matters. Keep clipboard, help, focus, redraw, and dialog creation in the
   Win32 layer.
2. Share command routing and stack mechanics only where the observed behavior
   agrees. For example, both Swap variants exchange X and Y, but floating
   point first commits any active text entry; integer mode works directly on
   X. Keep the two numeric types and mode-specific entry rules distinct. A
   calculator state still shares entry and error flags between its modes;
   separate calculator instances should not share them.
3. Compose the five strings independently of the GDI drawing code. Leave
   colors, font, positioning, clipping, invalidation, and timers in the
   window adapter.
4. Separate registry IO on an already-open test key from startup/exit's
   selection of the production key. Keep the value names, types, and stack
   bytes compatible with existing installations.

## Shared register moves

The C++ trial passed, but peer review found that writing five register moves
once did not justify adding a second language and linkage wrappers. Both
targets stay in C. A file-local macro in `rpn_stack.c` generates the same
five functions for the two typed stack records. Numeric coercion, entry,
and modal error handling remain in the mode-specific command paths.
Compile-time checks in `rpn_reg.c` guard the size and order of both persisted
stack records.

## Baseline limits, not feature requests

- The commands reject 64-bit binary display: grouped 32-bit binary fits
  the 35-column row exactly, but grouped 64-bit binary needs 71 columns.
  Restore changes the active radix to hex for this combination, keeping
  64-bit width and Last Base history. One's complement is disabled in the
  menu and not supported by the formatter.
- Auto is the only active float display mode. The legacy Fix code path
  uses the same conversion, and the float dialog has no display format
  menu. Proper Fix means a fixed number of decimal places, not a
  significant-digit limit. Implement Fix, Sci, and Eng separately from
  any future significant-digit control.
- The legacy `WM_MOVE` handler estimated the outer origin by subtracting
  system metrics from the client origin. At 150% scaling the reference
  saved logical `(5,5)` while at physical `(0,0)`, then restored at `(8,8)`.
  This moved all five float rows down one pixel within the screen crop.
  The current handler saves the actual outer bounds of the active,
  visible, non-minimized dialog using `GetWindowRect`. The registry
  values remain DWORDs and are restored as signed coordinates.
- Document unexpected but reproducible behavior separately from any proposed
  fix. Do not turn undefined outcomes into compatibility promises.
