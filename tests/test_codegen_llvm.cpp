// test_codegen_llvm.cpp — Fases L2, L3 y L4 de input/PLAN_LLVM.md.
//
// Solo se compila/registra cuando LATINO_LLVM_BACKEND está habilitado (ver
// tests/CMakeLists.txt). No compara texto de C como test_codegen.cpp: valida
// el mecanismo de ABI (Fase L2, Decisión 2 del plan) y el esqueleto de
// expresión de GeneradorLLVM (Fases L3-L4) --
//   1. (L2) RuntimeAbiLLVM importa generated/runtime_abi.ll (generado por
//      Clang a partir de tools/abi_probe.c) sin errores.
//   2. (L2) Un módulo que declara funciones del runtime vía RuntimeAbiLLVM y
//      contiene una función trivial que llama a lat_numero/lat_sumar y
//      retorna el resultado pasa llvm::verifyModule.
//   3. (L2) El tamaño/alineación de LatValor calculados por el compilador de
//      C++ del proyecto (sizeof/alignof sobre runtime/latino.h) coinciden
//      con los que Clang le asignó al StructType importado.
//   4. (L3) GeneradorLLVM::genExpr traduce LitNumero/LitCadena/LitLogico/
//      LitNulo a una llamada al constructor de runtime correspondiente, e
//      Identificador a el puntero de su celda ya declarada -- comprobado
//      por subcadena de IR (equivalente a `contiene(...)` de
//      test_codegen.cpp) + `verifyModule` en cada caso.
//   5. (L4) genExpr traduce Binaria/Unaria/PostOperador/Ternaria/
//      AccesoIndice/AccesoMiembro/ListaLiteral/DiccionarioLiteral/VarArgs;
//      declararLocales implementa el hoisting total (alloca en el entry
//      block + lat_nulo()); genAsignacion traduce la sentencia de
//      asignación simple/múltiple/tipada.

#include <iostream>
#include <string>
#include <unordered_map>

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "ast.h"
#include "compiler_llvm.h"
#include "config.h"
#include "recolector_variables.h"
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

static std::string irComoTexto(llvm::Module& modulo) {
    std::string texto;
    llvm::raw_string_ostream flujo(texto);
    modulo.print(flujo, nullptr);
    return texto;
}

static bool contiene(const std::string& t, const std::string& sub) {
    return t.find(sub) != std::string::npos;
}

static void verificarModulo(const std::string& nombre, llvm::Module& modulo) {
    std::string errores;
    llvm::raw_string_ostream flujoErrores(errores);
    bool invalido = llvm::verifyModule(modulo, &flujoErrores);
    CHECK(!invalido, nombre << ": el modulo debe pasar verifyModule: " << flujoErrores.str());
}

// Arma un módulo + función vacía con el builder posicionado en su entry
// block, listo para que un test le pida a GeneradorLLVM::genExpr una
// expresión aislada.
static llvm::Function* prepararFuncionDePrueba(llvm::LLVMContext& contexto, llvm::Module& modulo,
                                                llvm::IRBuilder<>& builder, const char* nombre) {
    llvm::FunctionType* tipoFn = llvm::FunctionType::get(builder.getVoidTy(), /*isVarArg=*/false);
    llvm::Function* fn =
        llvm::Function::Create(tipoFn, llvm::Function::ExternalLinkage, nombre, modulo);
    llvm::BasicBlock* entrada = llvm::BasicBlock::Create(contexto, "entrada", fn);
    builder.SetInsertPoint(entrada);
    return fn;
}

// --- Fase L2: mecanismo de ABI ---------------------------------------------

