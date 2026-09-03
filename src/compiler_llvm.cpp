// compiler_llvm.cpp — ver compiler_llvm.h
//
// Verificado contra LLVM 18.1.6 real (vcpkg, x64-windows-release, ver Fase
// L1 de input/PLAN_LLVM.md): el uso de punteros opacos (PointerType::get)
// en vez de getInt8PtrTy() compiló sin cambios.

#include "compiler_llvm.h"

#include <functional>

#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
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

                // (Fase L8) Método estático: NombreClase.metodo(args...) /
                // NombreEstructura.metodo(args...) -- se resuelve en tiempo
                // de compilación si 'obj->nombre' nombra una clase/
                // estructura conocida con un método estático de ese nombre,
                // recorriendo la cadena de herencia hacia arriba para una
                // clase (mismo algoritmo que GeneradorC::genLlamada). A
                // diferencia del despacho dinámico de más abajo, no hay
                // celda "este": el array de argumentos empieza directamente
                // en el primer argumento real.
                std::string tipoDeclarante;
                MetodoDef* metodoEstatico = nullptr;
                {
                    auto itC = clases_.find(obj->nombre);
                    for (ClaseDef* t = (itC != clases_.end()) ? itC->second : nullptr; t;) {
                        bool encontrado = false;
                        for (MetodoDef& m : t->metodos) {
                            if (m.nombre == am->miembro && m.esEstatico) {
                                metodoEstatico = &m;
                                encontrado = true;
                                break;
                            }
                        }
                        if (encontrado) {
                            tipoDeclarante = t->nombre;
                            break;
                        }
                        if (t->padre.empty()) break;
                        auto itPadre = clases_.find(t->padre);
                        t = (itPadre != clases_.end()) ? itPadre->second : nullptr;
                    }
                    if (tipoDeclarante.empty()) {
                        auto itE = estructuras_.find(obj->nombre);
                        if (itE != estructuras_.end()) {
                            for (MetodoDef& m : itE->second->metodos) {
                                if (m.nombre == am->miembro && m.esEstatico) {
                                    metodoEstatico = &m;
                                    tipoDeclarante = itE->second->nombre;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (metodoEstatico) {
                    llvm::Function* fnMetodo = declararMetodo(tipoDeclarante, *metodoEstatico, modulo);
                    size_t nargs = n->argumentos.size();
                    llvm::Value* argsArr;
                    if (nargs > 0) {
                        argsArr = builder.CreateAlloca(tipoLatValor, builder.getInt64(nargs),
                                                        "estatico_args");
                        for (size_t i = 0; i < nargs; i++) {
                            llvm::Value* v = genExpr(*n->argumentos[i], builder, modulo, variables);
                            if (!v) return nullptr;
                            llvm::Value* slot =
                                builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(i), "estatico_arg");
                            builder.CreateStore(builder.CreateLoad(tipoLatValor, v), slot);
                        }
                    } else {
                        argsArr = llvm::ConstantPointerNull::get(llvm::PointerType::get(*contexto_, 0));
                    }
                    llvm::Value* celdaRet = builder.CreateAlloca(tipoLatValor, nullptr, "estatico_ret");
                    builder.CreateCall(fnMetodo, {celdaRet, builder.getInt32((int)nargs), argsArr});
                    return celdaRet;
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
    if (auto* n = dynamic_cast<NuevoExpr*>(&expr)) {
        // (Fase L8) Mismo mapeo 1:1 que GeneradorC::genExpr(NuevoExpr) -- ver
        // el comentario de esta fase en compiler_llvm.h para el resumen del
        // algoritmo (ancestría + registro de campos/métodos + constructor).
        llvm::Function* fnObjNuevo = abi_->declarar(modulo, "lat_obj_nuevo");
        llvm::Function* fnObjSetClase = abi_->declarar(modulo, "lat_obj_set_clase");
        llvm::Function* fnObjSet = abi_->declarar(modulo, "lat_obj_set");
        llvm::Function* fnObjSetMetodo = abi_->declarar(modulo, "lat_obj_set_metodo");
        llvm::Function* fnFuncionNueva = abi_->declarar(modulo, "lat_funcion_nueva");

        llvm::Value* obj = builder.CreateAlloca(tipoLatValor, nullptr, "obj_nuevo");

        auto itC = clases_.find(n->clase);
        if (itC != clases_.end()) {
            ClaseDef* c = itC->second;

            // Cadena de ascendencia (de la hoja a la raíz) -- lat_obj_nuevo
            // crea el objeto con el nombre del ancestro más antiguo, y cada
            // lat_obj_set_clase posterior (de la raíz hacia la hoja) va
            // registrando el resto de la cadena, para que
            // lat_obj_es_instancia reconozca a los ancestros tras la
            // herencia -- mismo algoritmo que GeneradorC.
            std::vector<ClaseDef*> cadena;
            for (ClaseDef* t = c; t;) {
                cadena.push_back(t);
                if (t->padre.empty()) break;
                auto itPadreTipo = clases_.find(t->padre);
                t = (itPadreTipo != clases_.end()) ? itPadreTipo->second : nullptr;
            }
            llvm::Value* nombreRaiz =
                builder.CreateGlobalStringPtr(cadena.back()->nombre, "obj_clase_raiz", 0, &modulo);
            builder.CreateCall(fnObjNuevo, {obj, nombreRaiz});
            for (size_t i = cadena.size() - 1; i-- > 0;) {
                llvm::Value* nombreNivel =
                    builder.CreateGlobalStringPtr(cadena[i]->nombre, "obj_clase_nivel", 0, &modulo);
                builder.CreateCall(fnObjSetClase, {obj, nombreNivel});
            }

            std::function<void(ClaseDef*)> registrarTipo = [&](ClaseDef* tipo) {
                if (!tipo) return;
                if (!tipo->padre.empty()) {
                    auto itPadre = clases_.find(tipo->padre);
                    if (itPadre != clases_.end()) registrarTipo(itPadre->second);
                }
                for (CampoDef& campo : tipo->campos) {
                    if (!campo.valorDefecto) continue;
                    llvm::Value* valor = genExpr(*campo.valorDefecto, builder, modulo, variables);
                    if (!valor) continue;
                    llvm::Value* nombreCampo =
                        builder.CreateGlobalStringPtr(campo.nombre, "obj_campo_nombre", 0, &modulo);
                    builder.CreateCall(fnObjSet, {obj, nombreCampo, valor});
                }
                for (MetodoDef& metodo : tipo->metodos) {
                    if (metodo.esConstructor || metodo.esEstatico || metodo.esAbstracto) continue;
                    llvm::Function* metodoFn = declararMetodo(tipo->nombre, metodo, modulo);
                    llvm::Value* nombreMetodo =
                        builder.CreateGlobalStringPtr(metodo.nombre, "obj_metodo_nombre", 0, &modulo);
                    llvm::Value* celdaFn = builder.CreateAlloca(tipoLatValor, nullptr, "obj_metodo_valor");
                    builder.CreateCall(fnFuncionNueva, {celdaFn, metodoFn});
                    builder.CreateCall(fnObjSetMetodo, {obj, nombreMetodo, celdaFn});
                }
            };
            registrarTipo(c);

            MetodoDef* ctor = nullptr;
            for (MetodoDef& metodo : c->metodos) {
                if (metodo.esConstructor) {
                    ctor = &metodo;
                    break;
                }
            }
            if (ctor) {
                llvm::Function* fnCtor = declararMetodo(c->nombre, *ctor, modulo);
                size_t nargsTotal = n->argumentos.size() + 1;
                llvm::Value* argsArr =
                    builder.CreateAlloca(tipoLatValor, builder.getInt64(nargsTotal), "ctor_args");
                llvm::Value* slot0 = builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(0), "ctor_arg0");
                builder.CreateStore(builder.CreateLoad(tipoLatValor, obj), slot0);
                for (size_t i = 0; i < n->argumentos.size(); i++) {
                    llvm::Value* v = genExpr(*n->argumentos[i], builder, modulo, variables);
                    if (!v) return nullptr;
                    llvm::Value* slot =
                        builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(i + 1), "ctor_arg");
                    builder.CreateStore(builder.CreateLoad(tipoLatValor, v), slot);
                }
                llvm::Value* ctorRet = builder.CreateAlloca(tipoLatValor, nullptr, "ctor_ret");
                builder.CreateCall(fnCtor, {ctorRet, builder.getInt32((int)nargsTotal), argsArr});
            }
        } else {
            llvm::Value* nombreLiteral =
                builder.CreateGlobalStringPtr(n->clase, "obj_clase_nombre_lit", 0, &modulo);
            builder.CreateCall(fnObjNuevo, {obj, nombreLiteral});

            auto itE = estructuras_.find(n->clase);
            if (itE != estructuras_.end()) {
                EstructuraDef* e = itE->second;
                for (CampoDef& campo : e->campos) {
                    if (!campo.valorDefecto) continue;
                    llvm::Value* valor = genExpr(*campo.valorDefecto, builder, modulo, variables);
                    if (!valor) continue;
                    llvm::Value* nombreCampo =
                        builder.CreateGlobalStringPtr(campo.nombre, "obj_campo_nombre", 0, &modulo);
                    builder.CreateCall(fnObjSet, {obj, nombreCampo, valor});
                }
                for (MetodoDef& metodo : e->metodos) {
                    if (metodo.esConstructor || metodo.esEstatico || metodo.esAbstracto) continue;
                    llvm::Function* metodoFn = declararMetodo(e->nombre, metodo, modulo);
                    llvm::Value* nombreMetodo =
                        builder.CreateGlobalStringPtr(metodo.nombre, "obj_metodo_nombre", 0, &modulo);
                    llvm::Value* celdaFn = builder.CreateAlloca(tipoLatValor, nullptr, "obj_metodo_valor");
                    builder.CreateCall(fnFuncionNueva, {celdaFn, metodoFn});
                    builder.CreateCall(fnObjSetMetodo, {obj, nombreMetodo, celdaFn});
                }

                MetodoDef* ctor = nullptr;
                for (MetodoDef& metodo : e->metodos) {
                    if (metodo.esConstructor) {
                        ctor = &metodo;
                        break;
                    }
                }
                if (ctor) {
                    llvm::Function* fnCtor = declararMetodo(e->nombre, *ctor, modulo);
                    size_t nargsTotal = n->argumentos.size() + 1;
                    llvm::Value* argsArr =
                        builder.CreateAlloca(tipoLatValor, builder.getInt64(nargsTotal), "ctor_args");
                    llvm::Value* slot0 =
                        builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(0), "ctor_arg0");
                    builder.CreateStore(builder.CreateLoad(tipoLatValor, obj), slot0);
                    for (size_t i = 0; i < n->argumentos.size(); i++) {
                        llvm::Value* v = genExpr(*n->argumentos[i], builder, modulo, variables);
                        if (!v) return nullptr;
                        llvm::Value* slot =
                            builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(i + 1), "ctor_arg");
                        builder.CreateStore(builder.CreateLoad(tipoLatValor, v), slot);
                    }
                    llvm::Value* ctorRet = builder.CreateAlloca(tipoLatValor, nullptr, "ctor_ret");
                    builder.CreateCall(fnCtor, {ctorRet, builder.getInt32((int)nargsTotal), argsArr});
                }
            }
        }
        return obj;
    }
    if (auto* n = dynamic_cast<EsExpr*>(&expr)) {
        // "expr es Clase" -- comprobación en tiempo de ejecución por nombre,
        // no necesita ninguna tabla estática (paridad con
        // GeneradorC::genExpr(EsExpr)).
        llvm::Value* objeto = genExpr(*n->objeto, builder, modulo, variables);
        if (!objeto) return nullptr;
        llvm::Function* fnEsInstancia = abi_->declarar(modulo, "lat_obj_es_instancia");
        llvm::Value* claseC = builder.CreateGlobalStringPtr(n->clase, "es_clase_nombre", 0, &modulo);
        llvm::Value* esInstancia = builder.CreateCall(fnEsInstancia, {objeto, claseC}, "es_instancia");
        llvm::Function* fnLogico = abi_->declarar(modulo, "lat_logico");
        llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, "es_resultado");
        builder.CreateCall(fnLogico, {celda, esInstancia});
        return celda;
    }
    if (dynamic_cast<AccesoEste*>(&expr)) {
        auto it = variables.find("este");
        return it == variables.end() ? nullptr : it->second;
    }

    // Todos los nodos de Expresion reales están cubiertos arriba (Fases
    // L3-L8); nullptr aquí solo puede significar un tipo de nodo que el
    // parser nunca produce.
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
    if (auto* b = dynamic_cast<LlamadaBase*>(&s)) {
        // (Fase L8) base(args...) -- solo válida dentro de un constructor.
        // Empaqueta "este" + los argumentos evaluados en un array contiguo
        // (misma convención que NuevoExpr) y llama al constructor de
        // actualPadre_ si existe uno -- paridad exacta con
        // GeneradorC::genSentencia(LlamadaBase).
        auto itEste = variables.find("este");
        llvm::Value* celdaEste = itEste != variables.end() ? itEste->second : nullptr;

        size_t nargsTotal = b->argumentos.size() + 1;
        llvm::Value* argsArr = builder.CreateAlloca(tipoLatValor, builder.getInt64(nargsTotal), "base_args");
        llvm::Value* slot0 = builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(0), "base_arg0");
        if (celdaEste) {
            builder.CreateStore(builder.CreateLoad(tipoLatValor, celdaEste), slot0);
        } else {
            llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
            builder.CreateCall(fnNulo, {slot0});
        }
        for (size_t i = 0; i < b->argumentos.size(); i++) {
            llvm::Value* v = genExpr(*b->argumentos[i], builder, modulo, variables);
            if (!v) return;
            llvm::Value* slot = builder.CreateGEP(tipoLatValor, argsArr, builder.getInt64(i + 1), "base_arg");
            builder.CreateStore(builder.CreateLoad(tipoLatValor, v), slot);
        }

        MetodoDef* padreCtor = nullptr;
        if (!actualPadre_.empty()) {
            auto itPadre = clases_.find(actualPadre_);
            if (itPadre != clases_.end()) {
                for (MetodoDef& m : itPadre->second->metodos) {
                    if (m.esConstructor) {
                        padreCtor = &m;
                        break;
                    }
                }
            }
        }
        if (padreCtor) {
            llvm::Function* fnPadreCtor = declararMetodo(actualPadre_, *padreCtor, modulo);
            llvm::Value* celdaRet = builder.CreateAlloca(tipoLatValor, nullptr, "base_ret");
            builder.CreateCall(fnPadreCtor, {celdaRet, builder.getInt32((int)nargsTotal), argsArr});
        }
        // Sin constructor de clase base que ejecutar: no hay nada seguro
        // que emitir (paridad con el comentario que deja GeneradorC en ese
        // caso).
        return;
    }
    // Incluir/FuncionDef/ClaseDef/EstructuraDef/InterfazDef (declaraciones
    // de nivel superior): no se traducen aquí -- FuncionDef/ClaseDef/
    // EstructuraDef/InterfazDef las traducen genFuncion/genClase/
    // genEstructura/genInterfaz, siempre desde fuera de un bloque, igual que
    // GeneradorC::genSentencia.
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

