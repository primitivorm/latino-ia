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
//
// (Fase L10) También expone ejecutarJit(), el modo --jit: ejecuta el mismo
// llvm::Module en memoria vía llvm::orc::LLJIT, sin pasar por objeto +
// enlazador -- ver Decisión 4 del plan.

#ifndef INVOCADOR_LLVM_H
#define INVOCADOR_LLVM_H

#include <memory>
#include <string>

namespace llvm {
class LLVMContext;
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

// (Fase L10) Ejecuta 'modulo' en memoria vía llvm::orc::LLJIT, sin generar
// ningún .obj/.exe intermedio en disco. Toma posesión de 'modulo' y de
// 'contexto' (LLJIT/ThreadSafeModule los necesita por valor, no por
// referencia -- deben seguir vivos mientras el JIT ejecuta) -- típicamente
// el llvm::LLVMContext que ya poseía el GeneradorLLVM que construyó
// 'modulo' (ver GeneradorLLVM::tomarContexto()). Resuelve los símbolos
// `lat_*` del runtime contra el propio proceso de latino.exe (enlazado
// estáticamente vía el target CMake `latino_runtime_estatico`, Decisión 4
// del plan) en vez de contra un objeto/ejecutable separado. Busca y llama al
// símbolo `main` del módulo. Devuelve el código de salida de ese `main`, o
// un valor distinto de cero si el JIT no pudo crearse/resolver el módulo
// (los mensajes se reportan por stderr).
int ejecutarJit(std::unique_ptr<llvm::Module> modulo, std::unique_ptr<llvm::LLVMContext> contexto);

#endif  // INVOCADOR_LLVM_H
