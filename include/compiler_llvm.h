// compiler_llvm.h
//
// Backend de generación de código basado en LLVM (ver input/PLAN_LLVM.md).
// GeneradorLLVM construye el llvm::Module equivalente al AST de Latino, en
// paralelo al backend de C existente (GeneradorC, ver compiler.h) — ninguno
// de los dos reemplaza al otro; se seleccionan con --backend=c|llvm.
//
// Fase L3 (estado actual): `generar()` todavía emite el módulo "hola mundo"
// de plumbing de la Fase L1 (el recorrido real de `programa` -- variables,
// control de flujo, funciones -- llega en las Fases L4-L9, que es cuando
// además queda enlazado al driver). Lo nuevo de esta fase es `genExpr()`:
// traduce literales (`LitNumero`/`LitCadena`/`LitLogico`/`LitNulo`) e
// identificadores a IR real, usando el ABI importado por `RuntimeAbiLLVM`
// (Fase L2) en vez de construir las llamadas al runtime a mano.
#ifndef COMPILER_LLVM_H
#define COMPILER_LLVM_H

#include <memory>
#include <string>
#include <unordered_map>

#include <llvm/IR/IRBuilder.h>

#include "ast.h"
#include "runtime_abi_llvm.h"

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

    // Traduce una expresión aislada a IR en el punto de inserción actual de
    // 'builder', declarando en 'modulo' las funciones del runtime que haga
    // falta (vía RuntimeAbiLLVM). Devuelve un puntero a la celda
    // %struct.LatValor resultante -- NUNCA un valor LLVM de struct por
    // registro: mantener esa representación uniforme (puntero a celda) es
    // lo único consistente con el ABI real descubierto en la Fase L2 (en
    // Windows x64/MSVC, LatValor se pasa/retorna por puntero, nunca por
    // valor) y con la firma uniforme por puntero que usarán los
    // métodos/funciones de usuario a partir de la Fase L8.
    //
    // 'variables' mapea identificadores ya declarados al puntero de su
    // celda; la infraestructura real de declaración de variables (alloca en
    // el entry block, hoisting total) llega en la Fase L4 -- hasta entonces
    // quien llame arma la tabla a mano (así se prueba esta fase de forma
    // aislada). Devuelve nullptr para nodos de expresión que todavía no se
    // manejan (binarios, unarios, control de flujo, ... -- fases futuras) o
    // para un identificador ausente de 'variables'.
    llvm::Value* genExpr(Expresion& expr, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                         const std::unordered_map<std::string, llvm::Value*>& variables = {});

    RuntimeAbiLLVM& abi() { return *abi_; }
    llvm::LLVMContext& contexto() { return *contexto_; }

private:
    std::unique_ptr<llvm::LLVMContext> contexto_;
    std::unique_ptr<RuntimeAbiLLVM> abi_;
};

#endif  // COMPILER_LLVM_H
