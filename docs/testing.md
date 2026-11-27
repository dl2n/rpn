# Testing RPN

See [Calculator design](design.md) for the architecture and compatibility
constraints. This document distinguishes what can run now from the planned
coverage. The legacy reference is `cf3a8b903afc298fef72a422e47c327fa2714399`.

## Build and run the console checks

Use the Visual Studio 2026 developer PowerShell with the v145 C++ build
tools and Windows SDK installed. From the repository root:

```powershell
foreach ($config in 'Debug', 'Release') {
    msbuild .\rpn64.sln -nologo -p:Platform=x64 "-p:Configuration=$config"
    if ($LASTEXITCODE -ne 0) { throw "$config build failed" }
    & ".\$config\rpn_logic_tests.exe"
    if ($LASTEXITCODE -ne 0) { throw "$config tests failed" }
}
```

The reference console project links the actual application sources and
resources with `WinMain` renamed; its own `main` runs without launching the
GUI or opening the app's registry key. The registry cases use and clean up
unique keys under `HKCU\Software\RPNRegistryTests-*`.

The console baseline now also exercises entry digits, parsing and byte
limits in all four integer radices, plus integer and float stack operations
and float exponent editing. Integer and float cases now run through
`IntegerCommand` and `FloatCommand` with test-owned state and flags.
The composed `CalculatorCommand` checks also interleave two instances,
drive mode switches while entering, check the modal callback before entry
clears, and verify that shared register moves preserve all 64 integer bits.
The float baseline exercises each supported numeric command, including
trig and inverse trig in all three angle units. Error tests verify that
float commits the entry before reporting a modal error, unlike integer
division by zero. The desktop suite also switches modes mid-entry and
checks the saved stacks to verify that the entry flag is shared but the
registers stay independent.

The display tests assert all five strings, including grouping boundaries
in each integer radix, signed formatting, float entry text, angle and
inverse status, and rounding to ten significant digits. The desktop suite
observes the error marker appear and clear after a rejected 64-bit binary
selection. Candidate replay checks that a malformed calculator mode falls
back to integer mode without losing either stack. A restart with an
unsupported saved float display mode must still paint the same Auto rows.
The console tests verify that restore ignores edited precision settings.

## Baseline coverage

| Area | Console or registry checks | Desktop checks |
| --- | --- | --- |
| Integer | Comparable coverage for binary, octal, decimal, and hex: valid/invalid digits, entry and stack movement, arithmetic and bitwise behavior, selected word-size/signedness limits, and error paths. | Input and display for each radix, including mode and word-size selection. |
| Floating point | Entry/edit/commit, the four registers, arithmetic, angle/inverse behavior, and domain errors. Use numeric tolerances where appropriate and exact checks for text and flags. | Representative editing and calculation via physical input, including a mode switch in both directions. |
| Display | Exact text of status/T/Z/Y/X in supported modes, including grouping, padding, and raw float entry text. Test deterministic formatter failures separately from paint. | Compare actual display crops, check row bounds and collapsed geometry, and observe error indication and clearing. |
| Keypad | Check any extracted command/visibility decisions. | Drive actual buttons, keyboard, and menus; verify collapse/expand, focus, and dialog recreation. |
| Registry | Compare saved names, types, and bytes against an independent legacy fixture on an owned test key; exercise missing/partial values and non-persisted fields. | Close the real executable normally, relaunch it, and verify persisted state. |

The registry suite checks saves against independently specified legacy
DWORD values and binary stack bytes, then seeds legacy values directly for
restore. The wire format is unchanged; current saves deliberately write
Auto (`0`) instead of legacy Fix (`1`), and both restore as Auto. The
suite also checks partial keys, type and length mismatches, bad
settings, incompatible radix/word-size values, and ignored display
overrides. The desktop suite verifies the same routines through close and
relaunch. Do not substitute screenshots for numeric assertions or count
tests of unsupported modes as coverage of working features.

## Capturing the legacy desktop baseline

An ordinary interactive Windows session is sufficient; no admin rights,
test account, VM, or display changes are required. The runner temporarily
takes focus, moves the pointer, and uses the US English keyboard layout.
It prints numbered checkpoints in the console. Press **Esc** from any
window to stop the desktop pass; switching away from the calculator also
prevents further input to it. Do not start a run while anyone needs that
desktop.

Use Windows PowerShell 5.1 with MSBuild (the same toolchain as the console
checks). For legacy fixtures, pass a checkout of the pinned reference:

```powershell
.\tests\Run-DesktopTests.ps1 -SourceDirectory C:\rpn-reference -ConfirmDesktopInput -LegacyPlacement `
    -FixtureDirectory .\legacy-fixtures -ArtifactDirectory .\legacy-run -Record
