[CmdletBinding()]
param([double]$Duration = 15, [string]$Plant = '', [switch]$Interactive)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe = Join-Path $root 'var/build/stalthos-runtime/Release/Stalthos.exe'
if (-not (Test-Path $exe)) { throw 'Runtime executable absent; run Build.ps1' }
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$output = Join-Path $root "var/reports/runtime/$id"
New-Item -ItemType Directory -Force $output | Out-Null
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $exe
$start.WorkingDirectory = Split-Path $exe
$start.UseShellExecute = $false
$start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
foreach ($arg in @('dx12','alwaysactive','--output',$output,'--duration',$Duration.ToString([Globalization.CultureInfo]::InvariantCulture))) { $start.ArgumentList.Add($arg) }
if ($Plant) { $start.ArgumentList.Add('--plant'); $start.ArgumentList.Add((Resolve-Path $Plant).Path) }
if ($Interactive) { $start.ArgumentList.Add('--interactive') }
$process = [Diagnostics.Process]::Start($start)
[ordered]@{process_id=$process.Id;executable=$exe;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant();evidence=$output;interactive=[bool]$Interactive;requested_seconds=$Duration} | ConvertTo-Json | Set-Content (Join-Path $output 'launch.json') -Encoding utf8
Write-Output "Process $($process.Id); evidence $output"
if (-not $Interactive) {
    if (-not $process.WaitForExit([int](($Duration+60)*1000))) { $process.Kill(); throw "Bounded runtime did not exit; evidence retained at $output" }
    if ($process.ExitCode -ne 0) { throw "Runtime exited $($process.ExitCode); evidence retained at $output" }
    Get-Content (Join-Path $output 'runtime-receipt.json') -Raw
}