void GeneradorLLVM::recolectarTipos(Programa& programa) {
    clases_.clear();
    estructuras_.clear();
    interfaces_.clear();
    for (auto& s : programa.sentencias) {
        if (auto* c = dynamic_cast<ClaseDef*>(s.get()))
            clases_[c->nombre] = c;
        else if (auto* e = dynamic_cast<EstructuraDef*>(s.get()))
            estructuras_[e->nombre] = e;
        else if (auto* i = dynamic_cast<InterfazDef*>(s.get()))
            interfaces_[i->nombre] = i;
    }
}

llvm::Function* GeneradorLLVM::declararMetodo(const std::string& claseNombre, MetodoDef& metodo,
                                              llvm::Module& modulo) {
    std::string nombreFn = "lat_fn_" + claseNombre + "_" + metodo.nombre;
    if (llvm::Function* existente = modulo.getFunction(nombreFn)) return existente;

    llvm::PointerType* tipoPuntero = llvm::PointerType::get(*contexto_, /*AddressSpace=*/0);
    llvm::Type* tipoI32 = llvm::Type::getInt32Ty(*contexto_);
    // sret, nargs, args -- ver el comentario de esta función en
    // compiler_llvm.h para por qué la firma es "empaquetada" (nargs +
    // array) en vez del puntero-por-parámetro que usa declararFuncion.
    std::vector<llvm::Type*> tipos{tipoPuntero, tipoI32, tipoPuntero};
    llvm::FunctionType* tipoFn =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*contexto_), tipos, /*isVarArg=*/false);

    llvm::Function* fn = llvm::Function::Create(
        tipoFn, llvm::Function::InternalLinkage, nombreFn, &modulo);
    fn->addParamAttr(0, llvm::Attribute::getWithStructRetType(*contexto_, abi_->tipoLatValor()));
    return fn;
}

