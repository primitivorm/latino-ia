// compiler_llvm.cpp — ver compiler_llvm.h
//
// Verificado contra LLVM 18.1.6 real (vcpkg, x64-windows-release, ver Fase
// L1 de input/PLAN_LLVM.md): el uso de punteros opacos (PointerType::get)
// en vez de getInt8PtrTy() compiló sin cambios.

#include "compiler_llvm.h"

#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "config.h"
#include "fn_binaria.h"
#include "recolector_variables.h"

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
        llvm::Value* cond = genEsVerdadero(condCelda, builder, modulo);

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
    if (auto* n = dynamic_cast<Llamada*>(&expr)) {
        if (auto* destino = dynamic_cast<Identificador*>(n->destino.get())) {
            const std::string& nombre = destino->nombre;

            // (Fase L7) Builtins de un solo argumento opcional -- el
            // argumento ausente se completa con lat_nulo(), igual que
            // GeneradorC::genLlamada.
            static const std::unordered_map<std::string, const char*> BUILTINS_UN_ARG = {
                {"escribir", "lat_escribir"}, {"imprimir", "lat_imprimir"},
                {"escribe", "lat_escribir"},  {"poner", "lat_escribir"},
                {"acadena", "lat_acadena"},   {"alogico", "lat_alogico"},
                {"anumero", "lat_anumero"},   {"tipo", "lat_tipo"},
                {"error", "lat_error"},       {"incluir", "lat_incluir"},
            };
            auto itBuiltin = BUILTINS_UN_ARG.find(nombre);
            if (itBuiltin != BUILTINS_UN_ARG.end()) {
                llvm::Value* arg;
                if (!n->argumentos.empty()) {
                    arg = genExpr(*n->argumentos[0], builder, modulo, variables);
                    if (!arg) return nullptr;
                } else {
                    llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
                    arg = builder.CreateAlloca(tipoLatValor, nullptr, "builtin_arg_nulo");
                    builder.CreateCall(fnNulo, {arg});
                }
                llvm::Function* fn = abi_->declarar(modulo, itBuiltin->second);
                llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "builtin_ret");
                builder.CreateCall(fn, {celda, arg});
                return celda;
            }
            // Builtins sin argumentos.
            if (nombre == "leer" || nombre == "limpiar") {
                llvm::Function* fn = abi_->declarar(modulo, "lat_" + nombre);
                llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "builtin_ret");
                builder.CreateCall(fn, {celda});
                return celda;
            }
            // imprimirf(fmt, ...) -- variádica real del runtime, misma
            // firma void que lat_lista_de/lat_dic_de (sret AUSENTE: no hay
            // celda de retorno porque lat_imprimirf devuelve void, a
            // diferencia de todo el resto del runtime). Por eso, a
            // diferencia de cualquier otro builtin de esta lista, esta
            // llamada no produce un valor -- se traduce solo por su efecto
            // (imprimir), igual que en GeneradorC, donde el texto C
            // resultante ("lat_imprimirf(...)") tampoco es una LatValor
            // válida para usar en una expresión anidada (solo aparece hoy
            // como ExprSentencia). Devuelve nullptr como valor -- correcto
            // aquí porque no hay ninguna celda que devolver, no porque el
            // nodo sea "no soportado".
            if (nombre == "imprimirf") {
                llvm::Function* fn = abi_->declarar(modulo, "lat_imprimirf");
                std::vector<llvm::Value*> args{builder.getInt64(n->argumentos.size())};
                for (auto& a : n->argumentos) {
                    llvm::Value* v = genExpr(*a, builder, modulo, variables);
                    if (!v) return nullptr;
                    args.push_back(v);
                }
                builder.CreateCall(fn, args);
                return nullptr;
            }

            // Función de usuario ya declarada (Fase L6).
            auto it = funciones_.find(nombre);
            if (it == funciones_.end()) return nullptr;
            const InfoFuncionUsuario& info = it->second;

            llvm::Value* celdaRet = builder.CreateAlloca(tipoLatValor, nullptr, "llamada_ret");
            std::vector<llvm::Value*> args{celdaRet};

            llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
            size_t nargs = n->argumentos.size();
            for (size_t i = 0; i < info.numParametros; i++) {
                llvm::Value* arg;
                if (i < nargs) {
                    arg = genExpr(*n->argumentos[i], builder, modulo, variables);
                    if (!arg) return nullptr;
                } else {
                    arg = builder.CreateAlloca(tipoLatValor, nullptr, "llamada_arg_nulo");
                    builder.CreateCall(fnNulo, {arg});
                }
                args.push_back(arg);
            }
            if (info.variadico) {
                llvm::Function* fnListaDe = abi_->declarar(modulo, "lat_lista_de");
                llvm::Value* resto = builder.CreateAlloca(tipoLatValor, nullptr, "llamada_resto");
                size_t nResto = (nargs > info.numParametros) ? nargs - info.numParametros : 0;
                std::vector<llvm::Value*> argsResto{resto, builder.getInt64(nResto)};
                for (size_t i = info.numParametros; i < nargs; i++) {
                    llvm::Value* v = genExpr(*n->argumentos[i], builder, modulo, variables);
                    if (!v) return nullptr;
                    argsResto.push_back(v);
                }
                builder.CreateCall(fnListaDe, argsResto);
                args.push_back(resto);
            }
            builder.CreateCall(info.fn, args);
            return celdaRet;
        }

        if (auto* am = dynamic_cast<AccesoMiembro*>(n->destino.get())) {
            // (Fase L7) Llamada de librería: cadena.xxx(args), lista.xxx(args), ...
            if (auto* obj = dynamic_cast<Identificador*>(am->objeto.get())) {
                static const std::set<std::string> LIBS = {
                    "cadena", "lista", "dic", "mate", "sis", "archivo", "paquete"};
                if (LIBS.count(obj->nombre)) {
                    const std::string& lib = obj->nombre;
                    const std::string& fn = am->miembro;

                    // cadena.formato es variádica (primer arg = fmt, resto = valores).
                    if (lib == "cadena" && fn == "formato") {
                        llvm::Function* fnRt = abi_->declarar(modulo, "lat_cadena_formato");
                        llvm::Value* celda =
                            builder.CreateAlloca(tipoLatValor, nullptr, "cadena_formato");
                        std::vector<llvm::Value*> args{celda,
                                                        builder.getInt64(n->argumentos.size())};
                        for (auto& a : n->argumentos) {
                            llvm::Value* v = genExpr(*a, builder, modulo, variables);
                            if (!v) return nullptr;
                            args.push_back(v);
                        }
                        builder.CreateCall(fnRt, args);
                        return celda;
                    }

                    // Resto de funciones de librería: args fijos.
                    llvm::Function* fnRt = abi_->declarar(modulo, "lat_" + lib + "_" + fn);
                    if (!fnRt) return nullptr;
                    llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "lib_ret");
                    std::vector<llvm::Value*> args{celda};
                    for (auto& a : n->argumentos) {
                        llvm::Value* v = genExpr(*a, builder, modulo, variables);
                        if (!v) return nullptr;
                        args.push_back(v);
                    }
                    builder.CreateCall(fnRt, args);
                    return celda;
                }
            }

            // Llamada a método de objeto / módulo dinámico:
            // lat_obj_llamar_metodo(objeto, nombre, nargs, args...) -- mismo
            // fallback dinámico que GeneradorC::genLlamada usa tanto para
            // "milib.funcionExportada(args)" (objeto de tipo LAT_MODULO,
            // Reto 7 del plan) como para "instancia.metodo(args)" (objeto de
            // tipo LAT_OBJETO, una vez la Fase L8 sepa construir instancias
            // -- este fallback no necesita seguimiento de clases/estructuras
            // porque el despacho ya es dinámico en runtime vía el
            // diccionario de métodos del objeto). Métodos ESTÁTICOS
            // (NombreClase.metodo(...), que no pasan por 'este' y sí
            // requieren resolver en tiempo de compilación si 'NombreClase'
            // es una clase/estructura conocida) siguen pendientes de la Fase
            // L8: GeneradorLLVM todavía no lleva una tabla clases_/
            // estructuras_ análoga a la de GeneradorC.
            llvm::Value* objeto = genExpr(*am->objeto, builder, modulo, variables);
            if (!objeto) return nullptr;
            llvm::Function* fn = abi_->declarar(modulo, "lat_obj_llamar_metodo");
            if (!fn) return nullptr;
            llvm::Value* nombreC =
                builder.CreateGlobalStringPtr(am->miembro, "metodo_nombre", 0, &modulo);
            llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "metodo_ret");
            std::vector<llvm::Value*> args{celda, objeto, nombreC,
                                            builder.getInt32((int)n->argumentos.size())};
            for (auto& a : n->argumentos) {
                llvm::Value* v = genExpr(*a, builder, modulo, variables);
                if (!v) return nullptr;
                args.push_back(v);
            }
            builder.CreateCall(fn, args);
            return celda;
        }

        return nullptr;
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

