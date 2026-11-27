#requires -Version 5.1
[CmdletBinding(DefaultParameterSetName = 'Desktop')]
param(
    [Parameter(ParameterSetName = 'Desktop')]
    [Parameter(ParameterSetName = 'BuildOnly')]
    [string] $SourceDirectory,

    [Parameter(Mandatory, ParameterSetName = 'Desktop')]
    [switch] $ConfirmDesktopInput,

    [Parameter(Mandatory, ParameterSetName = 'Desktop')]
    [string] $FixtureDirectory,

    [Parameter(ParameterSetName = 'Desktop')]
    [switch] $VerifyEscape,

    [Parameter(ParameterSetName = 'Desktop')]
    [switch] $TraceGeometry,

    [Parameter(ParameterSetName = 'Desktop')]
    [switch] $VerifyPlacement,

    [Parameter(Mandatory, ParameterSetName = 'SelfTest')]
    [switch] $SelfTest,

    [Parameter(Mandatory, ParameterSetName = 'BuildOnly')]
    [switch] $BuildOnly,

    [Parameter(Mandatory)]
    [string] $ArtifactDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop') {
    throw 'Run this script with Windows PowerShell 5.1 (powershell.exe).'
}
if (!$SourceDirectory) {
    $SourceDirectory = Split-Path -Parent $PSScriptRoot
}

function New-OwnedDirectory([string] $Path) {
    $resolved = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    if (Test-Path -LiteralPath $resolved) {
        throw "Output directory already exists: $resolved"
    }
    return [IO.Directory]::CreateDirectory($resolved).FullName
}

function Build-TestCopy([string] $Output, [string] $RegistryPath) {
    $build = New-OwnedDirectory (Join-Path $Output 'build')
    $projectName = 'rpn64.vcxproj'
    $projectPath = Join-Path $SourceDirectory $projectName
    [xml] $project = Get-Content -LiteralPath $projectPath -Raw
    $inputs = @($projectName)
    foreach ($item in $project.SelectNodes('//*[@Include]')) {
        if ($item.LocalName -in 'ClCompile', 'ClInclude', 'Image', 'ResourceCompile') {
            $inputs += $item.GetAttribute('Include')
        }
    }
    foreach ($file in $inputs | Select-Object -Unique) {
        if ([IO.Path]::IsPathRooted($file) -or $file -match '(^|[\\/])\.\.([\\/]|$)') {
            throw "Project input escapes its source directory: $file"
        }
        $destination = Join-Path $build $file
        $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
        Copy-Item -LiteralPath (Join-Path $SourceDirectory $file) -Destination $destination
    }

    $registrySource = Join-Path $build 'rpn_reg.c'
    $productionKey = 'const TCHAR szRegKey[] = "Software\\BruceK\\RPN";'
    $lines = [IO.File]::ReadAllText($registrySource)
    if ([regex]::Matches($lines, [regex]::Escape($productionKey)).Count -ne 1) {
        throw 'Could not isolate the registry key in the test build. No executable was launched.'
    }
    $testKey = 'const TCHAR szRegKey[] = "' + $RegistryPath.Replace('\', '\\') + '";'
    [IO.File]::WriteAllText($registrySource, $lines.Replace($productionKey, $testKey))

    Push-Location -LiteralPath $build
    try {
        & msbuild.exe $projectName -nologo -v:quiet -p:Platform=x64 `
            -p:Configuration=Release '-p:SolutionDir=.\' | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'The desktop test build failed.' }
    } finally {
        Pop-Location
    }
    $target = Join-Path $build 'Release\rpn64.exe'
    $image = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($target))
    if (!$image.Contains("$RegistryPath$([char]0)") -or $image.Contains("Software\BruceK\RPN$([char]0)")) {
        throw 'The compiled test executable failed the registry isolation check. It was not launched.'
    }
    return $target
}

if ($PSCmdlet.ParameterSetName -eq 'Desktop' -and !$ConfirmDesktopInput) {
    throw 'Confirm that the test can take focus, move the pointer, and reserve Esc.'
}
if ($BuildOnly) {
    $output = New-OwnedDirectory $ArtifactDirectory
    $key = 'Software\BruceK\RPNDesktopTests\' + [Guid]::NewGuid().ToString('N')
    Write-Output (Build-TestCopy $output $key)
    return
}

Add-Type -Path @(
    (Join-Path $PSScriptRoot 'RpnDesktop.cs'),
    (Join-Path $PSScriptRoot 'RpnDisplayFixture.cs')
) -ReferencedAssemblies 'System.dll', 'System.Drawing.dll'

if ($PSCmdlet.ParameterSetName -eq 'SelfTest') {
    [RpnDisplayFixture]::SelfTest((New-OwnedDirectory $ArtifactDirectory))
    [RpnDesktop]::SelfTestCancellation()
    Write-Output 'Desktop fixture comparison and cancellation checks passed (no app launch or registry access).'
    return
}

$displayProfile = [RpnDesktop]::CheckEnvironment()
$os = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
$metadata = @{
    WindowsBuild = "$($os.CurrentBuildNumber).$($os.UBR)"
    Dpi = $displayProfile[2]
    Keyboard = 'en-US'
    Resolution = "$($displayProfile[0])x$($displayProfile[1])"
}
$artifactPath = New-OwnedDirectory $ArtifactDirectory
$fixturePath = (Get-Item -LiteralPath $FixtureDirectory).FullName
$recorded = Get-Content -LiteralPath (Join-Path $fixturePath 'profile.json') -Raw | ConvertFrom-Json
foreach ($field in $metadata.Keys) {
    if ($recorded.$field -ne $metadata[$field]) {
        throw "Desktop profile mismatch for $field`: expected $($recorded.$field), got $($metadata[$field])."
    }
}

$registryPath = 'Software\BruceK\RPNDesktopTests\' + [Guid]::NewGuid().ToString('N')
$Executable = Build-TestCopy $artifactPath $registryPath
$existing = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registryPath)
if ($null -ne $existing) {
    $existing.Dispose()
    throw 'The test registry key unexpectedly already exists. No executable was launched.'
}

