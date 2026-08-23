// compiler_llvm.cpp — ver compiler_llvm.h
//
// Verificado contra LLVM 18.1.6 real (vcpkg, x64-windows-release, ver Fase
// L1 de input/PLAN_LLVM.md): el uso de punteros opacos (PointerType::get)
// en vez de getInt8PtrTy() compiló sin cambios.

#include "compiler_llvm.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "config.h"

GeneradorLLVM::GeneradorLLVM()
    : contexto_(std::make_unique<llvm::LLVMContext>()),
      abi_(std::make_unique<RuntimeAbiLLVM>(*contexto_, LATINO_RUNTIME_ABI_LL)) {}

GeneradorLLVM::~GeneradorLLVM() = default;

llvm::Value* GeneradorLLVM::genExpr(Expresion& expr, llvm::IRBuilder<>& builder,
                                     llvm::Module& modulo,
                                     const std::unordered_map<std::string, llvm::Value*>& variables) {
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();

    if (auto* n = dynamic_cast<LitNumero*>(&expr)) {
        llvm::Function* fn = abi_->declarar(modulo, "lat_numero");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "lit_numero");
        builder.CreateCall(fn, {celda, llvm::ConstantFP::get(builder.getDoubleTy(), n->valor)});
        return celda;
    }
    if (auto* n = dynamic_cast<LitCadena*>(&expr)) {
        llvm::Function* fn = abi_->declarar(modulo, "lat_cadena");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "lit_cadena");
        llvm::Value* datos =
            builder.CreateGlobalStringPtr(n->valor, "lit_cadena_datos", /*AddressSpace=*/0, &modulo);
        builder.CreateCall(fn, {celda, datos});
        return celda;
    }
    if (auto* n = dynamic_cast<LitLogico*>(&expr)) {
        llvm::Function* fn = abi_->declarar(modulo, "lat_logico");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "lit_logico");
        builder.CreateCall(fn, {celda, builder.getInt32(n->valor ? 1 : 0)});
        return celda;
    }
    if (dynamic_cast<LitNulo*>(&expr)) {
        llvm::Function* fn = abi_->declarar(modulo, "lat_nulo");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "lit_nulo");
        builder.CreateCall(fn, {celda});
        return celda;
    }
    if (auto* n = dynamic_cast<Identificador*>(&expr)) {
        auto it = variables.find(n->nombre);
        return it == variables.end() ? nullptr : it->second;
    }

    // Binarios, unarios, control de flujo, llamadas, ... -- Fases L4 en
    // adelante.
    return nullptr;
}

std::unique_ptr<llvm::Module> GeneradorLLVM::generar(Programa& /*programa*/) {
    // Fase L1: "hola mundo" de plumbing. Se llama deliberadamente a puts()
    // de la libc (ABI trivial: i32(ptr)) y NO a lat_escribir/lat_cadena del
    // runtime de Latino -- el recorrido real de `programa` (declaración de
    // variables, sentencias, funciones) llega en las Fases L4-L9, que es
    // cuando además esto queda enlazado al driver (`main.cpp`). `genExpr()`
    // (Fase L3, ver arriba) ya sabe traducir literales e identificadores,
    // pero todavía no hay desde dónde invocarlo con un `Programa` real.
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
