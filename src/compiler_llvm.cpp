// compiler_llvm.cpp — ver compiler_llvm.h
//
// Verificado contra LLVM 18.1.6 real (vcpkg, x64-windows-release, ver Fase
// L1 de input/PLAN_LLVM.md): el uso de punteros opacos (PointerType::get)
// en vez de getInt8PtrTy() compiló sin cambios.

#include "compiler_llvm.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "config.h"
#include "fn_binaria.h"

extern "C" {
// Solo se usa el enum LatTipo (LAT_NUMERO, ...) -- los mismos valores que ya
// usa el runtime real, en vez de re-derivarlos a mano (mismo principio que
// la Decisión 2 del plan para el layout de LatValor: una sola fuente de
// verdad).
#include "latino.h"
}

namespace {

// Ninguno no debe llegar aquí (genAsignacion filtra ese caso antes); el resto
// de los valores de TipoAnotado tiene un LatTipo de runtime correspondiente.
int tipoAnotadoALatTipo(TipoAnotado t) {
    switch (t) {
        case TipoAnotado::Numero: return LAT_NUMERO;
        case TipoAnotado::Cadena: return LAT_CADENA;
        case TipoAnotado::Logico: return LAT_LOGICO;
        case TipoAnotado::Lista:  return LAT_LISTA;
        case TipoAnotado::Dic:    return LAT_DICCIONARIO;
        case TipoAnotado::Nulo:   return LAT_NULO;
        case TipoAnotado::Objeto: return LAT_OBJETO;
        default: return LAT_NULO;
    }
}

}  // namespace

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
    if (dynamic_cast<VarArgs*>(&expr)) {
        // "..." dentro de [...] -- ver ListaLiteral más abajo y el
        // comentario de VarArgs en compiler_llvm.h. La celda del parámetro
        // variádico la declara la Fase L6; aquí solo se busca por el mismo
        // nombre que usa GeneradorC ("lat_resto").
        auto it = variables.find("lat_resto");
        return it == variables.end() ? nullptr : it->second;
    }
    if (auto* n = dynamic_cast<Binaria*>(&expr)) {
        const char* fn = fnBinaria(n->op);
        if (!fn) return nullptr;
        llvm::Value* izq = genExpr(*n->izq, builder, modulo, variables);
        llvm::Value* der = genExpr(*n->der, builder, modulo, variables);
        if (!izq || !der) return nullptr;
        llvm::Function* fnRt = abi_->declarar(modulo, fn);
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "bin");
        builder.CreateCall(fnRt, {celda, izq, der});
        return celda;
    }
    if (auto* n = dynamic_cast<Unaria*>(&expr)) {
        llvm::Value* operando = genExpr(*n->operando, builder, modulo, variables);
        if (!operando) return nullptr;
        llvm::Function* fn = abi_->declarar(modulo, "lat_negar");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "neg");
        builder.CreateCall(fn, {celda, operando});
        return celda;
    }
    if (auto* n = dynamic_cast<PostOperador*>(&expr)) {
        // i++ / i--  ->  (v_i = lat_sumar(v_i, lat_numero(1))): a diferencia
        // de un Binaria, el destino de la escritura (la celda de 'operando')
        // es también uno de los operandos de lectura -- no se puede pasar
        // esa misma celda como 'sret' de la llamada (aliasing entre el
        // puntero de salida y uno de los de entrada). Se calcula en una
        // celda temporal, igual que el temporal oculto que ya usa C para el
        // valor de retorno de la llamada antes de la asignación, y luego se
        // copia (load+store, no una llamada) a la celda de la variable.
        llvm::Value* celdaVar = genExpr(*n->operando, builder, modulo, variables);
        if (!celdaVar) return nullptr;
        const char* fn = (n->op == "++") ? "lat_sumar" : "lat_restar";
        llvm::Function* fnRt = abi_->declarar(modulo, fn);
        llvm::Function* fnNumero = abi_->declarar(modulo, "lat_numero");
        llvm::Value* uno = builder.CreateAlloca(tipoLatValor, nullptr, "post_uno");
        builder.CreateCall(fnNumero, {uno, llvm::ConstantFP::get(builder.getDoubleTy(), 1.0)});
        llvm::Value* temp = builder.CreateAlloca(tipoLatValor, nullptr, "post_tmp");
        builder.CreateCall(fnRt, {temp, celdaVar, uno});
        builder.CreateStore(builder.CreateLoad(tipoLatValor, temp), celdaVar);
        return celdaVar;
    }
    if (auto* n = dynamic_cast<Ternaria*>(&expr)) {
        // (cond) ? siCierto : siFalso -- debe evaluar un solo lado (como el
        // ?: de C que emite GeneradorC), así que se traduce con basic blocks
        // reales en vez de evaluar ambos lados y elegir. La celda de
        // resultado es compartida por ambas ramas (nunca se hace un load de
        // struct por registro para "unificar" el valor -- ver la
        // representación uniforme por puntero de la Fase L3).
        llvm::Value* condCelda = genExpr(*n->condicion, builder, modulo, variables);
        if (!condCelda) return nullptr;
        llvm::Function* fnVerdadero = abi_->declarar(modulo, "lat_es_verdadero");
        llvm::Value* esVerdadero = builder.CreateCall(fnVerdadero, {condCelda}, "tern_cond_i32");
        llvm::Value* cond = builder.CreateICmpNE(esVerdadero, builder.getInt32(0), "tern_cond");

        llvm::Value* resultado = builder.CreateAlloca(tipoLatValor, nullptr, "tern");
        llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* bloqueCierto =
            llvm::BasicBlock::Create(*contexto_, "tern_cierto", fnActual);
        llvm::BasicBlock* bloqueFalso =
            llvm::BasicBlock::Create(*contexto_, "tern_falso", fnActual);
        llvm::BasicBlock* bloqueFin = llvm::BasicBlock::Create(*contexto_, "tern_fin", fnActual);
        builder.CreateCondBr(cond, bloqueCierto, bloqueFalso);

        builder.SetInsertPoint(bloqueCierto);
        llvm::Value* celdaCierto = genExpr(*n->siCierto, builder, modulo, variables);
        if (!celdaCierto) return nullptr;
        builder.CreateStore(builder.CreateLoad(tipoLatValor, celdaCierto), resultado);
        builder.CreateBr(bloqueFin);

        builder.SetInsertPoint(bloqueFalso);
        llvm::Value* celdaFalso = genExpr(*n->siFalso, builder, modulo, variables);
        if (!celdaFalso) return nullptr;
        builder.CreateStore(builder.CreateLoad(tipoLatValor, celdaFalso), resultado);
        builder.CreateBr(bloqueFin);

        builder.SetInsertPoint(bloqueFin);
        return resultado;
    }
    if (auto* n = dynamic_cast<AccesoIndice*>(&expr)) {
        llvm::Value* objeto = genExpr(*n->objeto, builder, modulo, variables);
        llvm::Value* indice = genExpr(*n->indice, builder, modulo, variables);
        if (!objeto || !indice) return nullptr;
        llvm::Function* fn = abi_->declarar(modulo, "lat_obtener_indice");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "idx");
        builder.CreateCall(fn, {celda, objeto, indice});
        return celda;
    }
    if (auto* n = dynamic_cast<AccesoMiembro*>(&expr)) {
        llvm::Value* objeto = genExpr(*n->objeto, builder, modulo, variables);
        if (!objeto) return nullptr;
        llvm::Function* fnCadena = abi_->declarar(modulo, "lat_cadena");
        llvm::Value* clave = builder.CreateAlloca(tipoLatValor, nullptr, "miembro_clave");
        llvm::Value* datos =
            builder.CreateGlobalStringPtr(n->miembro, "miembro_nombre", /*AddressSpace=*/0, &modulo);
        builder.CreateCall(fnCadena, {clave, datos});
        llvm::Function* fn = abi_->declarar(modulo, "lat_obtener_indice");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "miembro");
        builder.CreateCall(fn, {celda, objeto, clave});
        return celda;
    }
    if (auto* n = dynamic_cast<ListaLiteral*>(&expr)) {
        // "[...]" significa la lista de argumentos variádicos (lat_resto),
        // no una lista que la contiene -- igual que GeneradorC::genExpr.
        if (n->elementos.size() == 1 && dynamic_cast<VarArgs*>(n->elementos[0].get()))
            return genExpr(*n->elementos[0], builder, modulo, variables);
        llvm::Function* fn = abi_->declarar(modulo, "lat_lista_de");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "lista_lit");
        std::vector<llvm::Value*> args{celda, builder.getInt64(n->elementos.size())};
        for (auto& el : n->elementos) {
            llvm::Value* v = genExpr(*el, builder, modulo, variables);
            if (!v) return nullptr;
            args.push_back(v);
        }
        builder.CreateCall(fn, args);
        return celda;
    }
    if (auto* n = dynamic_cast<DiccionarioLiteral*>(&expr)) {
        llvm::Function* fn = abi_->declarar(modulo, "lat_dic_de");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "dic_lit");
        std::vector<llvm::Value*> args{celda, builder.getInt64(n->pares.size())};
        for (auto& par : n->pares) {
            llvm::Value* clave = genExpr(*par.clave, builder, modulo, variables);
            llvm::Value* valor = genExpr(*par.valor, builder, modulo, variables);
            if (!clave || !valor) return nullptr;
            args.push_back(clave);
            args.push_back(valor);
        }
        builder.CreateCall(fn, args);
        return celda;
    }

    // Control de flujo, llamadas, POO, ... -- Fases L5 en adelante.
    return nullptr;
}