function Write-Checkpoint([int] $Number, [string] $Description) {
    [RpnDesktop]::CheckEscape()
    Write-Host "$Number / $checkpointCount : $Description"
}

function Wait-Dialog([Diagnostics.Process] $Process, [string] $ClassName) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    do {
        if ($Process.HasExited) { throw "RPN exited early with code $($Process.ExitCode)." }
        $window = [RpnDesktop]::ActiveDialog($Process.Id, $ClassName)
        if ($window -ne [IntPtr]::Zero) { return $window }
        Start-Sleep -Milliseconds 25
    } while ($clock.ElapsedMilliseconds -lt 5000)
    throw "RPN window $ClassName did not become the foreground dialog."
}

function Invoke-Menu([IntPtr] $Window, [char] $Menu, [char] $Item) {
    [RpnDesktop]::Key($Window, [UInt16]$Menu, 0x12)
    $clock = [Diagnostics.Stopwatch]::StartNew()
    do {
        if ([RpnDesktop]::MenuActive($Window)) {
            [RpnDesktop]::Key($Window, [UInt16]$Item, 0)
            $clock.Restart()
            while ([RpnDesktop]::MenuActive($Window)) {
                if ($clock.ElapsedMilliseconds -ge 5000) { throw "RPN menu $Menu did not close." }
                Start-Sleep -Milliseconds 25
            }
            return
        }
        Start-Sleep -Milliseconds 25
    } while ($clock.ElapsedMilliseconds -lt 5000)
    throw "RPN menu $Menu did not open."
}

function Send-RpnKeys([IntPtr] $Window, [UInt16[]] $Keys) {
    foreach ($key in $Keys) { [RpnDesktop]::Key($Window, $key, 0) }
}

function Wait-Keypad([IntPtr] $Window, [int] $Display, [bool] $Expanded) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $previous = $null
    do {
        [RpnDesktop]::CheckEscape()
        $bounds = [RpnDesktop]::Geometry($Window, $Display)
        $displayInside = $bounds[1].Contains($bounds[2])
        $belowDisplay = $bounds[1].Bottom - $bounds[2].Bottom
        if ($displayInside -and [RpnDesktop]::KeypadChecked($Window) -eq $Expanded -and
            (($Expanded -and $belowDisplay -gt 0) -or (!$Expanded -and $belowDisplay -eq 0))) {
            # Outer bounds, client bounds, and menu state are separate reads.
            # Require two matching samples to avoid accepting a split sample.
            if ($null -ne $previous -and $bounds[0] -eq $previous[0] -and
                $bounds[1] -eq $previous[1] -and $bounds[2] -eq $previous[2]) {
                return $bounds[0]
            }
            $previous = $bounds
        } else {
            $previous = $null
        }
        Start-Sleep -Milliseconds 25
    } while ($clock.ElapsedMilliseconds -lt 5000)
    throw "The keypad did not reach the expected expanded state ($Expanded) with the display visible."
}