```

The runner builds a fresh x64 Release executable in the artifact directory.
The only source change in that copy is the registry path in `rpn_reg.c`,
redirected to `HKCU\Software\BruceK\RPNDesktopTests\<run-id>`. It never
launches an existing executable, and normal saved calculator state is
neither read nor overwritten. The runner stops only the process it started
and deletes only its unique registry leaf when it exits. The copied
sources and build outputs remain available for inspection.

The runner uses real menu, keypad, and keyboard input against this executable.
It checks collapsed-window geometry, verifies saved register values, and
compares both stacks' displays before and after restart. `-Record` writes to
a new directory, never replacing existing fixtures. An interrupted or
failed capture does not produce `profile.json` and cannot be replayed.
Review every crop for what the scenario is meant to show, including visible row count,
register ordering, radix, float entry text, and numeric result. Record the
reference revision, build configuration, font, and theme alongside the
captured Windows build, DPI, resolution, and executable hash.

The checked-in baseline is `tests\fixtures\legacy-desktop`: x64 Release
from the pinned revision, Windows build 26300.10075, 3840×2160 at 144 DPI,
with the app's `SYSTEM_FIXED_FONT` and ordinary Windows desktop rendering.
The runner disables rounded shell corners only on its own dialog. Use
`-LegacyPlacement` with the pinned source to replay every checkpoint
pixel-for-pixel, including the original restart drift. The default
candidate checks match the unchanged screen fixtures up to restart,
then verify the actual window position and compare the five displayed
rows without any pixel translation. Use `-TraceGeometry` to log the
outer window, client and crop bounds, DPI, and saved position in the
run's `geometry.txt`.

To compare a candidate, omit `-Record`, use the candidate source directory
(the current checkout by default), and pass the reviewed fixtures and a
new artifact directory. Use the same display settings as the recorded run.
A crop mismatch leaves the expected fixture untouched and writes actual/diff
PNGs. Add `-VerifyPlacement` to move the window to a nonzero origin and
exercise both mode switches and restart from there. The build, pixel
comparator, and cancellation notification can be
checked without any app launch, desktop input, or registry access:

```powershell
.\tests\Run-DesktopTests.ps1 -BuildOnly -ArtifactDirectory .\test-build
.\tests\Run-DesktopTests.ps1 -SelfTest -ArtifactDirectory .\fixture-self-test
```

Each milestone must pass its affected tests before and after the change.
Use peer review before the first extraction, before and after recomposition,
and before retiring the legacy comparison. Record decisions and known gaps
in these docs. The peer review confirmed the expected calculations and the
registry redirection. A live negative replay failed at the wrong fixture and
produced a diff. A separate `-VerifyEscape` pass sent Esc through the Windows
input path after relaunch and verified test process and key cleanup. Typing
during a run can interfere with the test; stop with Esc and restart the run
rather than accepting a new fixture.

The hex and float entry checkpoints wait for observable paint around
keyboard/mouse handoffs. `SendInput` confirms enqueueing, not that the
calculator processed the command. The keypad geometry check also requires
two matching samples: window bounds, client bounds, and the menu checkmark
are read separately and can straddle a resize. The suite drives Left,
Backspace, Delete, Up/Down, and View > Keypad, and verifies that the focus
and geometry changes do not break entry. With those checks, three
consecutive candidate and three pinned legacy replays passed without
relaxing any image fixture.

Retire the temporary old-vs-new execution only when permanent tests cover
its observations. Keep the standalone tests and reviewed fixtures.

## Follow-up: black-box arithmetic boundary matrices

After the baseline, build table-driven coverage for **every supported
operation crossed with its applicable arithmetic boundary cases**. Drive
the public calculator command path rather than individual operators. For
each case, independently assert the resulting register values, stack and
error behavior, and exact text from the production display path. Keep
desktop checks for wiring and painting; screenshots are not the numeric
oracle. Mark inapplicable combinations explicitly.

- **Integer:** Cover signed and unsigned 8-, 16-, 32-, and 64-bit settings,
  and every supported radix for entry and rendering. Include zero,
  sign-bit transitions, min/max and adjacent values, overflow and
  underflow, mixed-sign operands, zero divisors, and shift counts around
  0, the word width, and 63/64/65. Include signed and unsigned cases with
  the same register bits but different expected division and remainder;
  assert both the numeric results and their displayed text.
- **Floating point:** Cover every supported operation with applicable
  sign, magnitude, precision, and domain boundaries: signed zero, finite
  extremes, normal/subnormal boundaries, rounding and cancellation,
  overflow/underflow, and non-finite results where reachable. Include
  angle units and inverse operations where applicable. Use an independent
  reference and operation-appropriate tolerances for approximate values,
  exact classification/sign where meaningful, and separate exact
  assertions for the active display text and notation.

Keep these known integer inconsistencies as separate behavior fixes:

1. `/` and `%` use unsigned `NUMBER` operands even in two's-complement
   signed mode (`DivideOp` and `ModOp` in `rpn.c`). Add cases with negative
   dividend and divisor before fixing it; define the signed minimum
   divided by `-1` case without relying on native signed overflow.
2. The help promises six-bit shift counts, but the variable shift
   operators do not explicitly mask the count. The help also disagrees
   with the implementation about which register supplies the count.
   Confirm the intended operand order and count rule, then test and align
   code and help without using undefined C shifts as expected behavior.
3. Signed decimal entry rejects all digit appends to a negative X at
   8, 16, and 32 bits, not just appends near the lower limit. The Windows
   `MINCHAR`, `MINSHORT`, and `MINLONG` macros supply bit patterns that
   this comparison treats as positive. For example, at 8 bits, appending
   any digit `0` through `8` to an active `-12` should produce `-120`
   through `-128`; only `9` should be rejected. Check an ordinary
   negative prefix as well as this boundary when fixing the limits.

Use expected results derived from the agreed arithmetic contract, not
from the current implementation. Confirm targeted cases fail on the old
code and pass with their fixes; do not make known wrong results permanent
passing assertions.

## Follow-up: floating-point display controls

Implement true Fix (a fixed number of decimal places), Scientific, and
Engineering formatting as distinct modes, with exact-text and keypad/menu
tests. The calculator currently uses Auto with ten significant digits and
has no working display-format menu. Consider a separate significant-digit
control later; it is not the
same setting as the number of decimal places in Fix.
