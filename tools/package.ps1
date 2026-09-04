. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
& "$PSScriptRoot/build.ps1" -Configuration Release
$out = Join-Path $Repo 'artifacts/package'
New-Item -ItemType Directory -Force $out | Out-Null
Invoke-ProjectGodot @('--headless','--path',"$Repo/client",'--editor','--import')
Invoke-ProjectGodot @('--headless','--path',"$Repo/client",'--export-release','Windows Desktop',"$out/Voidfront.exe")
Copy-Item "$Repo/PLAY.md" $out
Copy-Item "$Repo/third_party/GODOT_CPP_LICENSE.md" $out
Write-Output "Package: $out/Voidfront.exe"
