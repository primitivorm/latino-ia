// compiler_llvm.h
//
// Backend de generación de código basado en LLVM (ver input/PLAN_LLVM.md).
// GeneradorLLVM construye el llvm::Module equivalente al AST de Latino, en
// paralelo al backend de C existente (GeneradorC, ver compiler.h) — ninguno
// de los dos reemplaza al otro; se seleccionan con --backend=c|llvm.
//
// Fase L1 (estado actual): todavía NO recorre el AST real. `generar()` solo
// construye un módulo mínimo para validar el plumbing de compilación +
// enlace de LLVM en el build (find_package, TargetMachine, enlace con el
// runtime de Latino). El recorrido real del AST (literales, expresiones,
// control de flujo, funciones, FFI al runtime, POO) llega en las Fases
// L3–L8. Antes de esas fases hace falta el mecanismo de ABI de la Fase L2
// (ver "Retos técnicos" en el plan) — sin él no es seguro emitir llamadas a
// funciones del runtime que reciben/devuelven LatValor por valor.

#ifndef COMPILER_LLVM_H
#define COMPILER_LLVM_H

#include <memory>

#include "ast.h"

namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm

class GeneradorLLVM {
public:
    GeneradorLLVM();
    ~GeneradorLLVM();

    // Devuelve el llvm::Module equivalente al programa, o nullptr si el
    // módulo construido no pasa la verificación de LLVM (llvm::verifyModule)
    // — eso indicaría un bug del propio generador, no un error del programa
    // de usuario (los errores de sintaxis/semántica ya se descartaron antes
    // de llegar aquí). El módulo devuelto vive mientras viva este
    // GeneradorLLVM: ambos comparten el mismo llvm::LLVMContext.
    std::unique_ptr<llvm::Module> generar(Programa& programa);

private:
    std::unique_ptr<llvm::LLVMContext> contexto_;
};

#endif  // COMPILER_LLVM_H