function Save-Checkpoint(
    [IntPtr] $Window, [int] $Display, [string] $Name, [string] $ChangedFrom,
    [switch] $ObserveOnly
) {
    $candidate = Join-Path $artifactPath "$Name.actual.png"
    $expected = Join-Path $fixturePath "$Name.png"
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $previous = $null
    $lastChanged = 0
    do {
        $geometry = [RpnDesktop]::Capture($Window, $Display, $candidate, $TraceGeometry)
        $signature = (Get-FileHash -LiteralPath $candidate).Hash
        if ($signature -ne $previous) {
            $previous = $signature
            $lastChanged = $clock.ElapsedMilliseconds
        }
        if ($signature -ne $ChangedFrom -and $clock.ElapsedMilliseconds - $lastChanged -ge 200) {
            if ($TraceGeometry) {
                Add-Content -LiteralPath (Join-Path $artifactPath 'geometry.txt') -Value "$Name`: $geometry"
            }
            if (!$ObserveOnly) {
                [RpnDisplayFixture]::AssertEqual($expected, $candidate, (Join-Path $artifactPath "$Name.diff.png"))
            }
            return $signature
        }
        Start-Sleep -Milliseconds 25
    } while ($clock.ElapsedMilliseconds -lt 5000)
    throw "The display did not settle to a new image at $Name."
}

function Assert-UnchangedRows([string] $Before, [string] $After) {
    [RpnDisplayFixture]::AssertSameRows((Join-Path $artifactPath "$Before.actual.png"),
        (Join-Path $artifactPath "$After.actual.png"))
}

function Assert-ErrorFlash([IntPtr] $Window, [int] $Display) {
    $normal = Join-Path $artifactPath 'hex.actual.png'
    $flashed = Join-Path $artifactPath 'hex-error.actual.png'
    $cleared = Join-Path $artifactPath 'hex-error-cleared.actual.png'
    if ([RpnDisplayFixture]::HasErrorIndicator($normal)) {
        throw 'The error indicator was already visible before the invalid operation.'
    }

    # Selecting binary at 64 bits leaves the radix and stack unchanged.
    # Observe the paint and timer independently instead of sleeping 300 ms.
    $seen = $false
    Invoke-Menu $Window 'B' 'B'
    $clock = [Diagnostics.Stopwatch]::StartNew()
    do {
        $path = if ($seen) { $cleared } else { $flashed }
        $null = [RpnDesktop]::Capture($Window, $Display, $path, $false)
        if ([RpnDisplayFixture]::HasErrorIndicator($path)) {
            $seen = $true
        } elseif ($seen) {
            [RpnDisplayFixture]::AssertEqual($normal, $cleared,
                (Join-Path $artifactPath 'hex-error-cleared.diff.png'))
            return
        }
    } while ($clock.ElapsedMilliseconds -lt 5000)
    throw "The display error indicator did not appear and clear (appeared: $seen)."
}

function Assert-Placement([IntPtr] $Window, [int] $Display, [Drawing.Rectangle] $Before) {
    $after = [RpnDesktop]::Geometry($Window, $Display)[0]
    if ($after.Location -ne $Before.Location) {
        throw "Window moved from $($Before.Location) to $($after.Location)."
    }
}

function Close-Rpn([Diagnostics.Process] $Process, [IntPtr] $Window) {
    [RpnDesktop]::Key($Window, 0x73, 0x12)
    $clock = [Diagnostics.Stopwatch]::StartNew()
    while (!$Process.WaitForExit(25)) {
        [RpnDesktop]::CheckEscape()
        if ($clock.ElapsedMilliseconds -ge 5000) { throw 'RPN did not exit after Alt+F4.' }
    }
    if ($Process.ExitCode -ne 0) { throw "RPN exited with code $($Process.ExitCode)." }
}