llvm::Value* GeneradorLLVM::genArgumentoDeArray(llvm::Value* argsPtr, llvm::Value* nargs, size_t idx,
                                                llvm::IRBuilder<>& builder, llvm::Module& modulo,
                                                const std::string& nombreCelda) {
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();
    llvm::Value* celda = builder.CreateAlloca(tipoLatValor, nullptr, nombreCelda);

    llvm::Function* fnActual = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* bloquePresente = llvm::BasicBlock::Create(*contexto_, "arg_presente", fnActual);
    llvm::BasicBlock* bloqueAusente = llvm::BasicBlock::Create(*contexto_, "arg_ausente", fnActual);
    llvm::BasicBlock* bloqueFin = llvm::BasicBlock::Create(*contexto_, "arg_fin", fnActual);

    llvm::Value* cond = builder.CreateICmpSGT(nargs, builder.getInt32((int)idx), "hay_arg");
    builder.CreateCondBr(cond, bloquePresente, bloqueAusente);

    builder.SetInsertPoint(bloquePresente);
    llvm::Value* slot = builder.CreateGEP(tipoLatValor, argsPtr, builder.getInt64(idx), "arg_slot");
    builder.CreateStore(builder.CreateLoad(tipoLatValor, slot), celda);
    builder.CreateBr(bloqueFin);

    builder.SetInsertPoint(bloqueAusente);
    llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
    builder.CreateCall(fnNulo, {celda});
    builder.CreateBr(bloqueFin);

    builder.SetInsertPoint(bloqueFin);
    return celda;
}

