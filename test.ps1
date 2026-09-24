#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [string]$BuildDir = "build-llvm",
    [ValidateSet("c", "llvm")]
    [string]$Backend = "llvm",
    [string]$Config = "Release",
    [switch]$Help
)

function Show-Usage {
    @"
Uso: test.ps1 [opciones]

Opciones:
  -BuildDir DIR   Directorio de build (por defecto: build-llvm)
  -Backend NAME   Backend c o llvm (por defecto: llvm)
  -Config CONFIG  Configuración del build (por defecto: Release; Visual
                  Studio es un generador multi-config y LLVM via vcpkg
                  solo se instala con el triplet "release", así que un
                  build Debug produce errores de enlace por runtime
                  library distinta)
  -Help           Mostrar esta ayuda
"@
}

if ($Help) {
    Show-Usage
    exit 0
}

$RootDir = $PSScriptRoot
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RootDir $BuildDir
}

$LlvmBackend = if ($Backend -eq "llvm") { "ON" } else { "OFF" }

Write-Host "==> Configurando $BuildDir (LLVM_BACKEND=$LlvmBackend)"
cmake -S $RootDir -B $BuildDir -DLATINO_LLVM_BACKEND=$LlvmBackend
if ($LASTEXITCODE -ne 0) {
    Write-Error "La configuración con cmake falló."
    exit 1
}

Write-Host "`n==> Compilando tests"
if ($Config) {
    cmake --build $BuildDir --config $Config
} else {
    cmake --build $BuildDir
}
if ($LASTEXITCODE -ne 0) {
    Write-Error "La compilación falló."
    exit 1
}

Write-Host "`n==> Ejecutando CTest en serie"
if ($Config) {
    ctest --test-dir $BuildDir -C $Config --output-on-failure --no-tests=error
} else {
    ctest --test-dir $BuildDir --output-on-failure --no-tests=error
}
exit $LASTEXITCODE