function Assert-SavedState([switch] $AfterModeSwitch, [int] $ExpectedMode = 2) {
    $state = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registryPath)
    if ($null -eq $state) { throw 'The application did not save its registry state.' }
    try {
        if ($TraceGeometry) {
            $position = "saved window position: ($($state.GetValue('WindowX')), $($state.GetValue('WindowY')))"
            Add-Content -LiteralPath (Join-Path $artifactPath 'geometry.txt') -Value $position
        }
        $fields = @{
            Radix = 10
            WordSize = 8
            IntegerType = 0
            LastRadix = 8
            CalcMode = $ExpectedMode
            KeypadHidden = 1
            FloatAngleType = 2
            SignificantDigits = 10
            FloatDisplayMode = 0
        }
        foreach ($name in $fields.Keys) {
            if ($state.GetValueKind($name) -ne 'DWord' -or $state.GetValue($name) -ne $fields[$name]) {
                throw "Unexpected saved value for $name; expected DWORD $($fields[$name])."
            }
        }
        $stacks = @{
            'Binary Stack' = @(217, 3, 2, 1)
            'Float Stack' = @(10.5, 0, 0, 0)
        }
        if ($AfterModeSwitch) {
            $stacks['Binary Stack'] = @(23, 3, 2, 1)
            $stacks['Float Stack'] = @(4, 10.5, 0, 0)
        }
        foreach ($name in $stacks.Keys) {
            $bytes = $state.GetValue($name, $null)
            if ($bytes -isnot [byte[]] -or $bytes.Length -ne 32) {
                throw "Malformed saved $name; expected four eight-byte registers."
            }
            for ($register = 0; $register -lt 4; ++$register) {
                $actual = if ($name -eq 'Binary Stack') {
                    [BitConverter]::ToUInt64($bytes, 8 * $register)
                } else {
                    [BitConverter]::ToDouble($bytes, 8 * $register)
                }
                if ($actual -ne $stacks[$name][$register]) {
                    throw "Unexpected saved $name register $register`: $actual (expected $($stacks[$name][$register]))."
                }
            }
        }
    } finally {
        $state.Dispose()
    }
}