void GeneradorLLVM::genMetodo(const std::string& claseNombre, MetodoDef& metodo,
                              const std::string& padreNombre, llvm::Module& modulo) {
    if (metodo.esAbstracto) return;

    llvm::Function* fn = declararMetodo(claseNombre, metodo, modulo);
    if (!fn->empty()) return;  // ya se generó el cuerpo (llamada repetida).

    llvm::StructType* tipoLatValor = abi_->tipoLatValor();
    llvm::IRBuilder<> builder(*contexto_);
    llvm::BasicBlock* entrada = llvm::BasicBlock::Create(*contexto_, "entrada", fn);
    builder.SetInsertPoint(entrada);

    auto argumento = fn->arg_begin();
    llvm::Value* celdaRetorno = &*argumento++;
    llvm::Value* nargs = &*argumento++;
    llvm::Value* argsPtr = &*argumento++;

    std::unordered_map<std::string, llvm::Value*> variables;
    bool instancia = !metodo.esEstatico;
    if (instancia)
        variables["este"] = genArgumentoDeArray(argsPtr, nargs, 0, builder, modulo, "este");

    size_t offset = instancia ? 1 : 0;
    for (size_t i = 0; i < metodo.parametros.size(); i++) {
        variables[metodo.parametros[i].nombre] =
            genArgumentoDeArray(argsPtr, nargs, i + offset, builder, modulo, "v_" + metodo.parametros[i].nombre);
    }

    std::set<std::string> excluir;
    if (instancia) excluir.insert("este");
    for (const auto& p : metodo.parametros) excluir.insert(p.nombre);

    std::set<std::string> nombresLocales;
    recolectarVariables(metodo.cuerpo, nombresLocales, excluir);
    auto locales = declararLocales(nombresLocales, builder, modulo);
    variables.insert(locales.begin(), locales.end());

    // Chequeos de tipo de parámetros anotados -- paridad con
    // GeneradorC::genMetodo/genFuncion.
    for (const auto& p : metodo.parametros) {
        if (p.tipo == TipoAnotado::Ninguno) continue;
        llvm::Function* fnVerificar = abi_->declarar(modulo, "lat_verificar_tipo");
        llvm::Value* celdaParam = variables[p.nombre];
        llvm::Value* nombreC = builder.CreateGlobalStringPtr(p.nombre, "nombre_param", 0, &modulo);
        llvm::Value* verificado = builder.CreateAlloca(tipoLatValor, nullptr, "param_verificado");
        builder.CreateCall(fnVerificar, {verificado, celdaParam,
                                         builder.getInt32(tipoAnotadoALatTipo(p.tipo)), nombreC,
                                         builder.getInt32(metodo.linea)});
        builder.CreateStore(builder.CreateLoad(tipoLatValor, verificado), celdaParam);
    }

    std::string anteriorClase = actualClase_;
    std::string anteriorPadre = actualPadre_;
    actualClase_ = claseNombre;
    actualPadre_ = padreNombre;

    llvm::Value* anteriorRetorno = celdaRetorno_;
    celdaRetorno_ = celdaRetorno;
    genBloque(metodo.cuerpo, builder, modulo, variables);
    if (!bloqueTerminado(builder)) {
        llvm::Function* fnNulo = abi_->declarar(modulo, "lat_nulo");
        builder.CreateCall(fnNulo, {celdaRetorno});
        builder.CreateRetVoid();
    }
    celdaRetorno_ = anteriorRetorno;

    actualClase_ = anteriorClase;
    actualPadre_ = anteriorPadre;
}

