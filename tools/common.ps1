$ErrorActionPreference = 'Stop'
# MSBuild rejects duplicate environment keys from this desktop host.
$VoidfrontToolPath = $env:PATH
Remove-Item Env:PATH -ErrorAction SilentlyContinue
Remove-Item Env:Path -ErrorAction SilentlyContinue
$env:Path = $VoidfrontToolPath
$Repo = Split-Path $PSScriptRoot -Parent
$Toolchain = Get-Content (Join-Path $PSScriptRoot 'toolchain.json') -Raw | ConvertFrom-Json
function Assert-RunningAllowed {
    if (Test-Path (Join-Path $Repo '.voidfront-agent/STOP')) { throw 'Voidfront STOP switch is present.' }
}
function Assert-Godot {
    $actual = (& $Toolchain.godot_console --version | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $actual.StartsWith($Toolchain.godot_version + '.')) { throw "Pinned Godot version mismatch: $actual" }
    $templates = Join-Path $env:APPDATA "Godot/export_templates/$($Toolchain.templates)/version.txt"
    if (-not (Test-Path $templates) -or (Get-Content $templates -Raw).Trim() -ne $Toolchain.templates) { throw 'Missing or mismatched export templates.' }
    if ((Get-FileHash $Toolchain.godot_editor).Hash -ne $Toolchain.godot_editor_sha256) { throw 'Pinned Godot executable hash mismatch.' }
    $releaseTemplate = Join-Path (Split-Path $templates) 'windows_release_x86_64.exe'
    if ((Get-FileHash $releaseTemplate).Hash -ne $Toolchain.windows_release_template_sha256) { throw 'Pinned Windows template hash mismatch.' }
    Write-Output "Godot verified: $actual"
}
function Assert-NativeExit([string]$Operation) {
    if ($LASTEXITCODE -ne 0) { throw "$Operation failed with exit code $LASTEXITCODE" }
}
function Invoke-ProjectGodot([string[]]$GodotArguments) {
    # Isolate automation settings/logs from the user's editor profile.
    $originalAppData = $env:APPDATA
    $profile = Join-Path $Repo 'artifacts/godot-profile'
    $templateTarget = Join-Path $profile "Godot/export_templates/$($Toolchain.templates)"
    New-Item -ItemType Directory -Force $templateTarget | Out-Null
    $templateSource = Join-Path $originalAppData "Godot/export_templates/$($Toolchain.templates)"
    foreach ($name in @('version.txt','windows_debug_x86_64.exe','windows_debug_x86_64_console.exe','windows_release_x86_64.exe','windows_release_x86_64_console.exe')) {
        $source = Join-Path $templateSource $name
        $destination = Join-Path $templateTarget $name
        if (-not (Test-Path $destination)) { Copy-Item -LiteralPath $source -Destination $destination }
        if ((Get-FileHash $destination).Hash -ne (Get-FileHash $source).Hash) { throw "Isolated template cache differs from pinned installation: $name" }
    }
    try {
        $env:APPDATA = $profile
        $output = & $Toolchain.godot_console @GodotArguments 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Output $_ }
        if ($exitCode -ne 0) { throw "Godot failed with exit code $exitCode" }
        # Godot can return zero despite parse/import errors. Fail on these too.
        $errors = $output | Where-Object { "$_" -match '^(SCRIPT ERROR:|ERROR:)' -and "$_" -notmatch '^ERROR: Failed to read the root certificate store\.' }
        if ($errors) { throw "Godot reported errors: $($errors -join ' | ')" }
    } finally { $env:APPDATA = $originalAppData }
}
