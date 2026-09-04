param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$SimOnly)
. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
Assert-Godot
$build = Join-Path $Repo $(if ($SimOnly) { 'build/headless' } else { 'build/windows' })
if (-not $SimOnly) { & "$PSScriptRoot/bootstrap.ps1" }
$client = if ($SimOnly) { 'OFF' } else { 'ON' }
cmake -S $Repo -B $build -G $Toolchain.cmake_generator -A x64 "-DVOIDFRONT_BUILD_CLIENT=$client"
Assert-NativeExit 'CMake configure'
cmake --build $build --config $Configuration --parallel 1 -- /nodeReuse:false
Assert-NativeExit 'MSVC build'
ctest --test-dir $build -C $Configuration --output-on-failure
Assert-NativeExit 'Simulation tests'
