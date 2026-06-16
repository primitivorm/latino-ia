// invocador_c.h
//
// Invoca al compilador de C del sistema para transformar el código C generado
// por el GeneradorC en un ejecutable, enlazándolo con el runtime de Latino.

#ifndef INVOCADOR_C_H
#define INVOCADOR_C_H

#include <string>

struct OpcionesC {
    // Carpeta del runtime (latino.h / latino.c). Vacío = usar LATINO_RUNTIME_DIR.
    std::string runtimeDir;
};

// Compila `archivoC` (que hace #include "latino.h") y lo enlaza con el runtime
// para producir `salidaExe`. Devuelve 0 si el ejecutable se generó; un valor
// distinto de cero en caso de error (los mensajes se reportan por stderr).
int compilarAEjecutable(const std::string& archivoC, const std::string& salidaExe,
                        const OpcionesC& opciones);

#endif  // INVOCADOR_C_H