std::unordered_map<std::string, llvm::Value*> GeneradorLLVM::declararLocales(
    const std::set<std::string>& nombres, llvm::IRBuilder<>& entryBuilder, llvm::Module& modulo) {
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();
    llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");

    std::unordered_map<std::string, llvm::Value*> variables;
    for (const std::string& nombre : nombres) {
        llvm::Value* celda = entryBuilder.CreateAlloca(tipoLatValor, nullptr, "v_" + nombre);
        entryBuilder.CreateCall(fnNulo, {celda});
        variables[nombre] = celda;
    }
    return variables;
}

void GeneradorLLVM::genAsignacionDestino(
    Expresion& destino, llvm::Value* valorCelda, llvm::IRBuilder<>& builder, llvm::Module& modulo,
    const std::unordered_map<std::string, llvm::Value*>& variables) {
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();

    if (auto* id = dynamic_cast<Identificador*>(&destino)) {
        auto it = variables.find(id->nombre);
        if (it == variables.end()) return;
        builder.CreateStore(builder.CreateLoad(tipoLatValor, valorCelda), it->second);
        return;
    }
    if (auto* ai = dynamic_cast<AccesoIndice*>(&destino)) {
        llvm::Value* objeto = genExpr(*ai->objeto, builder, modulo, variables);
        llvm::Value* indice = genExpr(*ai->indice, builder, modulo, variables);
        if (!objeto || !indice) return;
        llvm::Function* fn = abi_->declarar(modulo, "lat_asignar_indice");
        builder.CreateCall(fn, {objeto, indice, valorCelda});
        return;
    }
    if (auto* am = dynamic_cast<AccesoMiembro*>(&destino)) {
        llvm::Value* objeto = genExpr(*am->objeto, builder, modulo, variables);
        if (!objeto) return;
        llvm::Function* fnCadena = abi_->declarar(modulo, "lat_cadena");
        llvm::Value* clave = builder.CreateAlloca(tipoLatValor, nullptr, "miembro_clave");
        llvm::Value* datos =
            builder.CreateGlobalStringPtr(am->miembro, "miembro_nombre", /*AddressSpace=*/0, &modulo);
        builder.CreateCall(fnCadena, {clave, datos});
        llvm::Function* fn = abi_->declarar(modulo, "lat_asignar_indice");
        builder.CreateCall(fn, {objeto, clave, valorCelda});
        return;
    }
    // Otro tipo de destino: no debería llegar aquí (el analizador semántico
    // ya rechaza lvalues inválidos antes de llegar al backend).
}

