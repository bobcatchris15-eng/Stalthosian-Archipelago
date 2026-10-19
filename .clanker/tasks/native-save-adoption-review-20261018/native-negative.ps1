param([Parameter(Mandatory)][string]$Executable,[Parameter(Mandatory)][string]$SourceSave)
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
& (Join-Path $taskRoot 'mutate.exe') $SourceSave $taskRoot
if($LASTEXITCODE -ne 0){throw 'Fixture mutation failed'}
$exeHash=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash
$sourceHash=(Get-FileHash -LiteralPath $SourceSave -Algorithm SHA256).Hash
$target=Join-Path $taskRoot 'preserved-destination.stl'
Copy-Item -LiteralPath $SourceSave -Destination $target -Force
$targetHash=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
$results=@()
foreach($kind in @('checksum','configuration','payload-binding','anchor','habitat')){
 $report=Join-Path $taskRoot ('native-invalid-'+$kind+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
 New-Item -ItemType Directory -Path $report | Out-Null
 $start=[Diagnostics.ProcessStartInfo]::new();$start.FileName=$Executable;$start.WorkingDirectory=Split-Path $Executable
 $start.UseShellExecute=$false;$start.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
 foreach($arg in @('dx12','alwaysactive','--populated-world','--output',$report,'--duration','2','--load-state',(Join-Path $taskRoot ('bad-'+$kind+'.stl')),'--save-state',$target)){$start.ArgumentList.Add($arg)}
 $proc=[Diagnostics.Process]::Start($start)
 if(-not $proc.WaitForExit(45000)){$proc.Kill();throw "Negative process timed out: $kind"}
 $failure=Join-Path $report 'failure.json'
 if($proc.ExitCode -ne 1 -or -not(Test-Path $failure)){throw "Expected typed native failure for $kind, got exit$($proc.ExitCode)"}
 $body=Get-Content -LiteralPath $failure -Raw | ConvertFrom-Json
 if($body.ok -ne $false -or -not $body.error){throw "Missing failure detail for $kind"}
 $expected=@{checksum='checksum mismatch';configuration='configuration';'payload-binding'='host/registry state mismatch';anchor='regional continuation rejected';habitat='habitat identity mismatch'}
 if($body.error -notmatch $expected[$kind]){throw "Wrong rejection boundary for ${kind}: $($body.error)"}
 if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $targetHash){throw "Native failed load modified prior destination: $kind"}
 if(Test-Path (Join-Path $report 'runtime-receipt.json')){throw "Rejected native load emitted success receipt: $kind"}
 $results+=[ordered]@{case=$kind;exit_code=$proc.ExitCode;error=$body.error;destination_unchanged=$true;evidence=$report}
}
if((Get-FileHash -LiteralPath $SourceSave -Algorithm SHA256).Hash -ne $sourceHash){throw 'Source save changed'}
$receipt=[ordered]@{passed=$true;executable=$Executable;executable_sha256=$exeHash;source_save=$SourceSave;source_sha256=$sourceHash;prior_destination_sha256=$targetHash;cases=$results;scope='Independent serialized actual native rejected-load processes; no positive continuation or visual acceptance claim'}
$receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $taskRoot 'native-negative-receipt.json') -Encoding utf8
$receipt | ConvertTo-Json -Depth 8
