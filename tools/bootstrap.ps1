. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
Assert-Godot
$dependency = Join-Path $Repo 'third_party/godot-cpp'
if (-not (Test-Path "$dependency/.git")) {
    git clone --no-checkout $Toolchain.godot_cpp_repository $dependency
    Assert-NativeExit 'Fetch godot-cpp'
    git -C $dependency checkout --detach $Toolchain.godot_cpp_revision
    Assert-NativeExit 'Pin godot-cpp'
}
$revision = (git -C $dependency rev-parse HEAD).Trim()
Assert-NativeExit 'Read godot-cpp revision'
if ($revision -ne $Toolchain.godot_cpp_revision) { throw "Dependency revision mismatch: $revision; preserve changes and restore the recorded revision explicitly." }
if (git -C $dependency status --porcelain) { throw 'godot-cpp has local modifications.' }
Write-Output "godot-cpp verified: $revision / API $($Toolchain.godot_cpp_api)"
