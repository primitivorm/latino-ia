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
$LogPath = Join-Path $RootDir 'test.ps1.log'

# Función helper: ejecuta un bloque de script, captura su salida, la escribe
# tanto a consola como al archivo de log, y devuelve el código de salida.
function EjecutarYLoguear {
    param([string]$Descripcion, [scriptblock]$Bloque)
    Write-Host $Descripcion
    $salida = & $Bloque 2>&1
    $codigo = $LASTEXITCODE
    $salida | Out-File -FilePath $LogPath -Append -Encoding utf8
    # Escribir a consola sin enviar al pipeline, para que el llamador reciba
    # únicamente el código de salida en $codigo (no las líneas de salida).
    $salida | ForEach-Object { Write-Host $_ }
    return $codigo
}
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RootDir $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

if (Test-Path $BuildDir) {
    $cache = Join-Path $BuildDir 'CMakeCache.txt'
    if (Test-Path $cache) {
        Write-Host "Se elimina la configuración previa de CMake en $BuildDir para evitar conflictos de plataforma/generador."
        try {
            Remove-Item -Path $BuildDir -Recurse -Force -ErrorAction Stop
        } catch {
            Write-Error "No se pudo limpiar $BuildDir porque algún proceso de CMake/MSBuild/CTest la tiene abierta. Cierra esos procesos y vuelve a ejecutar este script."
            exit 1
        }
    }
}

$LlvmBackend = if ($Backend -eq "llvm") { "ON" } else { "OFF" }

$ToolchainArgs = @()
if ($Backend -eq 'llvm') {
    $VcpkgRoot = 'C:\vcpkg'
    $VcpkgToolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
    $Triplet = 'x64-windows-release'
    if (-not (Test-Path $VcpkgToolchain)) {
        Write-Error "No se encontró el toolchain de vcpkg en $VcpkgToolchain. Ejecuta install_llvm.ps1 o instala LLVM 18.1.x antes de correr este script."
        exit 1
    }
    $ToolchainArgs = @("-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain", "-DVCPKG_TARGET_TRIPLET=$Triplet")
}

Write-Host "==> Configurando $BuildDir (LLVM_BACKEND=$LlvmBackend)"
$codigo = EjecutarYLoguear "Configurando cmake" {
    cmake -S $RootDir -B $BuildDir -G 'Visual Studio 17 2022' -A x64 -DLATINO_LLVM_BACKEND=$LlvmBackend $ToolchainArgs
}
if ($codigo -ne 0) {
    Write-Error "La configuración con cmake falló."
    exit 1
}

Write-Host "`n==> Compilando tests"
if ($Config) {
    $codigo = EjecutarYLoguear "Compilando con cmake --build --config $Config" {
        cmake --build $BuildDir --config $Config
    }
} else {
    $codigo = EjecutarYLoguear "Compilando con cmake --build" {
        cmake --build $BuildDir
    }
}
if ($codigo -ne 0) {
    Write-Error "La compilación falló."
    exit 1
}

Write-Host "`n==> Ejecutando CTest en serie"
if ($Config) {
    $codigo = EjecutarYLoguear "Ejecutando ctest -C $Config" {
        ctest --test-dir $BuildDir -C $Config --output-on-failure --no-tests=error
    }
} else {
    $codigo = EjecutarYLoguear "Ejecutando ctest" {
        ctest --test-dir $BuildDir --output-on-failure --no-tests=error
    }
}
exit $codigo
