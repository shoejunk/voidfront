param([switch]$Packaged,[switch]$Movie,[switch]$Movement,[switch]$Crowd,[int]$Ticks=400,[string]$Name='runtime')
. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
Assert-Godot
if ($Movement -and ($Ticks -lt 1 -or $Ticks -gt 600)) { throw 'Movement capture requires 1..600 ticks.' }
if ($Crowd -and ($Movement -or $Ticks -lt 1 -or $Ticks -gt 600)) { throw 'Crowd capture requires its own 1..600 tick fixture.' }
if ($Name -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Capture name must contain only letters, digits, underscores and hyphens.' }
$out = Join-Path $Repo 'artifacts'
New-Item -ItemType Directory -Force $out | Out-Null
$executable = if ($Packaged) { "$Repo/artifacts/package/Voidfront.exe" } else { $Toolchain.godot_console }
$arguments = @('--log-file',"$out/$Name-engine.log",'--resolution','1920x1080')
if (-not $Packaged) { $arguments += @('--path',"$Repo/client") }
if ($Movie) { $arguments += @('--write-movie',"$out/$Name.avi",'--fixed-fps','60') }
$smokeOption = if ($Movement) { '--movement-smoke' } elseif ($Crowd) { '--crowd-smoke' } else { '--smoke' }
$arguments += @('--',$smokeOption,"--ticks=$Ticks","--capture=$out/$Name.png","--report=$out/$Name.json")
$originalAppData = $env:APPDATA
if (Test-Path "$out/$Name.json") { Remove-Item -LiteralPath "$out/$Name.json" }
try {
    $env:APPDATA = Join-Path $out 'godot-profile'
    $process = Start-Process -FilePath $executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$out/$Name-stdout.log" -RedirectStandardError "$out/$Name-stderr.log"
    Write-Output "Capture process: $($process.Id)"
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $peakResident = 0L
    while (-not $process.WaitForExit(1000)) {
        $process.Refresh()
        $peakResident = [Math]::Max($peakResident, $process.PeakWorkingSet64)
        if ($timer.Elapsed.TotalSeconds -gt 180) {
            $process.Kill()
            throw 'Owned capture process exceeded the 180-second smoke watchdog.'
        }
    }
    @{ wall_seconds=$timer.Elapsed.TotalSeconds; peak_resident_bytes=$peakResident; movie=[bool]$Movie; source='Windows process PeakWorkingSet64 sampled once per second'; } | ConvertTo-Json | Set-Content "$out/$Name-host.json"
    if ($process.ExitCode -ne 0) { throw "Runtime smoke failed ($($process.ExitCode)); inspect artifacts/$Name logs." }
    $errors = Get-Content "$out/$Name-stderr.log" | Where-Object { $_ -match '^(SCRIPT ERROR:|ERROR:)' -and $_ -notmatch '^ERROR: Failed to read the root certificate store\.' }
    if ($errors) { throw "Runtime reported errors: $($errors -join ' | ')" }
    if (-not (Test-Path "$out/$Name.json")) { throw 'Runtime exited without smoke report.' }
    $report = Get-Content "$out/$Name.json" -Raw | ConvertFrom-Json
    if (-not $report.ok) { throw 'Runtime smoke assertions failed.' }
    Write-Output (Get-Content "$out/$Name.json" -Raw)
} finally { $env:APPDATA = $originalAppData }
