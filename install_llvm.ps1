<#
.SYNOPSIS
    Instala LLVM (+ Clang) vía vcpkg para el backend --backend=llvm de Latino.

.DESCRIPTION
    Automatiza lo descrito en input/PLAN_LLVM.md (Fase L0/L1):
      1. Clona y arranca vcpkg (si no existe ya en -VcpkgRoot).
      2. Instala el puerto "llvm" con un feature set acotado
         (core, clang, target-x86) y el triplet "x64-windows-release", para
         evitar el problema real que se dio al ejecutar este plan: el feature
         set completo por defecto del puerto (clang, default-targets,
         enable-bindings, enable-terminfo, enable-zlib, enable-zstd, lld,
         tools) construyendo además la variante Debug agotó ~105 GB de disco
         antes de fallar. La combinación de este script usa ~24 GB y tarda
         ~2 horas en una máquina de referencia.
      3. Imprime el comando de CMake para configurar el proyecto con esta
         instalación.

    Es seguro volver a ejecutar este script: si vcpkg ya está clonado/armado
    en -VcpkgRoot, no lo vuelve a clonar; si el paquete LLVM ya está
    instalado con esas mismas features, vcpkg lo detecta y no reconstruye.

.PARAMETER VcpkgRoot
    Carpeta donde clonar/usar vcpkg. Por defecto C:\vcpkg.

.PARAMETER Triplet
    Triplet de vcpkg a usar. Por defecto x64-windows-release (evita
    construir también la variante Debug; ver PLAN_LLVM.md).

.PARAMETER Features
    Features del puerto "llvm" a instalar, separadas por coma. Por defecto
    "core,clang,target-x86" (lo mínimo que necesita este proyecto: la
    biblioteca de LLVM y el compilador clang, que la Fase L2 del plan usa
    para derivar el ABI de LatValor). No usa la sintaxis "[core,...]" que
    suprime las features por defecto del puerto, así que features
    adicionales del puerto (lld, tools, etc.) no se instalan salvo que se
    agreguen aquí explícitamente.

.PARAMETER MinFreeSpaceGB
    Espacio libre mínimo (en GB) requerido en la unidad de -VcpkgRoot antes
    de empezar. Por defecto 60 (con margen sobre los ~24 GB reales medidos).
    Usar -MinFreeSpaceGB 0 para omitir la comprobación.

.EXAMPLE
    .\install_llvm.ps1

.EXAMPLE
    .\install_llvm.ps1 -VcpkgRoot D:\vcpkg -MinFreeSpaceGB 40
#>

[CmdletBinding()]
param(
    [string]$VcpkgRoot = "C:\vcpkg",
    [string]$Triplet = "x64-windows-release",
    [string]$Features = "core,clang,target-x86",
    [int]$MinFreeSpaceGB = 60
)

$ErrorActionPreference = "Stop"

function Write-Paso($mensaje) {
    Write-Host ""
    Write-Host "==> $mensaje" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# 1. Comprobación de espacio en disco
# ---------------------------------------------------------------------------
if ($MinFreeSpaceGB -gt 0) {
    Write-Paso "Comprobando espacio libre en disco..."
    $unidad = (Resolve-Path (Split-Path -Qualifier $VcpkgRoot -ErrorAction SilentlyContinue)`
        -ErrorAction SilentlyContinue)
    $letraUnidad = (Split-Path -Qualifier $VcpkgRoot).TrimEnd('\')
    $disco = Get-PSDrive -Name $letraUnidad.TrimEnd(':') -ErrorAction SilentlyContinue
    if ($disco) {
        $libreGB = [math]::Round($disco.Free / 1GB, 1)
        Write-Host "Espacio libre en ${letraUnidad}: $libreGB GB"
        if ($libreGB -lt $MinFreeSpaceGB) {
            throw ("Solo hay $libreGB GB libres en $letraUnidad y se piden " +
                   "al menos $MinFreeSpaceGB GB (ver input/PLAN_LLVM.md: un " +
                   "feature set demasiado amplio del puerto 'llvm' agotó " +
                   "~105 GB en un intento real). Libera espacio, cambia " +
                   "-VcpkgRoot a otra unidad, o pasa -MinFreeSpaceGB 0 para " +
                   "omitir esta comprobación bajo tu propio riesgo.")
        }
    } else {
        Write-Warning "No se pudo determinar el espacio libre en $letraUnidad; continuando sin verificar."
    }
}

# ---------------------------------------------------------------------------
# 2. Clonar y arrancar vcpkg si hace falta
# ---------------------------------------------------------------------------
$vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"

if (Test-Path $vcpkgExe) {
    Write-Paso "vcpkg ya está listo en $VcpkgRoot"
} else {
    if (Test-Path $VcpkgRoot) {
        $bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
        if (-not (Test-Path $bootstrap)) {
            throw ("$VcpkgRoot ya existe pero no parece un checkout de vcpkg " +
                   "(falta bootstrap-vcpkg.bat). Elegí otra ruta con -VcpkgRoot " +
                   "o vaciá esa carpeta manualmente.")
        }
    } else {
        Write-Paso "Clonando vcpkg en $VcpkgRoot..."
        git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
        if ($LASTEXITCODE -ne 0) { throw "git clone de vcpkg falló (código $LASTEXITCODE)." }
    }

    Write-Paso "Arrancando vcpkg (bootstrap-vcpkg.bat)..."
    & (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat")
    if ($LASTEXITCODE -ne 0) { throw "bootstrap-vcpkg.bat falló (código $LASTEXITCODE)." }
}

# ---------------------------------------------------------------------------
# 3. Instalar LLVM con el feature set acotado
# ---------------------------------------------------------------------------
$paquete = "llvm[$Features]:$Triplet"
Write-Paso "Instalando $paquete (esto puede tardar más de una hora)..."
& $vcpkgExe install $paquete
if ($LASTEXITCODE -ne 0) { throw "vcpkg install $paquete falló (código $LASTEXITCODE)." }

# ---------------------------------------------------------------------------
# 4. Siguientes pasos
# ---------------------------------------------------------------------------
Write-Paso "LLVM instalado correctamente."
Write-Host @"

Para configurar el proyecto con esta instalación (ver README.md):

    cmake -B build-llvm ``
        -DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake ``
        -DVCPKG_TARGET_TRIPLET=$Triplet
    cmake --build build-llvm --config Release

('CMAKE_TOOLCHAIN_FILE' debe fijarse en la primera configuración de un
directorio de build nuevo -- por eso 'build-llvm' y no el 'build/' existente.)

Luego:

    .\build-llvm\src\Release\latino.exe --backend llvm ejemplos\hola.lat -o hola_llvm.exe --runtime runtime
"@