llvm::Value* GeneradorLLVM::genEsVerdadero(llvm::Value* celda, llvm::IRBuilder<>& builder,
                                            llvm::Module& modulo) {
    if (!celda) return builder.getInt1(false);
    llvm::Function* fn = abi_->declarar(modulo, "lat_es_verdadero");
    llvm::Value* i32 = builder.CreateCall(fn, {celda}, "es_verdadero");
    return builder.CreateICmpNE(i32, builder.getInt32(0), "cond");
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

namespace {
// Un basic block ya terminado (CreateCondBr/CreateBr/CreateRet ya emitido --
// p.ej. por un `romper` dentro del cuerpo que se acaba de traducir) no puede
// recibir más instrucciones: hacerlo produce IR inválido, no solo código
// muerto (a diferencia de C, donde una sentencia tras `break;` simplemente
// nunca se ejecuta pero sigue siendo sintaxis válida). genBloque/genSentencia
// consultan esto antes de cerrar cada construcción con un salto al bloque de
// continuación.
bool bloqueTerminado(llvm::IRBuilder<>& builder) {
    return builder.GetInsertBlock()->getTerminator() != nullptr;
}
}  // namespace

void GeneradorLLVM::genBloque(const ListaSent& cuerpo, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                              const std::unordered_map<std::string, llvm::Value*>& variables) {
    for (const auto& s : cuerpo) {
        if (!s) continue;
        if (bloqueTerminado(builder)) break;
        genSentencia(*s, builder, modulo, variables);
    }
}

void GeneradorLLVM::genSentencia(Sentencia& s, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                                 const std::unordered_map<std::string, llvm::Value*>& variables) {
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();

    if (auto* a = dynamic_cast<Asignacion*>(&s)) {
        genAsignacion(*a, builder, modulo, variables);
        return;
    }
    if (auto* es = dynamic_cast<ExprSentencia*>(&s)) {
        genExpr(*es->expr, builder, modulo, variables);
        return;
    }
    if (auto* si = dynamic_cast<Si*>(&s)) {
        llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* bloqueMerge = llvm::BasicBlock::Create(*contexto_, "si_fin", fnActual);

        llvm::Value* condCelda = genExpr(*si->condicion, builder, modulo, variables);
        llvm::Value* cond = genEsVerdadero(condCelda, builder, modulo);
        llvm::BasicBlock* bloqueEntonces =
            llvm::BasicBlock::Create(*contexto_, "si_entonces", fnActual);
        llvm::BasicBlock* bloqueSiguiente =
            llvm::BasicBlock::Create(*contexto_, "si_siguiente", fnActual);
        builder.CreateCondBr(cond, bloqueEntonces, bloqueSiguiente);

        builder.SetInsertPoint(bloqueEntonces);
        genBloque(si->entonces, builder, modulo, variables);
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueMerge);

        // Cadena de "osi": cada uno reutiliza el bloque "siguiente" del
        // anterior como su propio punto de entrada, igual que la cadena de
        // "} else if (...) {" que emite GeneradorC.
        for (auto& rama : si->osis) {
            builder.SetInsertPoint(bloqueSiguiente);
            llvm::Value* condOsiCelda = genExpr(*rama.condicion, builder, modulo, variables);
            llvm::Value* condOsi = genEsVerdadero(condOsiCelda, builder, modulo);
            llvm::BasicBlock* bloqueCuerpoOsi =
                llvm::BasicBlock::Create(*contexto_, "si_osi", fnActual);
            llvm::BasicBlock* bloqueSiguienteOsi =
                llvm::BasicBlock::Create(*contexto_, "si_siguiente", fnActual);
            builder.CreateCondBr(condOsi, bloqueCuerpoOsi, bloqueSiguienteOsi);

            builder.SetInsertPoint(bloqueCuerpoOsi);
            genBloque(rama.cuerpo, builder, modulo, variables);
            if (!bloqueTerminado(builder)) builder.CreateBr(bloqueMerge);

            bloqueSiguiente = bloqueSiguienteOsi;
        }

        builder.SetInsertPoint(bloqueSiguiente);
        if (si->tieneSino) genBloque(si->sino, builder, modulo, variables);
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueMerge);

        builder.SetInsertPoint(bloqueMerge);
        return;
    }
    if (auto* el = dynamic_cast<Elegir*>(&s)) {
        // Cadena de comparaciones (lat_igual + lat_es_verdadero), nunca un
        // SwitchInst nativo de LLVM -- ver Reto 8 del plan: los valores de
        // caso son expresiones dinámicas evaluadas en runtime, no enteros
        // constantes de compilación.
        llvm::Value* opcion = genExpr(*el->opcion, builder, modulo, variables);
        if (!opcion) return;

        llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* bloqueMerge = llvm::BasicBlock::Create(*contexto_, "elegir_fin", fnActual);
        llvm::Function* fnIgual = abi_->declarar(modulo, "lat_igual");

        llvm::BasicBlock* bloqueSiguiente = builder.GetInsertBlock();
        for (auto& caso : el->casos) {
            builder.SetInsertPoint(bloqueSiguiente);
            llvm::Value* valorCaso = genExpr(*caso.valor, builder, modulo, variables);
            llvm::Value* cond;
            if (valorCaso) {
                llvm::Value* igualdad =
                    builder.CreateAlloca(tipoLatValor, nullptr, "elegir_igual");
                builder.CreateCall(fnIgual, {igualdad, opcion, valorCaso});
                cond = genEsVerdadero(igualdad, builder, modulo);
            } else {
                cond = builder.getInt1(false);
            }
            llvm::BasicBlock* bloqueCuerpo =
                llvm::BasicBlock::Create(*contexto_, "elegir_caso", fnActual);
            llvm::BasicBlock* bloqueSiguienteCaso =
                llvm::BasicBlock::Create(*contexto_, "elegir_siguiente", fnActual);
            builder.CreateCondBr(cond, bloqueCuerpo, bloqueSiguienteCaso);

            builder.SetInsertPoint(bloqueCuerpo);
            genBloque(caso.cuerpo, builder, modulo, variables);
            if (!bloqueTerminado(builder)) builder.CreateBr(bloqueMerge);

            bloqueSiguiente = bloqueSiguienteCaso;
        }

        builder.SetInsertPoint(bloqueSiguiente);
        if (el->tieneDefecto) genBloque(el->defecto, builder, modulo, variables);
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueMerge);

        builder.SetInsertPoint(bloqueMerge);
        return;
    }
    if (auto* mi = dynamic_cast<Mientras*>(&s)) {
        llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* bloqueHeader =
            llvm::BasicBlock::Create(*contexto_, "mientras_header", fnActual);
        llvm::BasicBlock* bloqueCuerpo =
            llvm::BasicBlock::Create(*contexto_, "mientras_cuerpo", fnActual);
        llvm::BasicBlock* bloqueSalida =
            llvm::BasicBlock::Create(*contexto_, "mientras_fin", fnActual);

        builder.CreateBr(bloqueHeader);
        builder.SetInsertPoint(bloqueHeader);
        llvm::Value* condCelda = genExpr(*mi->condicion, builder, modulo, variables);
        llvm::Value* cond = genEsVerdadero(condCelda, builder, modulo);
        builder.CreateCondBr(cond, bloqueCuerpo, bloqueSalida);

        builder.SetInsertPoint(bloqueCuerpo);
        pilaSalidasBucle_.push_back(bloqueSalida);
        genBloque(mi->cuerpo, builder, modulo, variables);
        pilaSalidasBucle_.pop_back();
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueHeader);

        builder.SetInsertPoint(bloqueSalida);
        return;
    }
    if (auto* de = dynamic_cast<Desde*>(&s)) {
        if (de->inicio) genSentencia(*de->inicio, builder, modulo, variables);

        llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* bloqueHeader =
            llvm::BasicBlock::Create(*contexto_, "desde_header", fnActual);
        llvm::BasicBlock* bloqueCuerpo =
            llvm::BasicBlock::Create(*contexto_, "desde_cuerpo", fnActual);
        llvm::BasicBlock* bloqueLatch =
            llvm::BasicBlock::Create(*contexto_, "desde_incremento", fnActual);
        llvm::BasicBlock* bloqueSalida =
            llvm::BasicBlock::Create(*contexto_, "desde_fin", fnActual);

        builder.CreateBr(bloqueHeader);
        builder.SetInsertPoint(bloqueHeader);
        llvm::Value* condCelda = genExpr(*de->condicion, builder, modulo, variables);
        llvm::Value* cond = genEsVerdadero(condCelda, builder, modulo);
        builder.CreateCondBr(cond, bloqueCuerpo, bloqueSalida);

        // El incremento vive en su propio bloque ("latch"), después del
        // cuerpo -- un `romper` dentro del cuerpo salta directo a
        // bloqueSalida (ver más abajo) y se salta el incremento, igual que
        // `break;` dentro del `while (...) { cuerpo; incremento; }` que
        // emite GeneradorC::genSentencia para Desde.
        builder.SetInsertPoint(bloqueCuerpo);
        pilaSalidasBucle_.push_back(bloqueSalida);
        genBloque(de->cuerpo, builder, modulo, variables);
        pilaSalidasBucle_.pop_back();
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueLatch);

        builder.SetInsertPoint(bloqueLatch);
        if (de->incremento) genSentencia(*de->incremento, builder, modulo, variables);
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueHeader);

        builder.SetInsertPoint(bloqueSalida);
        return;
    }
    if (auto* re = dynamic_cast<Repetir*>(&s)) {
        llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* bloqueCuerpo =
            llvm::BasicBlock::Create(*contexto_, "repetir_cuerpo", fnActual);
        llvm::BasicBlock* bloqueCondicion =
            llvm::BasicBlock::Create(*contexto_, "repetir_condicion", fnActual);
        llvm::BasicBlock* bloqueSalida =
            llvm::BasicBlock::Create(*contexto_, "repetir_fin", fnActual);

        builder.CreateBr(bloqueCuerpo);
        builder.SetInsertPoint(bloqueCuerpo);
        pilaSalidasBucle_.push_back(bloqueSalida);
        genBloque(re->cuerpo, builder, modulo, variables);
        pilaSalidasBucle_.pop_back();
        if (!bloqueTerminado(builder)) builder.CreateBr(bloqueCondicion);

        builder.SetInsertPoint(bloqueCondicion);
        llvm::Value* condCelda = genExpr(*re->condicionHasta, builder, modulo, variables);
        llvm::Value* cond = genEsVerdadero(condCelda, builder, modulo);
        // repetir ... hasta condicion  <=>  do { ... } while (!condicion):
        // si la condición ya es verdadera, sale; si no, repite el cuerpo.
        builder.CreateCondBr(cond, bloqueSalida, bloqueCuerpo);

        builder.SetInsertPoint(bloqueSalida);
        return;
    }
    if (dynamic_cast<Romper*>(&s)) {
        if (!pilaSalidasBucle_.empty()) builder.CreateBr(pilaSalidasBucle_.back());
        // Sin bucle contenedor: no debería ocurrir (ver comentario en el
        // header); no hay nada seguro que emitir aquí.
        return;
    }
    if (auto* rt = dynamic_cast<Retornar*>(&s)) {
        if (!celdaRetorno_) return;  // sin función contenedora: ver Romper.
        if (rt->valor) {
            llvm::Value* val = genExpr(*rt->valor, builder, modulo, variables);
            if (!val) return;
            builder.CreateStore(builder.CreateLoad(tipoLatValor, val), celdaRetorno_);
        } else {
            llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
            builder.CreateCall(fnNulo, {celdaRetorno_});
        }
        builder.CreateRetVoid();
        return;
    }
    // Incluir/FuncionDef/ClaseDef/EstructuraDef/InterfazDef/LlamadaBase
    // (declaraciones de nivel superior o de Fases L7/L8): no se traducen
    // aquí -- FuncionDef la traducen declararFuncion/genFuncion, siempre
    // desde fuera de un bloque, igual que GeneradorC::genSentencia.
}

