#Requires -Version 7.0
param(
    [switch]$Packaged,
    [switch]$Visible,
    [switch]$Capture,
    [string]$PackageDirectory='',
    [ValidateSet('Scale128','Foundry')][string]$Map='Scale128',
    [ValidateRange(1,250)][int]$UnitsPerTeam=100,
    [ValidateRange(1,3600)][int]$Frames=600,
    [ValidateRange(0,1200)][int]$Warmup=120,
    [ValidateSet('ordinary','pose-refresh','pose-frozen')][string]$Controlled='ordinary',
    [ValidateRange(0,1000)][int]$Tick=40,
    [ValidateSet('standard','overview')][string]$Camera='standard',
    [ValidateRange(30,1800)][int]$TimeoutSeconds=300,
    [string]$Name='presentation-profile'
)
. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
Assert-Godot
if ($Controlled -eq 'ordinary' -and $Camera -ne 'standard') { throw 'Overview camera requires controlled profiling.' }
if ($Name -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Profile name must contain only letters, digits, underscores and hyphens.' }
$out = Join-Path $Repo 'artifacts'
New-Item -ItemType Directory -Force $out | Out-Null
$reportPath = Join-Path $out "$Name.json"
$hostPath = Join-Path $out "$Name-host.json"
# Do not overwrite an earlier observation or mistake a stale report for success.
foreach ($suffix in @('.json','-host.json','-engine.log','-stdout.log','-stderr.log','.png')) {
    if (Test-Path (Join-Path $out "$Name$suffix")) { throw "Profile artifacts already exist: $Name$suffix; choose a new -Name." }
}
if ([string]::IsNullOrEmpty($PackageDirectory)) { $PackageDirectory = "$Repo/artifacts/package" }
if (-not $Packaged -and $PSBoundParameters.ContainsKey('PackageDirectory')) { throw '-PackageDirectory requires -Packaged.' }
$executable = if ($Packaged) { Join-Path $PackageDirectory 'Voidfront.exe' } else { $Toolchain.godot_console }
if (-not (Test-Path -LiteralPath $executable)) { throw "Missing executable: $executable" }
$arguments = @('--log-file',"$out/$Name-engine.log",'--resolution','1920x1080')
if (-not $Packaged) { $arguments += @('--path',"$Repo/client") }
$arguments += @('--','--profile-presentation',"--profile-frames=$Frames","--profile-warmup=$Warmup","--report=$reportPath")
if ($Controlled -ne 'ordinary') { $arguments += @("--profile-controlled=$Controlled", "--profile-tick=$Tick", "--profile-camera=$Camera") }
if ($Map -eq 'Scale128') { $arguments += @('--scale128',"--units-per-team=$UnitsPerTeam") }
if ($Capture) { $arguments += "--capture=$out/$Name.png" }
# Hash before launch: a later edit or rebuild must not be attributed to this run.
# For packages, the PCK/DLL are the actual payload; source hashes are context only.
$fingerprintPaths = @(
    $executable, "$Repo/client/main.gd", "$Repo/client/hud.gd",
    "$Repo/client/presentation_profile.gd", "$PSScriptRoot/profile.ps1",
    "$Repo/client/project.godot", "$Repo/client/voidfront.gdextension",
    "$PSScriptRoot/toolchain.json"
)
if ($Packaged) {
    $fingerprintPaths += @((Join-Path $PackageDirectory 'Voidfront.pck'), (Join-Path $PackageDirectory 'voidfront_bridge.dll'))
} else {
    $fingerprintPaths += @($Toolchain.godot_editor, "$Repo/client/bin/Debug/voidfront_bridge.dll")
}
$fingerprints = foreach ($fingerprintPath in $fingerprintPaths) {
    $fingerprintFile = Get-Item -LiteralPath $fingerprintPath
    @{ path=$fingerprintFile.FullName; bytes=$fingerprintFile.Length
       modified_utc=$fingerprintFile.LastWriteTimeUtc.ToString('o')
       sha256=(Get-FileHash -LiteralPath $fingerprintFile.FullName).Hash }
}
$executableSha256 = $fingerprints[0].sha256
$originalAppData = $env:APPDATA
$process = $null
$timer = [Diagnostics.Stopwatch]::new()
$hostSamples = [Collections.Generic.List[object]]::new()
$peakResident = 0L
$failure = $null
$exitCode = $null
$runtimeProcessId = $null
$hostCountersValid = $false
$launchUtc = [DateTime]::UtcNow.ToString('o')
try {
    $env:APPDATA = Join-Path $out 'godot-profile'
    $launch = @{
        FilePath=$executable; ArgumentList=$arguments; PassThru=$true
        RedirectStandardOutput="$out/$Name-stdout.log"; RedirectStandardError="$out/$Name-stderr.log"
        WindowStyle='Hidden'
    }
    # Explicit user-operated foreground option; focus is still measured, never assumed.
    if ($Visible) { $launch.WindowStyle = 'Normal' }
    $timer.Start()
    $process = Start-Process @launch
    Write-Output "Profile process $($process.Id); $Map; visible=$([bool]$Visible); warmup=$Warmup; measured=$Frames"
    while (-not $process.HasExited) {
        $process.Refresh()
        if (-not $process.HasExited) {
            $peakResident = [Math]::Max($peakResident, $process.PeakWorkingSet64)
            $hostSamples.Add(@{
                process_id=$process.Id
                elapsed_seconds=$timer.Elapsed.TotalSeconds
                working_set_bytes=$process.WorkingSet64
                peak_working_set_bytes=$process.PeakWorkingSet64
                private_bytes=$process.PrivateMemorySize64
                cpu_seconds=$process.TotalProcessorTime.TotalSeconds
            })
        }
        if ($timer.Elapsed.TotalSeconds -gt $TimeoutSeconds) {
            $process.Kill($true)
            $process.WaitForExit()
            throw 'Owned profile process exceeded its bounded watchdog.'
        }
        if ($process.WaitForExit(500)) { break }
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    if ($exitCode -ne 0) { throw "Presentation profile failed ($exitCode); inspect artifacts/$Name logs." }
    $errors = Get-Content "$out/$Name-stderr.log" | Where-Object { $_ -match '^(SCRIPT ERROR:|ERROR:)' -and $_ -notmatch '^ERROR: Failed to read the root certificate store\.' }
    if ($errors) { throw "Runtime reported errors: $($errors -join ' | ')" }
    if (-not (Test-Path -LiteralPath $reportPath)) { throw 'Runtime exited without a profile report.' }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    $runtimeProcessId = $report.process_id
    $hostCountersValid = $null -ne $runtimeProcessId -and $runtimeProcessId -eq $process.Id -and $hostSamples.Count -gt 0
    if (-not $hostCountersValid) { Write-Warning 'Sampled PID differs from runtime PID or has no samples. Host counters describe the launcher only; no game resident-memory or CPU claim is supported.' }
    $expectedMode = if ($Controlled -eq 'ordinary') { 'ordinary_offline_presentation' } else { 'controlled_pose_resubmission' }
    if (-not $report.ok -or $report.mode -ne $expectedMode) { throw 'Runtime profile assertions failed.' }
    if (@($report.measured_samples).Count -ne $Frames -or @($report.warmup_samples).Count -ne $Warmup) { throw 'Incomplete raw profile samples.' }
    $requiredStages = if ($Controlled -eq 'ordinary') { @('bridge','snapshot','present','hud_process','main_process') } else { @('animation_pose_update','hud_process','main_process') }
    foreach ($stage in $requiredStages) {
        if ($report.measurement_summary.stage_calls_usec.$stage.count -le 0) { throw "Missing measured stage calls: $stage" }
    }
    if ($Controlled -eq 'ordinary') {
        if ($report.final_snapshot.tick -le $report.measurement_initial_snapshot.tick) { throw 'Measured simulation did not advance.' }
    } elseif ($report.controlled.variant -ne $Controlled -or $report.final_snapshot.tick -ne $Tick -or $report.initial_snapshot.hash -ne $report.final_snapshot.hash) {
        throw 'Controlled profile state or requested variant differs.'
    }
    if ($Capture -and -not (Test-Path "$out/$Name.png")) { throw 'Missing post-measurement screenshot.' }
    if (-not $report.foreground_entire_measurement) { Write-Warning 'Some measured frames were unfocused; this run cannot be described as continuous foreground evidence.' }
    Write-Output ($report.measurement_summary | ConvertTo-Json -Depth 6)
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    if ($null -ne $process -and -not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
    $timer.Stop()
    $env:APPDATA = $originalAppData
    @{
        launch_utc=$launchUtc; wall_seconds=$timer.Elapsed.TotalSeconds
        peak_resident_bytes=$(if ($hostCountersValid) { $peakResident } else { $null })
        sampled_process_peak_resident_bytes=$peakResident; samples=$hostSamples.ToArray()
        host_counters_valid_for_game=$hostCountersValid; runtime_process_id=$runtimeProcessId
        host_counter_scope=$(if ($hostCountersValid) { 'runtime_game_process' } else { 'launcher_only_or_unverified' })
        visible_requested=[bool]$Visible; packaged=[bool]$Packaged; screenshot_after_measurement=[bool]$Capture
        map=$Map; requested_units_per_team=$(if ($Map -eq 'Foundry') { 6 } else { $UnitsPerTeam })
        controlled=$Controlled; controlled_tick=$(if ($Controlled -eq 'ordinary') { $null } else { $Tick }); controlled_camera=$Camera
        process_id=$(if ($null -ne $process) { $process.Id } else { $null }); exit_code=$exitCode; failure=$failure
        executable=$executable; executable_sha256=$executableSha256; prelaunch_fingerprints=@($fingerprints)
        fingerprint_note='Collected before process launch. Packaged PCK and DLL identify loaded payload; source fingerprints are checkout context and do not prove package-source equality.'
        cpu=$env:PROCESSOR_IDENTIFIER; logical_processors=[Environment]::ProcessorCount
        windows=[Environment]::OSVersion.VersionString
        source='Windows launched-process counters sampled every 500 ms. Valid for game only when runtime-reported process_id equals sampled PID. CPU is cumulative process CPU, not frame CPU; host window includes startup and final output.'
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $hostPath
}
