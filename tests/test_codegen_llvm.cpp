// test_codegen_llvm.cpp — Fases L2 y L3 de input/PLAN_LLVM.md.
//
// Solo se compila/registra cuando LATINO_LLVM_BACKEND está habilitado (ver
// tests/CMakeLists.txt). No compara texto de C como test_codegen.cpp: valida
// el mecanismo de ABI (Fase L2, Decisión 2 del plan) y el esqueleto de
// expresión de GeneradorLLVM (Fase L3) --
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

    std::cout << "\nComprobaciones: " << g_checks << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE CODEGEN LLVM (FASES L2-L3) PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
