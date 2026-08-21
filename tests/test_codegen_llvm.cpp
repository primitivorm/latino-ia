// test_codegen_llvm.cpp — Fase L2 de input/PLAN_LLVM.md.
//
// Solo se compila/registra cuando LATINO_LLVM_BACKEND está habilitado (ver
// tests/CMakeLists.txt). No compara texto de C como test_codegen.cpp: valida
// el mecanismo de ABI (Decisión 2 del plan) --
//   1. RuntimeAbiLLVM importa generated/runtime_abi.ll (generado por Clang a
//      partir de tools/abi_probe.c) sin errores.
//   2. Un módulo que declara funciones del runtime vía RuntimeAbiLLVM y
//      contiene una función trivial que llama a lat_numero/lat_sumar y
//      retorna el resultado pasa llvm::verifyModule.
//   3. El tamaño/alineación de LatValor calculados por el compilador de C++
//      del proyecto (sizeof/alignof sobre runtime/latino.h) coinciden con
//      los que Clang le asignó al StructType importado -- la garantía real
//      de que ambos toolchains están de acuerdo en el ABI (Reto 1 del plan).

#include <iostream>
#include <string>

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "config.h"
#include "runtime_abi_llvm.h"

extern "C" {
#include "latino.h"
}

static int g_fallos = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_fallos;                                                        \
            std::cerr << "  FALLO [linea " << __LINE__ << "]: " << msg << "\n"; \
        }                                                                      \
    } while (0)

int main() {
    CHECK(std::string(LATINO_RUNTIME_ABI_LL) != "",
          "config.h debe traer una ruta a runtime_abi.ll cuando LATINO_LLVM_BACKEND esta ON");

    llvm::LLVMContext contexto;
    RuntimeAbiLLVM abi(contexto, LATINO_RUNTIME_ABI_LL);

    // --- Comprobación 3: el ABI que Clang derivó coincide con el que ve
    // el compilador de C++ del proyecto sobre el mismo header. ---
    llvm::StructType* tipoLatValor = abi.tipoLatValor();
    CHECK(tipoLatValor != nullptr, "runtime_abi.ll debe definir %struct.LatValor");
    if (tipoLatValor) {
        const llvm::DataLayout& dl = abi.moduloOrigen().getDataLayout();
        const llvm::StructLayout* layout = dl.getStructLayout(tipoLatValor);
        CHECK(layout->getSizeInBytes() == sizeof(LatValor),
              "sizeof(LatValor) segun Clang (" << layout->getSizeInBytes()
              << ") != sizeof(LatValor) segun el compilador de C++ (" << sizeof(LatValor) << ")");
        CHECK(dl.getABITypeAlign(tipoLatValor).value() == alignof(LatValor),
              "alineacion de LatValor segun Clang != alineacion segun el compilador de C++");
        CHECK(layout->getElementOffset(0) == offsetof(LatValor, tipo),
              "offset del campo 'tipo' segun Clang != offset segun el compilador de C++");
    }

    // --- Comprobaciones 1 y 2: declarar lat_numero/lat_sumar, construir una
    // función trivial que los invoca, y verificar el módulo. ---
    llvm::Module modulo("test_codegen_llvm", contexto);

    llvm::Function* fnNumero = abi.declarar(modulo, "lat_numero");
    llvm::Function* fnSumar = abi.declarar(modulo, "lat_sumar");
    CHECK(fnNumero != nullptr, "lat_numero debe existir en runtime_abi.ll");
    CHECK(fnSumar != nullptr, "lat_sumar debe existir en runtime_abi.ll");
    CHECK(modulo.getFunction("lat_numero") == fnNumero,
          "declarar() debe insertar la funcion en el modulo destino");

    if (fnNumero && fnSumar && tipoLatValor) {
        llvm::IRBuilder<> builder(contexto);

        llvm::FunctionType* tipoTrivial =
            llvm::FunctionType::get(builder.getInt32Ty(), /*isVarArg=*/false);
        llvm::Function* trivial = llvm::Function::Create(
            tipoTrivial, llvm::Function::ExternalLinkage, "sumar_5_y_3", modulo);
        llvm::BasicBlock* entrada = llvm::BasicBlock::Create(contexto, "entrada", trivial);
        builder.SetInsertPoint(entrada);

        llvm::AllocaInst* a = builder.CreateAlloca(tipoLatValor, nullptr, "a");
        llvm::AllocaInst* b = builder.CreateAlloca(tipoLatValor, nullptr, "b");
        llvm::AllocaInst* c = builder.CreateAlloca(tipoLatValor, nullptr, "c");

        // lat_numero(sret a, 5.0); lat_numero(sret b, 3.0); lat_sumar(sret c, a, b);
        builder.CreateCall(fnNumero, {a, llvm::ConstantFP::get(builder.getDoubleTy(), 5.0)});
        builder.CreateCall(fnNumero, {b, llvm::ConstantFP::get(builder.getDoubleTy(), 3.0)});
        builder.CreateCall(fnSumar, {c, a, b});

        // El campo numérico ('como.numero', un double) vive en el segundo
        // elemento del struct importado -- nunca asumido a mano: es el
        // mismo StructType que Clang derivó para el runtime real.
        llvm::Value* punteroNumero = builder.CreateStructGEP(tipoLatValor, c, 1, "num_ptr");
        llvm::Value* numero = builder.CreateLoad(builder.getDoubleTy(), punteroNumero, "num");
        llvm::Value* numeroI32 = builder.CreateFPToSI(numero, builder.getInt32Ty(), "num_i32");
        builder.CreateRet(numeroI32);

        std::string errores;
        llvm::raw_string_ostream flujoErrores(errores);
        bool invalido = llvm::verifyModule(modulo, &flujoErrores);
        CHECK(!invalido, "el modulo de prueba debe pasar verifyModule: " << flujoErrores.str());
    }

    std::cout << "\nComprobaciones: " << g_checks << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE ABI LLVM (FASE L2) PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
