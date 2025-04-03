# Crear directorio "build"
New-Item -ItemType Directory -Path .\build

# Navegar al directorio "build"
Set-Location -Path .\build

# Invocar el comando cmake para generar la salida de Visual Studio 2019
cmake -G "Visual Studio 16 2019" -A x64 ..

# Volver al directorio raíz del proyecto
Set-Location -Path ..

# Mostrar mensaje de finalización
Write-Host "Generación de salida de Visual Studio 2019 completada."
