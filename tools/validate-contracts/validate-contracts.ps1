$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '../..')
$ble = Get-Content (Join-Path $root 'contracts/ble-v1/constants.json') -Raw | ConvertFrom-Json
$map = Get-Content (Join-Path $root 'contracts/modbus-v0104/register-map.json') -Raw | ConvertFrom-Json
if ($ble.firmwareVersion -ne '0x050A' -or $ble.schemaVersion -ne 2 -or $ble.registerMap -ne '0x0104') { throw 'BLE compatibility constants mismatch' }
$ranges = @{}
foreach($r in $map){ for($i=0;$i -lt [int]$r.register_count;$i++){ $a=[int]$r.address+$i; if($ranges.ContainsKey($a)){ throw "register overlap at $a" }; $ranges[$a]=$r.name } }
Write-Output "Validated $($map.Count) register definitions, no overlaps."
