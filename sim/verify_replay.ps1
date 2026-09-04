param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
if (Test-Path (Join-Path (Split-Path $PSScriptRoot) '.voidfront-agent/STOP')) { throw 'Voidfront STOP switch is present.' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$recordPath = Join-Path $OutputDirectory 'record.vfr'
$recordTrace = Join-Path $OutputDirectory 'record.trace'
$replayTrace = Join-Path $OutputDirectory 'replay.trace'
& $Executable --ticks 2000 --seed 42 --record $recordPath --trace $recordTrace
if ($LASTEXITCODE -ne 0) { throw 'Recording process failed' }
& $Executable --replay $recordPath --trace $replayTrace
if ($LASTEXITCODE -ne 0) { throw 'Replay process failed' }
if ((Get-FileHash $recordTrace).Hash -ne (Get-FileHash $replayTrace).Hash) { throw 'Record and replay traces differ' }
[byte[]]$golden = [IO.File]::ReadAllBytes($recordPath)
$rejected = 0
function Reject-Replay([string]$Name, [byte[]]$Bytes) {
    $invalidPath = Join-Path $OutputDirectory "$Name.vfr"
    [IO.File]::WriteAllBytes($invalidPath, $Bytes)
    $diagnostic = & $Executable --replay $invalidPath 2>&1
    if ($LASTEXITCODE -eq 0) { throw "Malformed replay accepted: $Name" }
    $script:rejected++
    Write-Output "$Name rejected: $diagnostic"
}
foreach ($length in @(0, 3, 20, 23, 24, ($golden.Length - 1))) {
    $prefix = New-Object byte[] $length
    [Array]::Copy($golden, $prefix, $length)
    Reject-Replay "truncated-$length" $prefix
}
foreach ($mutation in @(
    @{ Name='container-version'; Offset=3; Value=2 },
    @{ Name='protocol-version'; Offset=4; Value=2 },
    @{ Name='zero-team-size'; Offset=12; Value=0 },
    @{ Name='zero-ticks'; Offset=16; Value=0 },
    @{ Name='frame-limit'; Offset=20; Value=100001 },
    @{ Name='frame-length'; Offset=24; Value=4294967295 },
    @{ Name='command-after-end'; Offset=32; Value=2000 },
    @{ Name='foreign-unit'; Offset=74; Value=7 }
)) {
    [byte[]]$bad = $golden.Clone()
    [byte[]]$value = [BitConverter]::GetBytes([uint32]$mutation.Value)
    [Array]::Copy($value, 0, $bad, $mutation.Offset, 4)
    Reject-Replay $mutation.Name $bad
}
Reject-Replay 'trailing-data' ([byte[]]($golden + 0))
# A valid replay with no frames must leave both teams idle: no generated AI.
[byte[]]$empty = $golden[0..23]
[Array]::Clear($empty, 20, 4)
$emptyPath = Join-Path $OutputDirectory 'empty-input.vfr'
$emptyTrace = Join-Path $OutputDirectory 'empty-input.trace'
[IO.File]::WriteAllBytes($emptyPath, $empty)
& $Executable --replay $emptyPath --trace $emptyTrace
if ($LASTEXITCODE -ne 0) { throw 'Valid zero-command replay failed' }
if ((Get-Content $emptyTrace -TotalCount 1) -eq (Get-Content $recordTrace -TotalCount 1)) { throw 'Replay unexpectedly generated AI inputs' }
Write-Output "PASS: separate record/replay traces match; $rejected malformed replays rejected; empty replay generates no AI"
