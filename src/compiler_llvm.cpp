// compiler_llvm.cpp — ver compiler_llvm.h
//
// Verificado contra LLVM 18.1.6 real (vcpkg, x64-windows-release, ver Fase
// L1 de input/PLAN_LLVM.md): el uso de punteros opacos (PointerType::get)
// en vez de getInt8PtrTy() compiló sin cambios.

#include "compiler_llvm.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

GeneradorLLVM::GeneradorLLVM() : contexto_(std::make_unique<llvm::LLVMContext>()) {}

GeneradorLLVM::~GeneradorLLVM() = default;

std::unique_ptr<llvm::Module> GeneradorLLVM::generar(Programa& /*programa*/) {
    // Fase L1: "hola mundo" de plumbing. Se llama deliberadamente a puts()
    // de la libc (ABI trivial: i32(ptr)) y NO a lat_escribir/lat_cadena del
    // runtime de Latino — esas funciones reciben/devuelven LatValor por
    // valor, y declarar esa ABI a mano sin el mecanismo de la Fase L2
    // (import de runtime_abi.ll generado por Clang) es exactamente el
    // riesgo que el plan pide evitar. A partir de la Fase L3 este método
    // recorrerá `programa` de verdad, igual que GeneradorC::generar hace
    // con GeneradorC (ver src/compiler.cpp).
    auto modulo = std::make_unique<llvm::Module>("latino_modulo", *contexto_);

    llvm::IRBuilder<> builder(*contexto_);

    // declare i32 @puts(ptr)
    llvm::PointerType* tipoPuntero = llvm::PointerType::get(*contexto_, /*AddressSpace=*/0);
    llvm::FunctionType* tipoPuts =
        llvm::FunctionType::get(builder.getInt32Ty(), {tipoPuntero}, /*isVarArg=*/false);
    llvm::FunctionCallee puts = modulo->getOrInsertFunction("puts", tipoPuts);

    // define i32 @main() { ... ; ret i32 0 }
    llvm::FunctionType* tipoMain = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function* main = llvm::Function::Create(
        tipoMain, llvm::Function::ExternalLinkage, "main", modulo.get());
    llvm::BasicBlock* entrada = llvm::BasicBlock::Create(*contexto_, "entrada", main);
    builder.SetInsertPoint(entrada);

    llvm::Value* saludo = builder.CreateGlobalStringPtr("hola LLVM", "saludo");
    builder.CreateCall(puts, {saludo});
    builder.CreateRet(builder.getInt32(0));

    if (llvm::verifyModule(*modulo, &llvm::errs())) {
        // No debería ocurrir para este módulo trivial; si ocurre, es un bug
        // del generador (o del propio código de esta fase), no del programa
        // de usuario.
        return nullptr;
    }

    return modulo;
}
