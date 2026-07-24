param(
    [string]$OutputDirectory = ".\config_dump",
    [string]$Programmer = ""
)
$ErrorActionPreference = "Stop"
if (-not $Programmer) {
    $candidates = @(
        "E:\ST\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "E:\ST\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "$env:ProgramFiles\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )
    $Programmer = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Programmer) { throw "STM32_Programmer_CLI.exe not found; pass -Programmer" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$directory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$region = Join-Path $directory "config_region.bin"
& $Programmer -c port=SWD mode=HotPlug -u 0x0801F000 0x1000 $region 2>&1 |
    Tee-Object -FilePath (Join-Path $directory "dump.log")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$bytes = [IO.File]::ReadAllBytes($region)
if ($bytes.Length -ne 4096) { throw "Expected 4096 bytes, got $($bytes.Length)" }
[IO.File]::WriteAllBytes((Join-Path $directory "slot_a.bin"), $bytes[0..2047])
[IO.File]::WriteAllBytes((Join-Path $directory "slot_b.bin"), $bytes[2048..4095])
Write-Host "Read-only dump complete: $directory"