static void prueba_l2_abi_layout_coincide_con_cxx() {
    llvm::LLVMContext contexto;
    RuntimeAbiLLVM abi(contexto, LATINO_RUNTIME_ABI_LL);

    llvm::StructType* tipoLatValor = abi.tipoLatValor();
    CHECK(tipoLatValor != nullptr, "runtime_abi.ll debe definir %struct.LatValor");
    if (!tipoLatValor) return;

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

static void prueba_l2_declarar_y_llamar_runtime() {
    llvm::LLVMContext contexto;
    RuntimeAbiLLVM abi(contexto, LATINO_RUNTIME_ABI_LL);
    llvm::StructType* tipoLatValor = abi.tipoLatValor();
    if (!tipoLatValor) return;

    llvm::Module modulo("l2_declarar_y_llamar", contexto);
    llvm::Function* fnNumero = abi.declarar(modulo, "lat_numero");
    llvm::Function* fnSumar = abi.declarar(modulo, "lat_sumar");
    CHECK(fnNumero != nullptr, "lat_numero debe existir en runtime_abi.ll");
    CHECK(fnSumar != nullptr, "lat_sumar debe existir en runtime_abi.ll");
    CHECK(modulo.getFunction("lat_numero") == fnNumero,
          "declarar() debe insertar la funcion en el modulo destino");
    if (!fnNumero || !fnSumar) return;

    llvm::IRBuilder<> builder(contexto);
    // Retorna i32 (no void, como prepararFuncionDePrueba): se construye a
    // mano en vez de reusar el helper.
    llvm::FunctionType* tipoTrivial = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function* sumar = llvm::Function::Create(
        tipoTrivial, llvm::Function::ExternalLinkage, "sumar_5_y_3", modulo);
    llvm::BasicBlock* entrada = llvm::BasicBlock::Create(contexto, "entrada", sumar);
    builder.SetInsertPoint(entrada);

    llvm::AllocaInst* a = builder.CreateAlloca(tipoLatValor, nullptr, "a");
    llvm::AllocaInst* b = builder.CreateAlloca(tipoLatValor, nullptr, "b");
    llvm::AllocaInst* c = builder.CreateAlloca(tipoLatValor, nullptr, "c");

    // lat_numero(sret a, 5.0); lat_numero(sret b, 3.0); lat_sumar(sret c, a, b);
    builder.CreateCall(fnNumero, {a, llvm::ConstantFP::get(builder.getDoubleTy(), 5.0)});
    builder.CreateCall(fnNumero, {b, llvm::ConstantFP::get(builder.getDoubleTy(), 3.0)});
    builder.CreateCall(fnSumar, {c, a, b});

    // El campo numérico ('como.numero', un double) vive en el segundo
    // elemento del struct importado -- nunca asumido a mano: es el mismo
    // StructType que Clang derivó para el runtime real.
    llvm::Value* punteroNumero = builder.CreateStructGEP(tipoLatValor, c, 1, "num_ptr");
    llvm::Value* numero = builder.CreateLoad(builder.getDoubleTy(), punteroNumero, "num");
    llvm::Value* numeroI32 = builder.CreateFPToSI(numero, builder.getInt32Ty(), "num_i32");
    builder.CreateRet(numeroI32);

    verificarModulo("l2_declarar_y_llamar", modulo);
}

// --- Fase L3: tipos, literales y esqueleto de expresión --------------------

static void prueba_l3_lit_numero(GeneradorLLVM& gen) {
    llvm::Module modulo("l3_lit_numero", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    LitNumero lit;
    lit.valor = 42.0;
    llvm::Value* celda = gen.genExpr(lit, builder, modulo);
    CHECK(celda != nullptr, "LitNumero debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_numero("), "debe llamar a lat_numero\n" << ir);
    CHECK(contiene(ir, "4.200000e+01"), "debe pasar 42.0 como argumento\n" << ir);
    verificarModulo("l3_lit_numero", modulo);
}

static void prueba_l3_lit_cadena(GeneradorLLVM& gen) {
    llvm::Module modulo("l3_lit_cadena", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    LitCadena lit;
    lit.valor = "hola";
    llvm::Value* celda = gen.genExpr(lit, builder, modulo);
    CHECK(celda != nullptr, "LitCadena debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_cadena("), "debe llamar a lat_cadena\n" << ir);
    CHECK(contiene(ir, "c\"hola\\00\""), "debe incrustar el contenido literal 'hola'\n" << ir);
    verificarModulo("l3_lit_cadena", modulo);
}

static void prueba_l3_lit_logico(GeneradorLLVM& gen) {
    llvm::Module modulo("l3_lit_logico", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    LitLogico litCierto;
    litCierto.valor = true;
    CHECK(gen.genExpr(litCierto, builder, modulo) != nullptr, "LitLogico(cierto) debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_logico(") && contiene(ir, "i32 1"),
          "debe llamar a lat_logico(1) para 'cierto'\n" << ir);
    verificarModulo("l3_lit_logico", modulo);
}

static void prueba_l3_lit_nulo(GeneradorLLVM& gen) {
    llvm::Module modulo("l3_lit_nulo", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    LitNulo lit;
    CHECK(gen.genExpr(lit, builder, modulo) != nullptr, "LitNulo debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_nulo("), "debe llamar a lat_nulo\n" << ir);
    verificarModulo("l3_lit_nulo", modulo);
}

static void prueba_l3_identificador(GeneradorLLVM& gen) {
    llvm::Module modulo("l3_identificador", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaX = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_x");
    std::unordered_map<std::string, llvm::Value*> variables{{"x", celdaX}};

    Identificador id;
    id.nombre = "x";
    llvm::Value* resultado = gen.genExpr(id, builder, modulo, variables);
    CHECK(resultado == celdaX,
          "Identificador debe devolver el puntero de su celda ya declarada, sin CreateLoad");

    Identificador desconocido;
    desconocido.nombre = "no_existe";
    CHECK(gen.genExpr(desconocido, builder, modulo, variables) == nullptr,
          "un identificador no declarado debe devolver nullptr");

    builder.CreateRetVoid();
    verificarModulo("l3_identificador", modulo);
}

// --- Fase L4: expresiones compuestas, hoisting y asignación ---------------

static ExprPtr litNumero(double v) {
    auto n = std::make_unique<LitNumero>();
    n->valor = v;
    return n;
}

static ExprPtr identificador(const std::string& nombre) {
    auto id = std::make_unique<Identificador>();
    id->nombre = nombre;
    return id;
}

static void prueba_l4_binaria(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_binaria", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    Binaria bin;
    bin.op = "+";
    bin.izq = litNumero(2);
    bin.der = litNumero(3);
    llvm::Value* celda = gen.genExpr(bin, builder, modulo);
    CHECK(celda != nullptr, "Binaria debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_sumar("), "'+' debe llamar a lat_sumar\n" << ir);
    verificarModulo("l4_binaria", modulo);
}

static void prueba_l4_binaria_op_desconocido(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_binaria_desconocido", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    Binaria bin;
    bin.op = "###";
    bin.izq = litNumero(1);
    bin.der = litNumero(1);
    CHECK(gen.genExpr(bin, builder, modulo) == nullptr,
          "un operador binario desconocido debe devolver nullptr");
}

static void prueba_l4_unaria(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_unaria", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    Unaria neg;
    neg.op = "-";
    neg.operando = litNumero(5);
    CHECK(gen.genExpr(neg, builder, modulo) != nullptr, "Unaria debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_negar("), "'-x' debe llamar a lat_negar\n" << ir);
    verificarModulo("l4_unaria", modulo);
}

