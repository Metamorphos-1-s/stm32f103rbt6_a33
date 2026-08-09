param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Programmer = "E:\ST\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $Programmer)) { throw "STM32_Programmer_CLI.exe not found" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$directory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$dump = Join-Path $directory "stage5b_ram.bin"
& $Programmer -c port=SWD mode=HotPlug -u 0x20000FF3 0xF00 $dump 2>&1 |
    Tee-Object -FilePath (Join-Path $directory "swd_dump.log")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& python (Join-Path $PSScriptRoot "parse_swd_diagnostics.py") $dump `
    --base-address 0x20000FF3 --json (Join-Path $directory "swd_diagnostics.json")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
