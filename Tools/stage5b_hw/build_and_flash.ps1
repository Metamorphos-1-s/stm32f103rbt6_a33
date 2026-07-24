param([string]$Programmer = "", [switch]$AllowFlash, [switch]$Yes)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Push-Location $root
try {
    $status = (git status --short)
    if ($status) { Write-Warning "Git worktree is dirty; this state is recorded in flash metadata." }
    cmake --preset Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build --preset Release --clean-first
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $elf = Get-ChildItem -Path (Join-Path $root "build\Release") -Filter "*.elf" -Recurse | Select-Object -First 1
    if (-not $elf) { throw "Release ELF not found" }
    $report = Join-Path $PSScriptRoot ("reports\{0}_flash" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Force -Path $report | Out-Null
    & (Join-Path $PSScriptRoot "flash_firmware.ps1") -Elf $elf.FullName -Programmer $Programmer `
        -LogPath (Join-Path $report "flash.log") -AllowFlash:$AllowFlash -Yes:$Yes
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    [ordered]@{ git_sha=(git rev-parse HEAD); git_status=($status -join "`n"); build_type="Release";
        elf=$elf.FullName; timestamp=(Get-Date).ToString("o") } | ConvertTo-Json |
        Set-Content -Encoding UTF8 (Join-Path $report "environment.json")
} finally { Pop-Location }