static void prueba_l4_post_operador(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_post_operador", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaX = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_x");
    std::unordered_map<std::string, llvm::Value*> variables{{"x", celdaX}};

    PostOperador incr;
    incr.op = "++";
    incr.operando = identificador("x");
    llvm::Value* resultado = gen.genExpr(incr, builder, modulo, variables);
    CHECK(resultado == celdaX,
          "i++ debe devolver la MISMA celda de la variable (ya actualizada), no una copia");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_sumar("), "i++ debe llamar a lat_sumar\n" << ir);
    CHECK(contiene(ir, "call void @lat_numero("), "i++ debe construir el literal 1\n" << ir);
    verificarModulo("l4_post_operador", modulo);
}

static void prueba_l4_ternaria(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_ternaria", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    Ternaria tern;
    tern.condicion = std::make_unique<LitLogico>();
    static_cast<LitLogico&>(*tern.condicion).valor = true;
    tern.siCierto = litNumero(1);
    tern.siFalso = litNumero(2);
    llvm::Value* resultado = gen.genExpr(tern, builder, modulo);
    CHECK(resultado != nullptr, "Ternaria debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call i32 @lat_es_verdadero("), "debe evaluar la condicion\n" << ir);
    CHECK(contiene(ir, "tern_cierto:") && contiene(ir, "tern_falso:") && contiene(ir, "tern_fin:"),
          "debe emitir basic blocks separados por rama (evaluacion perezosa)\n" << ir);
    verificarModulo("l4_ternaria", modulo);
}

static void prueba_l4_acceso_indice(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_acceso_indice", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaLista = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_lista");
    std::unordered_map<std::string, llvm::Value*> variables{{"lista", celdaLista}};

    AccesoIndice acceso;
    acceso.objeto = identificador("lista");
    acceso.indice = litNumero(0);
    CHECK(gen.genExpr(acceso, builder, modulo, variables) != nullptr,
          "AccesoIndice debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_obtener_indice("),
          "objeto[indice] debe llamar a lat_obtener_indice\n" << ir);
    verificarModulo("l4_acceso_indice", modulo);
}