void GeneradorLLVM::genAsignacion(
    Asignacion& asign, llvm::IRBuilder<>& builder, llvm::Module& modulo,
    const std::unordered_map<std::string, llvm::Value*>& variables) {
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();

    auto conVerificacionDeTipo = [&](llvm::Value* valorCelda, TipoAnotado tipo,
                                     const std::string& nombreVar, int linea) -> llvm::Value* {
        if (tipo == TipoAnotado::Ninguno) return valorCelda;
        llvm::Function* fn = abi_->declarar(modulo, "lat_verificar_tipo");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "verificado");
        llvm::Value* nombreC = builder.CreateGlobalStringPtr(nombreVar, "nombre_var", 0, &modulo);
        builder.CreateCall(fn, {celda, valorCelda, builder.getInt32(tipoAnotadoALatTipo(tipo)),
                                nombreC, builder.getInt32(linea)});
        return celda;
    };

    if (asign.destinos.size() == 1 && asign.valores.size() == 1) {
        llvm::Value* val = genExpr(*asign.valores[0], builder, modulo, variables);
        if (!val) return;
        TipoAnotado tipo = asign.tiposDestino.empty() ? TipoAnotado::Ninguno : asign.tiposDestino[0];
        std::string nombreVar;
        if (auto* id = dynamic_cast<Identificador*>(asign.destinos[0].get())) nombreVar = id->nombre;
        val = conVerificacionDeTipo(val, tipo, nombreVar, asign.linea);
        genAsignacionDestino(*asign.destinos[0], val, builder, modulo, variables);
        return;
    }

    // Asignación múltiple: se evalúan TODOS los valores a celdas temporales
    // antes de asignar ningún destino -- igual que GeneradorC (que usa
    // temporales _tN) y no en el orden entrelazado que produciría reusar
    // directamente las celdas de las variables si origen y destino se
    // solapan (p.ej. "a, b = b, a").
    llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
    std::vector<llvm::Value*> temporales;
    for (auto& v : asign.valores) {
        llvm::Value* val = genExpr(*v, builder, modulo, variables);
        llvm::Value* temp = builder.CreateAlloca(tipoLatValor, nullptr, "asig_tmp");
        if (val)
            builder.CreateStore(builder.CreateLoad(tipoLatValor, val), temp);
        else
            builder.CreateCall(fnNulo, {temp});
        temporales.push_back(temp);
    }
    for (size_t i = 0; i < asign.destinos.size(); i++) {
        llvm::Value* rhs;
        if (i < temporales.size()) {
            rhs = temporales[i];
        } else {
            rhs = builder.CreateAlloca(tipoLatValor, nullptr, "asig_tmp_nulo");
            builder.CreateCall(fnNulo, {rhs});
        }
        genAsignacionDestino(*asign.destinos[i], rhs, builder, modulo, variables);
    }
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
