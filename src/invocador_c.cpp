// invocador_c.cpp — ver invocador_c.h

#define _CRT_SECURE_NO_WARNINGS

#include "invocador_c.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "config.h"

namespace fs = std::filesystem;

namespace {

std::string entrecomillar(const std::string& s) {
    return "\"" + s + "\"";
}

// Recopila todos los archivos .c en runtime/libs/ como cadena de argumentos.
std::string libsCSources(const std::string& runtimeDir) {
    std::string result;
    fs::path libsDir = fs::path(runtimeDir) / "libs";
    std::error_code ec;
    if (!fs::exists(libsDir, ec) || !fs::is_directory(libsDir, ec)) return result;
    for (auto& entry : fs::directory_iterator(libsDir, ec)) {
        if (!ec && entry.path().extension() == ".c")
            result += " " + entrecomillar(entry.path().generic_string());
    }
    return result;
}

// Ejecuta el comando MSVC dentro del entorno de Visual Studio escribiendo un
// .bat temporal (evita problemas de comillas anidadas con std::system).
int ejecutarMsvc(const std::string& archivoC, const std::string& salidaExe,
                 const std::string& runtimeDir) {
    fs::path tmp = fs::temp_directory_path();
    fs::path objDir = tmp / "latino_obj";
    std::error_code ec;
    fs::create_directories(objDir, ec);

    std::string vsdevcmd = LATINO_VSDEVCMD;
    // Barras normales y '/' final: evita que '\"' escape la comilla de cierre.
    std::string objArg = objDir.generic_string() + "/";
    std::string libs   = libsCSources(runtimeDir);
    std::string cl =
        "cl /nologo /utf-8 /I " + entrecomillar(runtimeDir) +
        " /I " + entrecomillar(runtimeDir + "/libs") +
        " " + entrecomillar(archivoC) +
        " " + entrecomillar(runtimeDir + "/latino.c") + libs +
        " /Fe:" + entrecomillar(salidaExe) + " /Fo:" + entrecomillar(objArg);

    fs::path bat = tmp / "latino_compilar.bat";
    {
        std::ofstream f(bat);
        f << "@echo off\n";
        // -startdir=none evita que VsDevCmd cambie el directorio de trabajo.
        if (!vsdevcmd.empty())
            f << "call " << entrecomillar(vsdevcmd)
              << " -arch=x64 -no_logo -startdir=none >nul\n";
        f << cl << "\n";
        f << "exit /b %errorlevel%\n";
    }

    if (!vsdevcmd.empty() && !fs::exists(vsdevcmd)) {
        std::cerr << "Advertencia: no se encontró VsDevCmd.bat en " << vsdevcmd
                  << "; se intentará usar 'cl' del PATH." << std::endl;
    }

    std::string comando = entrecomillar(bat.string());
    return std::system(comando.c_str());
}

// Compila con un compilador de estilo GNU (gcc/clang/cc).
int ejecutarGnu(const std::string& cc, const std::string& archivoC,
                const std::string& salidaExe, const std::string& runtimeDir) {
    std::string libs = libsCSources(runtimeDir);
    std::string comando = entrecomillar(cc) + " -std=c11 -O2 -I " +
                          entrecomillar(runtimeDir) +
                          " -I " + entrecomillar(runtimeDir + "/libs") +
                          " " + entrecomillar(archivoC) +
                          " " + entrecomillar(runtimeDir + "/latino.c") + libs +
                          " -o " + entrecomillar(salidaExe) + " -lm";
    return std::system(comando.c_str());
}

}  // namespace

int compilarAEjecutable(const std::string& archivoC, const std::string& salidaExe,
                        const OpcionesC& opciones) {
    std::string runtimeDir =
        opciones.runtimeDir.empty() ? std::string(LATINO_RUNTIME_DIR) : opciones.runtimeDir;

    const char* ccEnv = std::getenv("CC");

    int codigo;
    if (ccEnv != nullptr) {
        // El usuario indicó un compilador propio (gcc/clang) vía CC.
        codigo = ejecutarGnu(ccEnv, archivoC, salidaExe, runtimeDir);
    } else if (std::string(LATINO_CC_ESTILO) == "msvc") {
        codigo = ejecutarMsvc(archivoC, salidaExe, runtimeDir);
    } else {
        codigo = ejecutarGnu(LATINO_CC, archivoC, salidaExe, runtimeDir);
    }

    if (codigo != 0)
        std::cerr << "Error: el compilador de C falló (código " << codigo << ")."
                  << std::endl;
    return codigo;
}
