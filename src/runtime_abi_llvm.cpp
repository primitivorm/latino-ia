// runtime_abi_llvm.cpp — ver runtime_abi_llvm.h

#include "runtime_abi_llvm.h"

#include <cstdlib>
#include <iostream>

#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

RuntimeAbiLLVM::RuntimeAbiLLVM(llvm::LLVMContext& contexto, const std::string& rutaRuntimeAbiLl) {
    llvm::SMDiagnostic error;
    modulo_ = llvm::parseIRFile(rutaRuntimeAbiLl, error, contexto);
    if (!modulo_) {
        std::cerr << "Error: no se pudo importar el ABI del runtime desde '"
                  << rutaRuntimeAbiLl << "':\n";
        error.print("runtime_abi_llvm", llvm::errs());
        std::cerr << "(¿se generó generated/runtime_abi.ll? ver Fase L2 de "
                     "input/PLAN_LLVM.md)\n";
        std::exit(1);
    }
}

RuntimeAbiLLVM::~RuntimeAbiLLVM() = default;

llvm::StructType* RuntimeAbiLLVM::tipoLatValor() const {
    return llvm::StructType::getTypeByName(modulo_->getContext(), "struct.LatValor");
}

llvm::Function* RuntimeAbiLLVM::declarar(llvm::Module& destino, const std::string& nombre) const {
    llvm::Function* origen = modulo_->getFunction(nombre);
    if (!origen) return nullptr;

    if (llvm::Function* existente = destino.getFunction(nombre)) return existente;

    llvm::Function* declarada = llvm::Function::Create(
        origen->getFunctionType(), llvm::Function::ExternalLinkage, nombre, destino);
    // Copiar los atributos (incluido 'sret' en el retorno indirecto de
    // LatValor por valor) tal como los clasificó Clang — no son parte del
    // FunctionType y hay que preservarlos explícitamente.
    declarada->setAttributes(origen->getAttributes());
    declarada->setCallingConv(origen->getCallingConv());
    return declarada;
}