static void prueba_l4_acceso_miembro(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_acceso_miembro", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaObj = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_obj");
    std::unordered_map<std::string, llvm::Value*> variables{{"obj", celdaObj}};

    AccesoMiembro acceso;
    acceso.objeto = identificador("obj");
    acceso.miembro = "campo";
    CHECK(gen.genExpr(acceso, builder, modulo, variables) != nullptr,
          "AccesoMiembro debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "c\"campo\\00\""), "debe incrustar el nombre del miembro\n" << ir);
    CHECK(contiene(ir, "call void @lat_cadena("), "debe construir la clave como cadena\n" << ir);
    CHECK(contiene(ir, "call void @lat_obtener_indice("),
          "objeto.miembro debe llamar a lat_obtener_indice\n" << ir);
    verificarModulo("l4_acceso_miembro", modulo);
}

static void prueba_l4_lista_literal(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_lista_literal", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    ListaLiteral lista;
    lista.elementos.push_back(litNumero(1));
    lista.elementos.push_back(litNumero(2));
    CHECK(gen.genExpr(lista, builder, modulo) != nullptr, "ListaLiteral debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "@lat_lista_de("), "[a, b] debe llamar a lat_lista_de\n" << ir);
    CHECK(contiene(ir, "i64 2"), "debe pasar el conteo de elementos\n" << ir);
    verificarModulo("l4_lista_literal", modulo);
}

static void prueba_l4_lista_literal_resto(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_lista_literal_resto", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaResto = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "lat_resto");
    std::unordered_map<std::string, llvm::Value*> variables{{"lat_resto", celdaResto}};

    ListaLiteral lista;
    lista.elementos.push_back(std::make_unique<VarArgs>());
    llvm::Value* resultado = gen.genExpr(lista, builder, modulo, variables);
    CHECK(resultado == celdaResto,
          "[...] debe devolver directamente la celda de 'lat_resto', sin llamar a lat_lista_de");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(!contiene(ir, "lat_lista_de"), "[...] no debe llamar a lat_lista_de\n" << ir);
    verificarModulo("l4_lista_literal_resto", modulo);
}

static void prueba_l4_dic_literal(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_dic_literal", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    DiccionarioLiteral dic;
    ParDic par;
    par.clave = std::make_unique<LitCadena>();
    static_cast<LitCadena&>(*par.clave).valor = "a";
    par.valor = litNumero(1);
    dic.pares.push_back(std::move(par));
    CHECK(gen.genExpr(dic, builder, modulo) != nullptr, "DiccionarioLiteral debe generar un valor");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "@lat_dic_de("), "{a: 1} debe llamar a lat_dic_de\n" << ir);
    CHECK(contiene(ir, "i64 1"), "debe pasar el conteo de pares\n" << ir);
    verificarModulo("l4_dic_literal", modulo);
}

static void prueba_l4_declarar_locales(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_declarar_locales", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    auto asign = std::make_unique<Asignacion>();
    asign->destinos.push_back(identificador("x"));
    asign->valores.push_back(litNumero(1));
    ListaSent cuerpo;
    cuerpo.push_back(std::move(asign));

    std::set<std::string> nombres;
    recolectarVariables(cuerpo, nombres, {});
    CHECK(nombres.size() == 1 && nombres.count("x") == 1,
          "recolectarVariables debe encontrar 'x' en el cuerpo");

    auto variables = gen.declararLocales(nombres, builder, modulo);
    CHECK(variables.count("x") == 1, "declararLocales debe declarar una celda para 'x'");
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "alloca %struct.LatValor"), "debe emitir un alloca de %struct.LatValor\n" << ir);
    CHECK(contiene(ir, "call void @lat_nulo("), "debe inicializar la celda con lat_nulo()\n" << ir);
    verificarModulo("l4_declarar_locales", modulo);
}

static void prueba_l4_asignacion_simple(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_asignacion_simple", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaX = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_x");
    std::unordered_map<std::string, llvm::Value*> variables{{"x", celdaX}};

    Asignacion asign;
    asign.destinos.push_back(identificador("x"));
    asign.valores.push_back(litNumero(42));
    gen.genAsignacion(asign, builder, modulo, variables);
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_numero("), "debe evaluar el literal 42\n" << ir);
    CHECK(contiene(ir, "store %struct.LatValor"), "debe copiar el valor a la celda de 'x'\n" << ir);
    verificarModulo("l4_asignacion_simple", modulo);
}