void GeneradorLLVM::genClase(ClaseDef& c, llvm::Module& modulo) {
    std::string anteriorClase = actualClase_;
    std::string anteriorPadre = actualPadre_;
    actualClase_ = c.nombre;
    actualPadre_ = c.padre;
    for (MetodoDef& metodo : c.metodos)
        genMetodo(c.nombre, metodo, c.padre, modulo);
    actualClase_ = anteriorClase;
    actualPadre_ = anteriorPadre;
}

void GeneradorLLVM::genEstructura(EstructuraDef& e, llvm::Module& modulo) {
    std::string anteriorClase = actualClase_;
    std::string anteriorPadre = actualPadre_;
    actualClase_ = e.nombre;
    actualPadre_.clear();
    for (MetodoDef& metodo : e.metodos)
        genMetodo(e.nombre, metodo, "", modulo);
    actualClase_ = anteriorClase;
    actualPadre_ = anteriorPadre;
}

void GeneradorLLVM::genInterfaz(InterfazDef& /*i*/) {
    // Una interfaz no tiene ningún cuerpo que traducir -- paridad exacta con
    // GeneradorC::genInterfaz.
}

std::unique_ptr<llvm::Module> GeneradorLLVM::generar(Programa& programa) {
    auto modulo = std::make_unique<llvm::Module>("latino_modulo", *contexto_);

    recolectarTipos(programa);

    // Prototipos de las funciones de usuario primero -- permite recursión
    // indirecta (mutua) exactamente igual que el patrón de dos pasadas de
    // GeneradorC::generarCuerpo: si se generara el cuerpo de cada función
    // inmediatamente, una función que llama a otra declarada más adelante en
    // el programa no encontraría todavía su prototipo.
    std::vector<FuncionDef*> funcionesUsuario;
    for (auto& s : programa.sentencias)
        if (auto* f = dynamic_cast<FuncionDef*>(s.get()))
            funcionesUsuario.push_back(f);
    for (FuncionDef* f : funcionesUsuario)
        declararFuncion(*f, *modulo);
    for (FuncionDef* f : funcionesUsuario)
        genFuncion(*f, *modulo);

    for (auto& [nombre, c] : clases_)
        genClase(*c, *modulo);
    for (auto& [nombre, e] : estructuras_)
        genEstructura(*e, *modulo);
    for (auto& [nombre, i] : interfaces_)
        genInterfaz(*i);

    // main(int argc, char** argv) -- equivalente al "int main(int argc,
    // char *argv[])" que emite GeneradorC::generarCuerpo.
    llvm::IRBuilder<> builder(*contexto_);
    llvm::PointerType* tipoPuntero = llvm::PointerType::get(*contexto_, /*AddressSpace=*/0);
    llvm::FunctionType* tipoMain = llvm::FunctionType::get(
        builder.getInt32Ty(), {builder.getInt32Ty(), tipoPuntero}, /*isVarArg=*/false);
    llvm::Function* main = llvm::Function::Create(
        tipoMain, llvm::Function::ExternalLinkage, "main", modulo.get());
    llvm::Value* argc = main->getArg(0);
    llvm::Value* argv = main->getArg(1);
    argc->setName("argc");
    argv->setName("argv");

    llvm::BasicBlock* entrada = llvm::BasicBlock::Create(*contexto_, "entrada", main);
    builder.SetInsertPoint(entrada);

    // lat_set_args(argc, argv) -- paridad con GeneradorC::generarCuerpo.
    llvm::Function* fnSetArgs = abi_->declarar(*modulo, "lat_set_args");
    builder.CreateCall(fnSetArgs, {argc, argv});

    // lat_abi_verificar(tam, alineacion, offset_tipo) -- Decisión 2 del
    // plan: confirma en runtime que el layout de LatValor que Clang derivó
    // en runtime_abi.ll (el que este generador asumió durante toda la
    // traducción) coincide con el que usó el compilador de C que construyó
    // latino.c -- pueden ser toolchains distintos. Los tres valores se leen
    // del mismo DataLayout que ya usa test_codegen_llvm.cpp para esta
    // comprobación (abi_->moduloOrigen(), nunca el del módulo destino, que
    // todavía no tiene DataLayout hasta que invocador_llvm.cpp lo fija).
    llvm::StructType* tipoLatValor = abi_->tipoLatValor();
    const llvm::DataLayout& dl = abi_->moduloOrigen().getDataLayout();
    const llvm::StructLayout* layout = dl.getStructLayout(tipoLatValor);
    llvm::Function* fnAbiVerificar = abi_->declarar(*modulo, "lat_abi_verificar");
    builder.CreateCall(fnAbiVerificar,
                       {llvm::ConstantInt::get(fnAbiVerificar->getArg(0)->getType(),
                                                layout->getSizeInBytes()),
                        llvm::ConstantInt::get(fnAbiVerificar->getArg(1)->getType(),
                                                dl.getABITypeAlign(tipoLatValor).value()),
                        llvm::ConstantInt::get(fnAbiVerificar->getArg(2)->getType(),
                                                layout->getElementOffset(0))});

    // Variables locales de nivel superior + el resto de las sentencias
    // (genBloque/genSentencia ya no traducen Incluir/FuncionDef/ClaseDef/
    // EstructuraDef/InterfazDef -- ver el comentario al final de
    // genSentencia -- así que no hace falta filtrarlas aquí, a diferencia de
    // GeneradorC::generarCuerpo, que sí filtra FuncionDef a mano).
    std::set<std::string> nombresLocales;
    recolectarVariables(programa.sentencias, nombresLocales, {});
    std::unordered_map<std::string, llvm::Value*> variables =
        declararLocales(nombresLocales, builder, *modulo);

    genBloque(programa.sentencias, builder, *modulo, variables);
    if (!bloqueTerminado(builder))
        builder.CreateRet(builder.getInt32(0));

    if (llvm::verifyModule(*modulo, &llvm::errs())) {
        // Indica un bug del propio generador -- los errores de sintaxis/
        // semántica del programa de usuario ya se descartaron antes de
        // llegar aquí.
        return nullptr;
    }

    return modulo;
}
