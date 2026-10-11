param([string]$Out='artifacts/net-match-2026-10-11',[string]$PackagePath='artifacts/package/Voidfront.exe',[ValidateRange(1000,12000)][int]$Ticks=12000,[ValidateRange(1,999999)][int]$Seed=7,[ValidateRange(1,16)][int]$Delay=2)
. "$PSScriptRoot/common.ps1"
Assert-RunningAllowed
Assert-Godot
$evidencePath = [System.IO.Path]::GetFullPath((Join-Path $Repo $Out))
if (Test-Path -LiteralPath $evidencePath) { throw 'Evidence directory already exists; select a fresh path.' }
New-Item -ItemType Directory -Path $evidencePath | Out-Null
$package = [System.IO.Path]::GetFullPath((Join-Path $Repo $PackagePath))
$reservations = @()
$children = @()
$peak = @(0L,0L)
$sessionCode = Get-Random -Minimum 1 -Maximum 2147483647
$clock = [Diagnostics.Stopwatch]::StartNew()
$originalAppData = $env:APPDATA
try {
    for ($player=0; $player -lt 2; $player++) {
        $socket = New-Object System.Net.Sockets.UdpClient
        $socket.Client.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Loopback, 0)))
        $reservations += $socket
    }
    $ports = @($reservations | ForEach-Object { $_.Client.LocalEndPoint.Port })
    foreach ($socket in $reservations) { $socket.Close() }
    $env:APPDATA = Join-Path $Repo 'artifacts/godot-profile'
    for ($player=0; $player -lt 2; $player++) {
        $prefix = Join-Path $evidencePath "p$player"
        $arguments = @('--log-file',"$prefix-engine.log",'--resolution','1280x720','--','--network-economy-smoke',"--ticks=$Ticks","--seed=$Seed","--delay=$Delay","--session=$sessionCode","--player=$player","--port=$($ports[$player])","--remote-port=$($ports[1-$player])","--capture=$prefix.png","--report=$prefix.json")
        $child = Start-Process -FilePath $package -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput "$prefix.out" -RedirectStandardError "$prefix.err"
        $null = $child.Handle
        $children += $child
        Write-Output "Player $player process $($child.Id), port $($ports[$player])"
    }
    while (@($children | Where-Object { -not $_.HasExited }).Count -gt 0) {
        Assert-RunningAllowed
        for ($player=0; $player -lt 2; $player++) {
            $children[$player].Refresh()
            if (-not $children[$player].HasExited) { $peak[$player] = [Math]::Max($peak[$player],$children[$player].PeakWorkingSet64) }
        }
        if ($clock.Elapsed.TotalSeconds -gt ($Ticks / 20.0 + 180)) { throw 'Owned paired match exceeded watchdog; evidence retained.' }
        Start-Sleep -Milliseconds 500
    }
    foreach ($child in $children) { $child.WaitForExit() }
    $hostReport = @{ utc=[DateTime]::UtcNow.ToString('o'); wall_seconds=$clock.Elapsed.TotalSeconds; pids=@($children | ForEach-Object { $_.Id }); ports=$ports; session=$sessionCode; seed=$Seed; delay=$Delay; ticks=$Ticks; peak_resident_bytes=$peak; package_path=$package; package_sha256=(Get-FileHash $package).Hash; pck_sha256=(Get-FileHash ([System.IO.Path]::ChangeExtension($package,'pck'))).Hash; bridge_sha256=(Get-FileHash (Join-Path (Split-Path $package) 'voidfront_bridge.dll')).Hash; note='Two real rendered processes, loopback only. Process observations do not establish performance or human-play acceptance.' }
    [System.IO.File]::WriteAllText((Join-Path $evidencePath 'host.json'),($hostReport | ConvertTo-Json -Depth 5),[System.Text.UTF8Encoding]::new($false))
    for ($player=0; $player -lt 2; $player++) {
        if ($children[$player].ExitCode -ne 0) { throw "Player $player exited $($children[$player].ExitCode); inspect retained logs." }
        $report = Get-Content (Join-Path $evidencePath "p$player.json") -Raw | ConvertFrom-Json
        if (-not $report.ok) { throw "Player $player reported a failed match fixture." }
        $engineErrors = Get-Content (Join-Path $evidencePath "p$player.err") | Where-Object { $_ -match '^(SCRIPT ERROR:|ERROR:)' -and $_ -notmatch '^ERROR: Failed to read the root certificate store\.' }
        if ($engineErrors) { throw "Player $player engine error: $($engineErrors -join ' | ')" }
        Write-Output "Player ${player}: winner $($report.winner), final tick $($report.final_tick), checks $($report.fixture.checks.Count)"
    }
} finally {
    foreach ($socket in $reservations) { $socket.Close() }
    foreach ($child in $children) { if (-not $child.HasExited) { $child.Kill(); $child.WaitForExit() } }
    $env:APPDATA = $originalAppData
}