static void prueba_l4_asignacion_tipada(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_asignacion_tipada", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaX = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_x");
    std::unordered_map<std::string, llvm::Value*> variables{{"x", celdaX}};

    Asignacion asign;
    asign.linea = 7;
    asign.destinos.push_back(identificador("x"));
    asign.valores.push_back(litNumero(5));
    asign.tiposDestino.push_back(TipoAnotado::Numero);
    gen.genAsignacion(asign, builder, modulo, variables);
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    CHECK(contiene(ir, "call void @lat_verificar_tipo("),
          "una asignacion con tipo anotado debe llamar a lat_verificar_tipo\n" << ir);
    CHECK(contiene(ir, "i32 " + std::to_string(static_cast<int>(LAT_NUMERO))),
          "debe pasar LAT_NUMERO como tipo esperado\n" << ir);
    CHECK(contiene(ir, "i32 7"), "debe pasar el numero de linea\n" << ir);
    verificarModulo("l4_asignacion_tipada", modulo);
}

static void prueba_l4_asignacion_multiple(GeneradorLLVM& gen) {
    llvm::Module modulo("l4_asignacion_multiple", gen.contexto());
    llvm::IRBuilder<> builder(gen.contexto());
    prepararFuncionDePrueba(gen.contexto(), modulo, builder, "f");

    llvm::Value* celdaA = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_a");
    llvm::Value* celdaB = builder.CreateAlloca(gen.abi().tipoLatValor(), nullptr, "v_b");
    std::unordered_map<std::string, llvm::Value*> variables{{"a", celdaA}, {"b", celdaB}};

    // a, b = b, a  -- ejercita el orden "evaluar todo antes de asignar nada"
    // (si se asignara en el mismo orden en que se evalua, la segunda
    // asignacion leeria el valor ya sobrescrito de 'a').
    Asignacion asign;
    asign.destinos.push_back(identificador("a"));
    asign.destinos.push_back(identificador("b"));
    asign.valores.push_back(identificador("b"));
    asign.valores.push_back(identificador("a"));
    gen.genAsignacion(asign, builder, modulo, variables);
    builder.CreateRetVoid();

    std::string ir = irComoTexto(modulo);
    // LLVM sufija con "1" el segundo alloca que pide el mismo nombre base
    // dentro de la misma función -- su presencia confirma que se crearon DOS
    // celdas temporales distintas (una por valor) antes de asignar ningún
    // destino, no una sola reutilizada.
    CHECK(contiene(ir, "%asig_tmp =") && contiene(ir, "%asig_tmp1 ="),
          "debe evaluar ambos valores a celdas temporales distintas antes de asignar\n" << ir);
    verificarModulo("l4_asignacion_multiple", modulo);
}

int main() {
    CHECK(std::string(LATINO_RUNTIME_ABI_LL) != "",
          "config.h debe traer una ruta a runtime_abi.ll cuando LATINO_LLVM_BACKEND esta ON");

    prueba_l2_abi_layout_coincide_con_cxx();
    prueba_l2_declarar_y_llamar_runtime();

    GeneradorLLVM gen;
    prueba_l3_lit_numero(gen);
    prueba_l3_lit_cadena(gen);
    prueba_l3_lit_logico(gen);
    prueba_l3_lit_nulo(gen);
    prueba_l3_identificador(gen);

    prueba_l4_binaria(gen);
    prueba_l4_binaria_op_desconocido(gen);
    prueba_l4_unaria(gen);
    prueba_l4_post_operador(gen);
    prueba_l4_ternaria(gen);
    prueba_l4_acceso_indice(gen);
    prueba_l4_acceso_miembro(gen);
    prueba_l4_lista_literal(gen);
    prueba_l4_lista_literal_resto(gen);
    prueba_l4_dic_literal(gen);
    prueba_l4_declarar_locales(gen);
    prueba_l4_asignacion_simple(gen);
    prueba_l4_asignacion_tipada(gen);
    prueba_l4_asignacion_multiple(gen);

    std::cout << "\nComprobaciones: " << g_checks << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE CODEGEN LLVM (FASES L2-L4) PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
