$ErrorActionPreference='Stop'
$root='E:/Stalthosian Archipelago'
$cache=Join-Path $root 'var/vendor/wicked-release-cache'
$lock=Get-Content (Join-Path $root 'stalthos-runtime/config/wicked.lock.json') -Raw | ConvertFrom-Json
$script=Get-Content (Join-Path $root 'stalthos-runtime/scripts/Build.ps1') -Raw
$begin=$script.IndexOf('    if ($provenance.schema_version')
$end=$script.IndexOf("`n}`r`n",$begin)
if($end -lt 0) {$end=$script.IndexOf("`n}`n",$begin)}
$gate=[scriptblock]::Create($script.Substring($begin,$end-$begin))
$raw=Get-Content (Join-Path $cache 'provenance.json') -Raw
foreach($case in @('valid','empty','partial','duplicate','wrong_configuration','wrong_schema','bad_hash')) {
 $provenance=$raw | ConvertFrom-Json
 switch($case) {
 'empty' {$provenance.artifacts=@()}
 'partial' {$provenance.artifacts=@($provenance.artifacts | Select-Object -Skip 1)}
 'duplicate' {$provenance.artifacts[1]=$provenance.artifacts[0]}
 'wrong_configuration' {$provenance.configuration='Debug'}
 'wrong_schema' {$provenance.schema_version=2}
 'bad_hash' {$provenance.artifacts[0].sha256='bad'}
 }
 $accepted=$false
 try { & $gate; $accepted=$true; "$case ACCEPTED" } catch { "$case REJECTED $($_.Exception.Message)" }
 if(($case -eq 'valid') -ne $accepted) {throw "Unexpected outcome: $case"}
}
