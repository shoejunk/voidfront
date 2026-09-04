$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (Test-Path (Join-Path $repo '.voidfront-agent/STOP')) { throw 'Voidfront STOP switch is present.' }
$blender = 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
if (!(Test-Path -LiteralPath $blender)) { throw "Required Blender missing: $blender" }
& $blender --background --factory-startup --python-exit-code 2 --python (Join-Path $repo 'art/generate_walker.py')
if ($LASTEXITCODE -ne 0) { throw 'Blender asset generation failed' }
& $blender --background --factory-startup --python-exit-code 2 --python (Join-Path $repo 'art/validate_walker.py')
if ($LASTEXITCODE -ne 0) { throw 'GLB validation failed' }
