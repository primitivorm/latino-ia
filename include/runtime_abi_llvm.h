// runtime_abi_llvm.h — Fase L2 de input/PLAN_LLVM.md: mecanismo de ABI.
//
// RuntimeAbiLLVM importa generated/runtime_abi.ll (emitido por Clang a
// partir de tools/abi_probe.c, ver el `add_custom_command` en
// CMakeLists.txt) y expone el struct %LatValor y las declaraciones de las
// funciones del runtime tal como Clang clasificó su ABI real para el
// target — GeneradorLLVM nunca debe construir ese tipo/esas firmas a mano
// (ver Decisión 2 del plan: la clasificación ABI de un struct de 16 bytes
// pasado/devuelto por valor difiere entre Windows x64 y SysV x86-64).

#ifndef RUNTIME_ABI_LLVM_H
#define RUNTIME_ABI_LLVM_H

#include <memory>
#include <string>

namespace llvm {
class LLVMContext;
class Module;
class StructType;
class Function;
}  // namespace llvm

class RuntimeAbiLLVM {
public:
    // Parsea el archivo .ll indicado dentro de 'contexto'. Termina el
    // proceso con un mensaje claro en stderr si el archivo no existe o no
    // es un módulo LLVM válido: sin este mecanismo ninguna llamada al
    // runtime desde código generado por LLVM es segura (ver Reto 1 del
    // plan). 'contexto' debe seguir viva mientras viva este objeto.
    RuntimeAbiLLVM(llvm::LLVMContext& contexto, const std::string& rutaRuntimeAbiLl);
    ~RuntimeAbiLLVM();

    RuntimeAbiLLVM(const RuntimeAbiLLVM&) = delete;
    RuntimeAbiLLVM& operator=(const RuntimeAbiLLVM&) = delete;

    // El struct LatValor tal como lo clasificó Clang: 'tipo' en el offset
    // 0, tamaño/alineación reales del ABI del target (no un StructType
    // construido a mano por GeneradorLLVM).
    llvm::StructType* tipoLatValor() const;

    // Declara la función 'nombre' del runtime en 'destino' (o devuelve la
    // declaración existente si ya se agregó antes), reutilizando el
    // llvm::FunctionType y los atributos exactos que Clang le asignó en
    // runtime_abi.ll — incluida la clasificación por valor de LatValor
    // (paso indirecto vía puntero con 'sret' en el retorno, ver Fase L2 de
    // input/PLAN_LLVM.md). 'destino' debe compartir el mismo LLVMContext
    // que se pasó al constructor. Devuelve nullptr si 'nombre' no es una
    // función conocida del runtime.
    llvm::Function* declarar(llvm::Module& destino, const std::string& nombre) const;

    // El módulo importado tal cual (todas las declaraciones del runtime +
    // la definición de 'lat_valor_layout_probe'), para casos que necesiten
    // iterarlo directamente (pruebas, o declarar "todo el runtime" en la
    // Fase L7 sin mantener una lista de nombres aparte).
    llvm::Module& moduloOrigen() const { return *modulo_; }

private:
    std::unique_ptr<llvm::Module> modulo_;
};

#endif  // RUNTIME_ABI_LLVM_H