llvm::Function* GeneradorLLVM::declararFuncion(FuncionDef& f, llvm::Module& modulo) {
    auto it = funciones_.find(f.nombre);
    if (it != funciones_.end()) return it->second.fn;

    llvm::PointerType* tipoPuntero = llvm::PointerType::get(*contexto_, /*AddressSpace=*/0);
    std::vector<llvm::Type*> tipos{tipoPuntero};  // celda de retorno (sret)
    for (size_t i = 0; i < f.parametros.size(); i++) tipos.push_back(tipoPuntero);
    if (f.variadico) tipos.push_back(tipoPuntero);  // "lat_resto", ya empaquetada por el llamador
    llvm::FunctionType* tipoFn =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*contexto_), tipos, /*isVarArg=*/false);

    llvm::Function* fn = llvm::Function::Create(
        tipoFn, llvm::Function::InternalLinkage, "lat_fn_" + f.nombre, &modulo);
    fn->addParamAttr(0, llvm::Attribute::getWithStructRetType(*contexto_, abi_->tipoLatValor()));

    funciones_[f.nombre] = InfoFuncionUsuario{fn, f.parametros.size(), f.variadico};
    return fn;
}

void GeneradorLLVM::genFuncion(FuncionDef& f, llvm::Module& modulo) {
    llvm::Function* fn = declararFuncion(f, modulo);
    if (!fn->empty()) return;  // ya se generó el cuerpo (llamada repetida).

    llvm::StructType* tipoLatValor = abi_->tipoLatValor();
    llvm::IRBuilder<> builder(*contexto_);
    llvm::BasicBlock* entrada = llvm::BasicBlock::Create(*contexto_, "entrada", fn);
    builder.SetInsertPoint(entrada);

    auto argumento = fn->arg_begin();
    llvm::Value* celdaRetorno = &*argumento++;

    // Cada parámetro entrante se copia a una celda local fresca -- nunca se
    // reutiliza el puntero recibido como celda de la variable (ver
    // comentario de esta función en compiler_llvm.h): el llamador puede
    // pasar el puntero de su propia variable, y Latino tiene semántica de
    // paso por valor.
    std::unordered_map<std::string, llvm::Value*> variables;
    for (size_t i = 0; i < f.parametros.size(); i++, ++argumento) {
        llvm::Value* celda =
            builder.CreateAlloca(tipoLatValor, nullptr, "v_" + f.parametros[i].nombre);
        builder.CreateStore(builder.CreateLoad(tipoLatValor, &*argumento), celda);
        variables[f.parametros[i].nombre] = celda;
    }
    if (f.variadico) {
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "v_lat_resto");
        builder.CreateStore(builder.CreateLoad(tipoLatValor, &*argumento), celda);
        variables["lat_resto"] = celda;
    }

    std::set<std::string> excluir;
    for (const auto& p : f.parametros) excluir.insert(p.nombre);
    if (f.variadico) excluir.insert("lat_resto");

    std::set<std::string> nombresLocales;
    recolectarVariables(f.cuerpo, nombresLocales, excluir);
    auto locales = declararLocales(nombresLocales, builder, modulo);
    variables.insert(locales.begin(), locales.end());

    // Chequeos de tipo de parámetros anotados (tipado gradual, Fase 27) --
    // paridad con GeneradorC::genFuncion.
    for (const auto& p : f.parametros) {
        if (p.tipo == TipoAnotado::Ninguno) continue;
        llvm::Function* fnVerificar = abi_->declarar(modulo, "lat_verificar_tipo");
        llvm::Value* celdaParam = variables[p.nombre];
        llvm::Value* nombreC = builder.CreateGlobalStringPtr(p.nombre, "nombre_param", 0, &modulo);
        llvm::Value* verificado = builder.CreateAlloca(tipoLatValor, nullptr, "param_verificado");
        builder.CreateCall(fnVerificar, {verificado, celdaParam,
                                         builder.getInt32(tipoAnotadoALatTipo(p.tipo)), nombreC,
                                         builder.getInt32(f.linea)});
        builder.CreateStore(builder.CreateLoad(tipoLatValor, verificado), celdaParam);
    }

    llvm::Value* anteriorRetorno = celdaRetorno_;
    celdaRetorno_ = celdaRetorno;
    genBloque(f.cuerpo, builder, modulo, variables);
    if (!bloqueTerminado(builder)) {
        // Retornar nulo implícito al final -- igual que GeneradorC::genFuncion,
        // que siempre emite "return lat_nulo();" tras el cuerpo.
        llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
        builder.CreateCall(fnNulo, {celdaRetorno});
        builder.CreateRetVoid();
    }
    celdaRetorno_ = anteriorRetorno;
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
