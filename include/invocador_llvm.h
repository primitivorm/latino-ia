// invocador_llvm.h
//
// Lleva un llvm::Module a ejecutable nativo: inicializa el target nativo,
// crea un llvm::TargetMachine, emite un objeto (.obj/.o) en el propio
// proceso de latino.exe (sin depender de tener `clang` instalado en la
// máquina del usuario final), y reutiliza compilarAEjecutable (ver
// invocador_c.h) para enlazar ese objeto con el runtime de Latino — el mismo
// paso de enlace que ya usa el backend de C (cl.exe/VsDevCmd o cc/gcc/clang
// aceptan objetos .obj/.o en la misma línea de enlace que fuentes .c, así
// que no hace falta lógica de enlace nueva). Ver input/PLAN_LLVM.md,
// decisión "De LLVM IR a ejecutable (AOT)" y Fases L1/L9.

#ifndef INVOCADOR_LLVM_H
#define INVOCADOR_LLVM_H

#include <string>

namespace llvm {
class Module;
}  // namespace llvm

struct OpcionesLLVM {
    // Carpeta del runtime (latino.h / latino.c). Vacío = usar
    // LATINO_RUNTIME_DIR (misma semántica que OpcionesC::runtimeDir, ver
    // invocador_c.h).
    std::string runtimeDir;
};

// Emite `modulo` como objeto nativo y lo enlaza con el runtime de Latino
// para producir `salidaExe`. Devuelve 0 si el ejecutable se generó; un valor
// distinto de cero en caso de error (los mensajes se reportan por stderr).
int compilarLLVMAEjecutable(llvm::Module& modulo, const std::string& salidaExe,
                            const OpcionesLLVM& opciones);

#endif  // INVOCADOR_LLVM_H
