// runtime_abi_llvm.cpp — ver runtime_abi_llvm.h

#include "runtime_abi_llvm.h"

#include <cstdlib>
#include <iostream>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
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

    // En SysV x86-64, LatValor (16 bytes) se devuelve y se pasa por valor;
    // el generador usa deliberadamente la convención indirecta uniforme del
    // runtime Windows (celda de salida + punteros). Un wrapper local adapta
    // ambos ABI sin cambiar todos los puntos de llamada del generador.
    auto esLatValorSysV = [](llvm::Type* tipo) {
        if (!tipo->isStructTy()) return false;
        auto* estructura = llvm::cast<llvm::StructType>(tipo);
        return estructura->getNumElements() == 2 &&
               estructura->getElementType(0)->isIntegerTy(32) &&
               estructura->getElementType(1)->isIntegerTy(64);
    };
    llvm::StructType* latValor = tipoLatValor();
    bool retornoPorValor = esLatValorSysV(origen->getReturnType()) && !origen->isVarArg();
    const auto& parametrosOrigen = origen->getFunctionType()->params();
    bool tieneParametroLatValor = false;
    for (size_t i = 0; i < parametrosOrigen.size(); ++i) {
        bool parLatValor = parametrosOrigen[i]->isIntegerTy(32) && i + 1 < parametrosOrigen.size() &&
                           parametrosOrigen[i + 1]->isIntegerTy(64);
        if (esLatValorSysV(parametrosOrigen[i]) || parLatValor) {
            tieneParametroLatValor = true;
            if (parLatValor) ++i;
        }
    }
    bool necesitaWrapper = !origen->isVarArg() && (retornoPorValor || tieneParametroLatValor);
    if (necesitaWrapper) {
          auto* abiLatValor = retornoPorValor
                          ? llvm::cast<llvm::StructType>(origen->getReturnType())
                          : llvm::StructType::get(
                              latValor->getContext(),
                              {llvm::Type::getInt32Ty(latValor->getContext()),
                               llvm::Type::getInt64Ty(latValor->getContext())});
        std::string nombreWrapper = "__latino_abi_" + nombre;
        if (llvm::Function* existente = destino.getFunction(nombreWrapper)) return existente;

        llvm::PointerType* punteroLatValor = llvm::PointerType::get(latValor->getContext(), 0);
        std::vector<llvm::Type*> parametros;
        if (retornoPorValor) parametros.push_back(punteroLatValor);
        for (size_t i = 0; i < parametrosOrigen.size(); ++i) {
            llvm::Type* parametro = parametrosOrigen[i];
            bool parLatValor = parametro->isIntegerTy(32) && i + 1 < parametrosOrigen.size() &&
                               parametrosOrigen[i + 1]->isIntegerTy(64);
            if (esLatValorSysV(parametro) || parLatValor) {
                parametros.push_back(punteroLatValor);
                if (parLatValor) ++i;
            } else {
                parametros.push_back(parametro);
            }
        }

        llvm::Type* retornoWrapper = retornoPorValor
                                         ? llvm::Type::getVoidTy(latValor->getContext())
                                         : origen->getReturnType();
        auto* tipoWrapper = llvm::FunctionType::get(retornoWrapper, parametros, false);
        llvm::Function* wrapper = llvm::Function::Create(
            tipoWrapper, llvm::Function::InternalLinkage, nombreWrapper, destino);
        llvm::Function* real = llvm::Function::Create(
            origen->getFunctionType(), llvm::Function::ExternalLinkage, nombre, destino);
        real->setAttributes(origen->getAttributes());
        real->setCallingConv(origen->getCallingConv());

        llvm::BasicBlock* entrada = llvm::BasicBlock::Create(latValor->getContext(), "entrada", wrapper);
        llvm::IRBuilder<> builder(entrada);
        auto argumentosWrapper = wrapper->arg_begin();
        llvm::Value* salida = retornoPorValor ? &*argumentosWrapper++ : nullptr;
        std::vector<llvm::Value*> argumentosReales;
        for (size_t i = 0; i < parametrosOrigen.size(); ++i) {
            llvm::Type* parametro = parametrosOrigen[i];
            bool parLatValor = parametro->isIntegerTy(32) && i + 1 < parametrosOrigen.size() &&
                               parametrosOrigen[i + 1]->isIntegerTy(64);
            if (esLatValorSysV(parametro)) {
                llvm::Value* argumento = &*argumentosWrapper++;
                argumentosReales.push_back(builder.CreateLoad(parametro, argumento));
            } else if (parLatValor) {
                llvm::Value* argumento = &*argumentosWrapper++;
                llvm::Value* valor = builder.CreateLoad(abiLatValor, argumento);
                argumentosReales.push_back(builder.CreateExtractValue(valor, 0));
                argumentosReales.push_back(builder.CreateExtractValue(valor, 1));
                ++i;
            } else {
                argumentosReales.push_back(&*argumentosWrapper++);
            }
        }
        llvm::Value* resultado = builder.CreateCall(real, argumentosReales);
        if (retornoPorValor) {
            builder.CreateStore(resultado, salida);
            builder.CreateRetVoid();
        } else if (origen->getReturnType()->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(resultado);
        }
        return wrapper;
    }

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
