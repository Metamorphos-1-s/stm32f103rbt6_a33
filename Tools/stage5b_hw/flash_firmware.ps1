param(
    [Parameter(Mandatory=$true)][string]$Elf,
    [string]$Programmer = "",
    [string]$LogPath = "flash.log",
    [switch]$AllowFlash,
    [switch]$Yes,
    [switch]$NoNrst
)
$ErrorActionPreference = "Stop"
if (-not $AllowFlash) { throw "Flashing requires -AllowFlash" }
if (-not $Yes) {
    $answer = Read-Host "Type FLASH_STAGE5B to program and verify the ELF"
    if ($answer -ne "FLASH_STAGE5B") { throw "Confirmation rejected" }
}
$elfPath = (Resolve-Path -LiteralPath $Elf).Path
$logFullPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $LogPath))
$logDirectory = [IO.Path]::GetDirectoryName($logFullPath)
if ($logDirectory) { New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null }
if (-not $Programmer) {
    $candidates = @(
        "E:\ST\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "E:\ST\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "$env:ProgramFiles\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )
    $Programmer = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Programmer -or -not (Test-Path -LiteralPath $Programmer)) {
    throw "STM32_Programmer_CLI.exe not found; pass -Programmer"
}
$metadata = [ordered]@{
    timestamp = (Get-Date).ToString("o")
    git_sha = (git rev-parse HEAD)
    git_status = ((git status --short) -join "`n")
    elf = $elfPath
    elf_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $elfPath).Hash
    programmer = $Programmer
    mass_erase = $false
    config_region_preserved = $true
    nrst_connected = (-not $NoNrst)
}
$metadata | ConvertTo-Json | Set-Content -Encoding UTF8 ([IO.Path]::ChangeExtension($logFullPath, ".json"))
$connectArgs = if ($NoNrst) {
    @("-c", "port=SWD", "mode=Normal", "reset=SWrst")
} else {
    @("-c", "port=SWD", "mode=UR", "reset=HWrst")
}
& $Programmer @connectArgs -w $elfPath -v -rst 2>&1 | Tee-Object -FilePath $logFullPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
