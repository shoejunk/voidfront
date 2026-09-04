param([switch]$Packaged)
. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
Assert-Godot
if ($Packaged) { & "$Repo/artifacts/package/Voidfront.exe" } else { & $Toolchain.godot_console --path "$Repo/client" }
Assert-NativeExit 'Voidfront runtime'
