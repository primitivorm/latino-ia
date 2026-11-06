# Pregunta al usuario si quiere soporte para el backend LLVM (ver README.md
# / input/PLAN_LLVM.md). Si lo pide, instala LLVM vía install_llvm.ps1 y
# genera un directorio de build separado ("build-llvm") apuntando al
# toolchain de vcpkg, ya que CMAKE_TOOLCHAIN_FILE solo puede fijarse en la
# primera configuración de un directorio de build.
$vcpkgRoot = "C:\vcpkg"
$triplet = "x64-windows-release"
$directorioBuild = ".\build"

$respuesta = Read-Host "¿Deseas soporte para el backend LLVM (--backend=llvm)? (s/N)"
if ($respuesta -match "^[sS]") {
    Write-Host "Instalando LLVM con install_llvm.ps1..."
    .\install_llvm.ps1 -VcpkgRoot $vcpkgRoot -Triplet $triplet
    if ($LASTEXITCODE -ne 0) {
        Write-Error "install_llvm.ps1 falló. Abortando."
        exit 1
    }
    $directorioBuild = ".\build-llvm"
}

# Crear directorio de build si no existe; si ya existe y estaba generado con
# otra plataforma/generador, lo destruimos para evitar los errores de
# "generator platform: x64" / "Does not match the platform used previously".
if (Test-Path $directorioBuild) {
    Write-Host "Se elimina la configuración previa de CMake en $directorioBuild para evitar conflictos de plataforma/generador."
    Remove-Item -Path $directorioBuild -Recurse -Force
}
New-Item -ItemType Directory -Path $directorioBuild | Out-Null

# Navegar al directorio de build
Set-Location -Path $directorioBuild

# Invocar el comando cmake para generar la salida de Visual Studio 2022.
# Usamos -S .. -B . para dejar explícita la fuente y el directorio de salida,
# evitando que CMake reuse un directorio con caché previo bajo otra config.
if ($respuesta -match "^[sS]") {
    cmake -S .. -B . -G "Visual Studio 17 2022" -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
        "-DVCPKG_TARGET_TRIPLET=$triplet"
} else {
    cmake -S .. -B . -G "Visual Studio 17 2022" -A x64
}
if ($LASTEXITCODE -ne 0) {
    Set-Location -Path ..
    Write-Error "La generación de la solución con cmake falló."
    exit 1
}

# Compilar el proyecto en modo Release
cmake --build . --config Release

# Volver al directorio raíz del proyecto
Set-Location -Path ..

if ($LASTEXITCODE -ne 0) {
    Write-Error "La compilación falló."
    exit 1
}

# Mostrar mensaje de finalización
Write-Host "Compilación completada en $directorioBuild (Release). El ejecutable 'latino' está en $directorioBuild\src\Release."
