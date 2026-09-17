// test_genericos.cpp
//
// Pruebas de genéricos al estilo de Rust (PLAN_GENERICOS.md).
// Cubre: parser (<T>, bounds, "donde", turbofish "::<...>", "nuevo Clase<...>"),
// análisis semántico (ámbito de T, inferencia, bounds) y generación de código C
// (erasure: un parámetro genérico nunca recibe un chequeo de runtime).

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"

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

// ---------------------------------------------------------------------------
// Utilidades comunes
// ---------------------------------------------------------------------------
static bool contiene(const std::string& t, const std::string& sub) {
    return t.find(sub) != std::string::npos;
}

static std::unique_ptr<Programa> parsear(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    return parser.parse();
}

static bool analizar(const std::string& src, std::string& mensajes) {
    auto prog = parsear(src);
    if (!prog) { mensajes = "<error de sintaxis>"; return false; }
    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    AnalizadorSemantico sem;
    bool ok = sem.analizar(*prog);
    std::cerr.rdbuf(viejo);
    mensajes = cap.str();
    return ok;
}

static std::string generar(const std::string& src) {
    auto prog = parsear(src);
    if (!prog) return "<nullptr>";
    GeneradorC gen;
    return gen.generar(*prog);
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

static void prueba_parser_funcion_generica_simple() {
    auto prog = parsear(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* f = dynamic_cast<FuncionDef*>(prog->sentencias[0].get());
    CHECK(f != nullptr, "es una FuncionDef");
    if (!f) return;
    CHECK(f->genericos.size() == 1 && f->genericos[0].nombre == "T", "genericos == [T]");
    CHECK(f->genericos[0].bounds.empty(), "T sin bounds");
    CHECK(f->parametros.size() == 1 && f->parametros[0].tipo == TipoAnotado::Objeto &&
              f->parametros[0].tipoClase == "T",
          "parametro x: T se representa como Objeto/tipoClase=T");
    CHECK(f->tipoRetornoClase == "T", "retorno T");
}

static void prueba_parser_bounds_simples_y_multiples() {
    auto prog = parsear(
        "interfaz Comparable\n"
        "    funcion compararCon(otroValor: Comparable): numero\n"
        "fin\n"
        "interfaz Imprimible\n"
        "    funcion aCadena(): cadena\n"
        "fin\n"
        "funcion maximo<T: Comparable + Imprimible>(a: T, b: T): T\n"
        "    retornar a\n"
        "fin\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog) return;
    auto* f = dynamic_cast<FuncionDef*>(prog->sentencias[2].get());
    CHECK(f != nullptr, "es una FuncionDef");
    if (!f) return;
    CHECK(f->genericos.size() == 1, "un parametro generico");
    CHECK(f->genericos[0].bounds.size() == 2, "dos bounds");
    if (f->genericos[0].bounds.size() == 2) {
        CHECK(f->genericos[0].bounds[0] == "Comparable", "primer bound Comparable");
        CHECK(f->genericos[0].bounds[1] == "Imprimible", "segundo bound Imprimible");
    }
}

static void prueba_parser_clausula_donde() {
    auto prog = parsear(
        "interfaz Comparable\n"
        "    funcion compararCon(otroValor: Comparable): numero\n"
        "fin\n"
        "funcion maximo<T>(a: T, b: T): T donde T: Comparable\n"
        "    retornar a\n"
        "fin\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog) return;
    auto* f = dynamic_cast<FuncionDef*>(prog->sentencias[1].get());
    CHECK(f != nullptr, "es una FuncionDef");
    if (!f) return;
    CHECK(f->genericos.size() == 1 && f->genericos[0].bounds.size() == 1 &&
              f->genericos[0].bounds[0] == "Comparable",
          "'donde' fusiona el bound con el <T> ya declarado");
}

static void prueba_parser_clase_generica() {
    auto prog = parsear(
        "clase Pila<T>\n"
        "    privado tope: T\n"
        "    funcion Pila()\n"
        "    fin\n"
        "fin\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* c = dynamic_cast<ClaseDef*>(prog->sentencias[0].get());
    CHECK(c != nullptr, "es una ClaseDef");
    if (!c) return;
    CHECK(c->genericos.size() == 1 && c->genericos[0].nombre == "T", "genericos == [T]");
    CHECK(c->campos.size() == 1 && c->campos[0].tipoClase == "T", "campo tope: T");
}

static void prueba_parser_nuevo_con_argumentos_de_tipo() {
    auto prog = parsear(
        "clase Pila<T>\n"
        "    funcion Pila()\n"
        "    fin\n"
        "fin\n"
        "p = nuevo Pila<numero>()\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.size() < 2) return;
    auto* a = dynamic_cast<Asignacion*>(prog->sentencias[1].get());
    CHECK(a != nullptr, "es una Asignacion");
    if (!a || a->valores.empty()) return;
    auto* n = dynamic_cast<NuevoExpr*>(a->valores[0].get());
    CHECK(n != nullptr, "el valor es un NuevoExpr");
    if (!n) return;
    CHECK(n->clase == "Pila", "clase == Pila");
    CHECK(n->tipoArgs.size() == 1 && n->tipoArgs[0] == "numero", "tipoArgs == [numero]");
}

static void prueba_parser_turbofish() {
    auto prog = parsear(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "escribir(identidad::<cadena>(\"hola\"))\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.size() < 2) return;
    auto* es = dynamic_cast<ExprSentencia*>(prog->sentencias[1].get());
    CHECK(es != nullptr, "es una ExprSentencia");
    if (!es) return;
    auto* llamadaEscribir = dynamic_cast<Llamada*>(es->expr.get());
    CHECK(llamadaEscribir != nullptr && llamadaEscribir->argumentos.size() == 1,
          "escribir(...) con un argumento");
    if (!llamadaEscribir || llamadaEscribir->argumentos.empty()) return;
    auto* llamadaIdentidad = dynamic_cast<Llamada*>(llamadaEscribir->argumentos[0].get());
    CHECK(llamadaIdentidad != nullptr, "el argumento es la llamada a identidad");
    if (!llamadaIdentidad) return;
    CHECK(llamadaIdentidad->tipoArgsExplicitos.size() == 1 &&
              llamadaIdentidad->tipoArgsExplicitos[0] == "cadena",
          "tipoArgsExplicitos == [cadena] (turbofish)");
}

static void prueba_parser_variable_anotada_con_generico_no_falla() {
    // PLAN_GENERICOS.md: "p: Pila<numero> = ..." debe parsear sin error, aunque
    // Asignacion no guarde el nombre de clase (limitación preexistente, ver el plan).
    auto prog = parsear(
        "clase Pila<T>\n"
        "    funcion Pila()\n"
        "    fin\n"
        "fin\n"
        "p: Pila<numero> = nuevo Pila<numero>()\n");
    CHECK(prog != nullptr, "parsea 'p: Pila<numero> = ...' sin error");
}

// ---------------------------------------------------------------------------
// Análisis semántico
// ---------------------------------------------------------------------------

static void prueba_semantico_T_valido_en_su_propio_ambito() {
    std::string msg;
    bool ok = analizar(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "escribir(identidad(5))\n",
        msg);
    CHECK(ok, "sin errores: " + msg);
}

static void prueba_semantico_T_invalido_fuera_de_ambito() {
    std::string msg;
    bool ok = analizar(
        "funcion f(x: T): numero\n"
        "    retornar 0\n"
        "fin\n",
        msg);
    CHECK(!ok, "error: T no está declarado como generico en esta funcion");
    CHECK(contiene(msg, "tipo de objeto desconocido 'T'"), "mensaje esperado: " + msg);
}

static void prueba_semantico_bound_interfaz_desconocida() {
    std::string msg;
    bool ok = analizar(
        "funcion f<T: NoExiste>(x: T): T\n"
        "    retornar x\n"
        "fin\n",
        msg);
    CHECK(!ok, "error: bound desconocido");
    CHECK(contiene(msg, "restricción genérica desconocida"), "mensaje esperado: " + msg);
}

static void prueba_semantico_bound_no_es_interfaz() {
    std::string msg;
    bool ok = analizar(
        "clase Animal\n"
        "    funcion Animal()\n"
        "    fin\n"
        "fin\n"
        "funcion f<T: Animal>(x: T): T\n"
        "    retornar x\n"
        "fin\n",
        msg);
    CHECK(!ok, "error: Animal no es una interfaz");
    CHECK(contiene(msg, "no es una interfaz"), "mensaje esperado: " + msg);
}

static void prueba_semantico_inferencia_simple_ok() {
    std::string msg;
    bool ok = analizar(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "escribir(identidad(5))\n"
        "escribir(identidad(\"hola\"))\n",
        msg);
    CHECK(ok, "sin errores: " + msg);
}

static void prueba_semantico_inferencia_conflicto() {
    std::string msg;
    bool ok = analizar(
        "funcion par<T>(a: T, b: T): T\n"
        "    retornar a\n"
        "fin\n"
        "escribir(par(5, \"x\"))\n",
        msg);
    CHECK(!ok, "error: T no puede ser numero y cadena a la vez");
    CHECK(contiene(msg, "se infirió como 'numero'"), "mensaje esperado: " + msg);
}

static void prueba_semantico_inferencia_variable_anotada() {
    std::string msg;
    bool ok = analizar(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "n: numero = 5\n"
        "escribir(identidad(n))\n",
        msg);
    CHECK(ok, "infiere T=numero desde la anotacion de la variable: " + msg);
}

static void prueba_semantico_falta_turbofish() {
    std::string msg;
    bool ok = analizar(
        "funcion vacio<T>(): T\n"
        "    retornar nulo\n"
        "fin\n"
        "escribir(vacio())\n",
        msg);
    CHECK(!ok, "error: T solo aparece en el retorno, requiere turbofish");
    CHECK(contiene(msg, "no se puede inferir el tipo genérico 'T'"), "mensaje esperado: " + msg);
}

static void prueba_semantico_turbofish_arity() {
    std::string msg;
    bool ok = analizar(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "escribir(identidad::<numero, cadena>(5))\n",
        msg);
    CHECK(!ok, "error: identidad solo tiene un parametro generico");
}

static void prueba_semantico_bound_satisfecho_ok() {
    std::string msg;
    bool ok = analizar(
        "interfaz Comparable\n"
        "    funcion compararCon(otroValor: Comparable): numero\n"
        "fin\n"
        "clase Entero implementa Comparable\n"
        "    funcion Entero()\n"
        "    fin\n"
        "    publico funcion compararCon(otroValor: Comparable): numero\n"
        "        retornar 0\n"
        "    fin\n"
        "fin\n"
        "funcion identidadAcotada<T: Comparable>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "escribir(identidadAcotada::<Entero>(nuevo Entero()))\n",
        msg);
    CHECK(ok, "Entero implementa Comparable: sin errores: " + msg);
}

static void prueba_semantico_bound_no_satisfecho() {
    std::string msg;
    bool ok = analizar(
        "interfaz Comparable\n"
        "    funcion compararCon(otroValor: Comparable): numero\n"
        "fin\n"
        "clase Circulo\n"
        "    funcion Circulo()\n"
        "    fin\n"
        "fin\n"
        "funcion identidadAcotada<T: Comparable>(x: T): T\n"
        "    retornar x\n"
        "fin\n"
        "escribir(identidadAcotada::<Circulo>(nuevo Circulo()))\n",
        msg);
    CHECK(!ok, "error: Circulo no implementa Comparable");
    CHECK(contiene(msg, "'Circulo' no implementa 'Comparable'"), "mensaje esperado: " + msg);
}

static void prueba_semantico_bound_sobre_primitivo() {
    std::string msg;
    bool ok = analizar(
        "interfaz Comparable\n"
        "    funcion compararCon(otroValor: Comparable): numero\n"
        "fin\n"
        "funcion maximo<T: Comparable>(a: T, b: T): T\n"
        "    retornar a\n"
        "fin\n"
        "escribir(maximo(5, 6))\n",
        msg);
    CHECK(!ok, "error: numero no puede satisfacer un bound de interfaz");
    CHECK(contiene(msg, "los tipos primitivos no pueden satisfacer"), "mensaje esperado: " + msg);
}

static void prueba_semantico_nuevo_arity_genericos() {
    std::string msg;
    bool ok = analizar(
        "clase Pila<T>\n"
        "    funcion Pila()\n"
        "    fin\n"
        "fin\n"
        "p = nuevo Pila<numero, cadena>()\n",
        msg);
    CHECK(!ok, "error: Pila solo tiene un parametro generico");
    CHECK(contiene(msg, "espera 1 argumento(s) de tipo genérico"), "mensaje esperado: " + msg);
}

static void prueba_semantico_nuevo_sin_argumentos_de_tipo_ok() {
    std::string msg;
    bool ok = analizar(
        "clase Pila<T>\n"
        "    funcion Pila()\n"
        "    fin\n"
        "fin\n"
        "p = nuevo Pila()\n",
        msg);
    CHECK(ok, "sin <...> se permite (degradacion gradual): " + msg);
}

static void prueba_semantico_nuevo_bound_no_satisfecho() {
    std::string msg;
    bool ok = analizar(
        "interfaz Comparable\n"
        "    funcion compararCon(otroValor: Comparable): numero\n"
        "fin\n"
        "clase Circulo\n"
        "    funcion Circulo()\n"
        "    fin\n"
        "fin\n"
        "clase Caja<T: Comparable>\n"
        "    funcion Caja()\n"
        "    fin\n"
        "fin\n"
        "c = nuevo Caja<Circulo>()\n",
        msg);
    CHECK(!ok, "error: Circulo no implementa Comparable, requerido por Caja<T>");
    CHECK(contiene(msg, "no implementa 'Comparable'"), "mensaje esperado: " + msg);
}

static void prueba_semantico_metodo_generico_propio_de_clase() {
    // El campo/parametro tipado "T" dentro de la propia clase Pila<T> nunca
    // debe reportarse como "tipo de objeto desconocido".
    std::string msg;
    bool ok = analizar(
        "clase Pila<T>\n"
        "    privado tope: T\n"
        "    funcion Pila()\n"
        "    fin\n"
        "    publico funcion cima(): T\n"
        "        retornar este.tope\n"
        "    fin\n"
        "    publico funcion apilar(valor: T)\n"
        "        este.tope = valor\n"
        "    fin\n"
        "fin\n",
        msg);
    CHECK(ok, "sin errores: " + msg);
}

// ---------------------------------------------------------------------------
// Generación de código — erasure
// ---------------------------------------------------------------------------

static void prueba_codegen_funcion_generica_sin_chequeo_runtime() {
    std::string c = generar(
        "funcion identidad<T>(x: T): T\n"
        "    retornar x\n"
        "fin\n");
    CHECK(!contiene(c, "lat_verificar_tipo"), "un parametro T no emite chequeo de runtime: " + c);
}

static void prueba_codegen_funcion_generica_y_no_generica_mezcladas() {
    // El parametro genérico T no se verifica, pero un parametro con tipo fijo
    // en la MISMA función sigue verificándose (erasure es por-parámetro, no
    // deshabilita el tipado gradual del resto de la firma).
    std::string c = generar(
        "funcion f<T>(x: T, n: numero): T\n"
        "    retornar x\n"
        "fin\n");
    CHECK(!contiene(c, "LAT_OBJETO"), "no hay chequeo LAT_OBJETO para T: " + c);
    CHECK(contiene(c, "lat_verificar_tipo") && contiene(c, "LAT_NUMERO"),
          "el parametro no generico 'n: numero' sigue verificandose: " + c);
}

static void prueba_codegen_metodo_generico_de_clase_sin_chequeo() {
    std::string c = generar(
        "clase Pila<T>\n"
        "    funcion Pila()\n"
        "    fin\n"
        "    publico funcion apilar(valor: T)\n"
        "    fin\n"
        "fin\n");
    CHECK(!contiene(c, "LAT_OBJETO"), "el parametro T de un metodo de Pila<T> no se verifica: " + c);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== PRUEBAS DE GENERICOS (PLAN_GENERICOS.md) ===\n\n";

    std::cout << "-- Parser --\n";
    prueba_parser_funcion_generica_simple();
    prueba_parser_bounds_simples_y_multiples();
    prueba_parser_clausula_donde();
    prueba_parser_clase_generica();
    prueba_parser_nuevo_con_argumentos_de_tipo();
    prueba_parser_turbofish();
    prueba_parser_variable_anotada_con_generico_no_falla();

    std::cout << "-- Análisis semántico --\n";
    prueba_semantico_T_valido_en_su_propio_ambito();
    prueba_semantico_T_invalido_fuera_de_ambito();
    prueba_semantico_bound_interfaz_desconocida();
    prueba_semantico_bound_no_es_interfaz();
    prueba_semantico_inferencia_simple_ok();
    prueba_semantico_inferencia_conflicto();
    prueba_semantico_inferencia_variable_anotada();
    prueba_semantico_falta_turbofish();
    prueba_semantico_turbofish_arity();
    prueba_semantico_bound_satisfecho_ok();
    prueba_semantico_bound_no_satisfecho();
    prueba_semantico_bound_sobre_primitivo();
    prueba_semantico_nuevo_arity_genericos();
    prueba_semantico_nuevo_sin_argumentos_de_tipo_ok();
    prueba_semantico_nuevo_bound_no_satisfecho();
    prueba_semantico_metodo_generico_propio_de_clase();

    std::cout << "-- Generación de código (erasure) --\n";
    prueba_codegen_funcion_generica_sin_chequeo_runtime();
    prueba_codegen_funcion_generica_y_no_generica_mezcladas();
    prueba_codegen_metodo_generico_de_clase_sin_chequeo();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << "\n";
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE GENERICOS PASARON.\n";
    return g_fallos == 0 ? 0 : 1;
}