$integerDisplay = 257
$floatDisplay = 261
$checkpointCount = 12
$process = $null
$expectingEscape = $false
[RpnDesktop]::RegisterEscape()
try {
    Write-Host 'Press Esc to stop the desktop tests.'
    Write-Checkpoint 1 'mixed keyboard and mouse input in hex'
    $process = Start-Process -FilePath $Executable -PassThru
    $dialog = Wait-Dialog $process 'Rpn'
    $null = Wait-Keypad $dialog $integerDisplay $true
    # SendInput returning does not mean the dialog handled the input.
    # Start with a visible click so no keyboard Enter is pending at the handoff.
    $initial = Save-Checkpoint $dialog $integerDisplay 'hex-before-input' -ObserveOnly
    [RpnDesktop]::Click($dialog, 0x31)
    $null = Save-Checkpoint $dialog $integerDisplay 'hex-after-click' $initial -ObserveOnly
    Send-RpnKeys $dialog @(0x0D, 0x32, 0x0D, 0x33, 0x0D, 0x44, 0x35, 0x0D)
    $hex = Save-Checkpoint $dialog $integerDisplay 'hex'

    Write-Checkpoint 2 'paint and clear the 64-bit binary rejection'
    Assert-ErrorFlash $dialog $integerDisplay

    Write-Checkpoint 3 '8-bit binary display'
    Invoke-Menu $dialog 'W' '8'
    Invoke-Menu $dialog 'B' 'B'
    Send-RpnKeys $dialog @(0xDD)
    $binary = Save-Checkpoint $dialog $integerDisplay 'binary' $hex
    Write-Checkpoint 4 'octal display'
    Invoke-Menu $dialog 'B' 'O'
    Send-RpnKeys $dialog @(0xDD)
    $octal = Save-Checkpoint $dialog $integerDisplay 'octal' $binary
    Write-Checkpoint 5 'decimal display'
    Invoke-Menu $dialog 'B' 'D'
    Send-RpnKeys $dialog @(0xDD)
    $decimal = Save-Checkpoint $dialog $integerDisplay 'decimal' $octal

    Write-Checkpoint 6 'collapse the keypad and continue typing'
    $expanded = Wait-Keypad $dialog $integerDisplay $true
    Send-RpnKeys $dialog @(0x26)
    $collapsed = Wait-Keypad $dialog $integerDisplay $false
    if ($collapsed.Location -ne $expanded.Location -or $collapsed.Width -ne $expanded.Width -or
        $collapsed.Height -ge $expanded.Height) {
        throw "Unexpected keypad collapse bounds: expanded $expanded; collapsed $collapsed."
    }
    $null = Save-Checkpoint $dialog $integerDisplay 'decimal-before-collapsed-input'
    Assert-UnchangedRows 'decimal' 'decimal-before-collapsed-input'
    Send-RpnKeys $dialog @(0xDD)
    $null = Save-Checkpoint $dialog $integerDisplay 'decimal-collapsed' $decimal

    Write-Checkpoint 7 'switch to float, expand the keypad, and edit X'
    Invoke-Menu $dialog 'B' 'F'
    $floatDialog = Wait-Dialog $process 'RpnF'
    if ($floatDialog -eq $dialog) { throw 'Switching to float did not create its own dialog.' }
    $null = Wait-Keypad $floatDialog $floatDisplay $false
    Assert-Placement $floatDialog $floatDisplay $collapsed
    Send-RpnKeys $floatDialog @(0x28)
    $null = Wait-Keypad $floatDialog $floatDisplay $true
    $initial = Save-Checkpoint $floatDialog $floatDisplay 'float-before-input' -ObserveOnly
    Send-RpnKeys $floatDialog @(0x31, 0x32)
    $prefix = Save-Checkpoint $floatDialog $floatDisplay 'float-keyboard-prefix' $initial -ObserveOnly
    [RpnDesktop]::Click($floatDialog, 0x2E)
    $withDot = Save-Checkpoint $floatDialog $floatDisplay 'float-after-click' $prefix -ObserveOnly
    Send-RpnKeys $floatDialog @(0x35)
    $editing = Save-Checkpoint $floatDialog $floatDisplay 'float-entry' $withDot
    Write-Checkpoint 8 'subtract in float mode and select radians'
    Send-RpnKeys $floatDialog @(0x0D, 0x32, 0xBD)
    Invoke-Menu $floatDialog 'T' 'R'
    $null = Save-Checkpoint $floatDialog $floatDisplay 'float-radians' $editing

    Write-Checkpoint 9 'close and check the saved registry values'
    Invoke-Menu $floatDialog 'V' 'K'
    $null = Wait-Keypad $floatDialog $floatDisplay $false
    Invoke-Menu $floatDialog 'V' 'K'
    $null = Wait-Keypad $floatDialog $floatDisplay $true
    Invoke-Menu $floatDialog 'V' 'K'
    $floatBounds = Wait-Keypad $floatDialog $floatDisplay $false
    $null = Save-Checkpoint $floatDialog $floatDisplay 'float-before-restart'
    Assert-UnchangedRows 'float-radians' 'float-before-restart'
    $beforeRestart = 'float-before-restart'
    if ($VerifyPlacement) {
        [RpnDesktop]::MoveForPlacementCheck($floatDialog)
        $floatBounds = Wait-Keypad $floatDialog $floatDisplay $false
        if ($floatBounds.X -le 0 -or $floatBounds.Y -le 0) {
            throw "Expected a nonzero test window position, got $floatBounds."
        }
        $beforeRestart = 'float-moved-before-restart'
        $null = Save-Checkpoint $floatDialog $floatDisplay $beforeRestart -ObserveOnly
        Invoke-Menu $floatDialog 'B' 'D'
        $dialog = Wait-Dialog $process 'Rpn'
        $null = Wait-Keypad $dialog $integerDisplay $false
        Assert-Placement $dialog $integerDisplay $floatBounds
        Invoke-Menu $dialog 'B' 'F'
        $floatDialog = Wait-Dialog $process 'RpnF'
        $null = Wait-Keypad $floatDialog $floatDisplay $false
        Assert-Placement $floatDialog $floatDisplay $floatBounds
    }
    Close-Rpn $process $floatDialog
    Assert-SavedState
    $process.Dispose()
    $process = $null

    # A previous save can contain a mode with no formatter. The next
    # launch must still draw the same float rows and save Auto on exit.
    $state = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registryPath, $true)
    if ($null -eq $state) { throw 'Cannot open the saved test state to set a display override.' }
    try {
        $state.SetValue('FloatDisplayMode', 2, [Microsoft.Win32.RegistryValueKind]::DWord)
    } finally {
        $state.Dispose()
    }

    Write-Checkpoint 10 'relaunch and verify both calculator modes'
    $process = Start-Process -FilePath $Executable -PassThru
    $floatDialog = Wait-Dialog $process 'RpnF'
    $null = Wait-Keypad $floatDialog $floatDisplay $false
    Assert-Placement $floatDialog $floatDisplay $floatBounds
    if ($VerifyEscape) {
        $expectingEscape = $true
        [RpnDesktop]::Key($floatDialog, 0x1B, 0)
        $clock = [Diagnostics.Stopwatch]::StartNew()
        do {
            [RpnDesktop]::CheckEscape()
            Start-Sleep -Milliseconds 25
        } while ($clock.ElapsedMilliseconds -lt 5000)
        throw 'Sending Esc did not stop the desktop pass.'
    }
    $null = Save-Checkpoint $floatDialog $floatDisplay 'float-restarted' -ObserveOnly
    Assert-UnchangedRows $beforeRestart 'float-restarted'
    Invoke-Menu $floatDialog 'B' 'D'
    $dialog = Wait-Dialog $process 'Rpn'
    $null = Wait-Keypad $dialog $integerDisplay $false
    Assert-Placement $dialog $integerDisplay $floatBounds
    $null = Save-Checkpoint $dialog $integerDisplay 'decimal-restarted' -ObserveOnly
    Assert-UnchangedRows 'decimal-collapsed' 'decimal-restarted'
    Write-Checkpoint 11 'check Left, Backspace, Delete, and mode switches during entry'
    # Shorten integer X to 2. Starting float entry then switching back leaves
    # entry active, so typing 3 appends to integer X rather than lifting it.
    Send-RpnKeys $dialog @(0x25, 0x08)
    Invoke-Menu $dialog 'B' 'F'
    $floatDialog = Wait-Dialog $process 'RpnF'
    Send-RpnKeys $floatDialog @(0x34)
    Invoke-Menu $floatDialog 'B' 'D'
    $dialog = Wait-Dialog $process 'Rpn'
    Send-RpnKeys $dialog @(0x33)
    Invoke-Menu $dialog 'B' 'F'
    $floatDialog = Wait-Dialog $process 'RpnF'
    # Commit the carried entry, drop it with Delete, then enter it again.
    # If Delete is ignored the duplicate 4 remains in Y instead of 10.5.
    Send-RpnKeys $floatDialog @(0x0D, 0x2E, 0x34, 0x0D)
    Close-Rpn $process $floatDialog
    Assert-SavedState -AfterModeSwitch
    Write-Checkpoint 12 'recover a bad saved calculator mode without discarding the stacks'
    $process.Dispose()
    $process = $null
    $state = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($registryPath, $true)
    if ($null -eq $state) { throw 'Cannot open the saved test state for the recovery check.' }
    try {
        $state.SetValue('CalcMode', 99, [Microsoft.Win32.RegistryValueKind]::DWord)
    } finally {
        $state.Dispose()
    }
    $process = Start-Process -FilePath $Executable -PassThru
    $dialog = Wait-Dialog $process 'Rpn'
    $null = Wait-Keypad $dialog $integerDisplay $false
    Close-Rpn $process $dialog
    Assert-SavedState -AfterModeSwitch -ExpectedMode 1
} catch {
    if (!$expectingEscape -or $_.Exception.GetBaseException() -isnot [OperationCanceledException]) {
        throw
    }
} finally {
    try {
        if ($null -ne $process) {
            try {
                if (!$process.HasExited) {
                    try {
                        Stop-Process -Id $process.Id
                    } catch {
                        if (!$process.HasExited) { throw }
                    }
                    if (!$process.WaitForExit(5000)) { throw 'The test calculator did not stop.' }
                }
            } finally {
                $process.Dispose()
            }
        }
        [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($registryPath, $false)
    } finally {
        [RpnDesktop]::ReleaseEscape()
    }
}
if ($VerifyEscape) {
    Write-Output 'Esc stopped the desktop pass; the test process and key were cleaned up.'
    return
}
Write-Output "RPN desktop checks passed. Artifacts: $artifactPath"
