param([switch]$Network)
. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
$evidence = Join-Path $Repo 'artifacts/determinism'
foreach ($configuration in @('Debug','Release')) {
    & "$PSScriptRoot/build.ps1" -Configuration $configuration
    & "$Repo/sim/verify_replay.ps1" -Executable "$Repo/build/windows/sim/$configuration/voidfront_headless.exe" -OutputDirectory "$evidence/$configuration"
    python "$PSScriptRoot/verify_output_aliases.py" --executable "$Repo/build/windows/sim/$configuration/voidfront_headless.exe"
    Assert-NativeExit 'Replay/output alias preservation'
}
python "$PSScriptRoot/compare_traces.py" "$evidence/Debug/record.trace" "$evidence/Release/record.trace"
Assert-NativeExit 'Cross-configuration trace comparison'
for ($run = 1; $run -le 10; $run++) {
    & "$Repo/build/windows/sim/Release/voidfront_headless.exe" --replay "$evidence/Release/record.vfr" --trace "$evidence/repeat-$run.trace"
    Assert-NativeExit 'Repeated replay'
    python "$PSScriptRoot/compare_traces.py" "$evidence/Release/record.trace" "$evidence/repeat-$run.trace"
    Assert-NativeExit 'Repeated trace comparison'
}
if ($Network) {
    python "$PSScriptRoot/verify_network.py"
    Assert-NativeExit 'Separate-process lockstep and impairment suite'
}
