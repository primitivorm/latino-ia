// test_tipado.cpp
//
// Pruebas del sistema de tipado gradual opcional (Fase 27).
// Cubre: parser (anotaciones en AST), análisis semántico (verificación estática)
// y generación de código C (emisión de lat_verificar_tipo).

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
// Pruebas de parser — las anotaciones se almacenan correctamente en el AST
// ---------------------------------------------------------------------------

static void prueba_parser_tipo_numero() {
    auto prog = parsear("n: numero = 42\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* a = dynamic_cast<Asignacion*>(prog->sentencias[0].get());
    CHECK(a != nullptr, "es una Asignacion");
    if (!a) return;
    CHECK(a->tiposDestino.size() == 1 && a->tiposDestino[0] == TipoAnotado::Numero,
          "tipo anotado es Numero");
}

static void prueba_parser_tipo_cadena() {
    auto prog = parsear("s: cadena = \"hola\"\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* a = dynamic_cast<Asignacion*>(prog->sentencias[0].get());
    CHECK(a != nullptr, "es una Asignacion");
    if (!a) return;
    CHECK(a->tiposDestino.size() == 1 && a->tiposDestino[0] == TipoAnotado::Cadena,
          "tipo anotado es Cadena");
}

static void prueba_parser_tipo_logico() {
    auto prog = parsear("b: logico = cierto\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* a = dynamic_cast<Asignacion*>(prog->sentencias[0].get());
    CHECK(a != nullptr, "es una Asignacion");
    if (!a) return;
    CHECK(a->tiposDestino.size() == 1 && a->tiposDestino[0] == TipoAnotado::Logico,
          "tipo anotado es Logico");
}

static void prueba_parser_tipo_lista() {
    auto prog = parsear("lst: lista = [1, 2, 3]\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* a = dynamic_cast<Asignacion*>(prog->sentencias[0].get());
    CHECK(a != nullptr, "es una Asignacion");
    if (!a) return;
    CHECK(a->tiposDestino.size() == 1 && a->tiposDestino[0] == TipoAnotado::Lista,
          "tipo anotado es Lista");
}

static void prueba_parser_sin_anotacion_retrocompat() {
    // Una asignación sin ":" no debe tener tiposDestino
    auto prog = parsear("n = 42\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* a = dynamic_cast<Asignacion*>(prog->sentencias[0].get());
    CHECK(a != nullptr, "es una Asignacion");
    if (!a) return;
    CHECK(a->tiposDestino.empty(), "tiposDestino vacío (sin anotación)");
}

static void prueba_parser_funcion_tipos_parametros() {
    auto prog = parsear(
        "funcion suma(a: numero, b: numero): numero\n"
        "    retornar a + b\n"
        "fin\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* f = dynamic_cast<FuncionDef*>(prog->sentencias[0].get());
    CHECK(f != nullptr, "es una FuncionDef");
    if (!f) return;
    CHECK(f->parametros.size() == 2, "dos parámetros");
    if (f->parametros.size() == 2) {
        CHECK(f->parametros[0].nombre == "a" && f->parametros[0].tipo == TipoAnotado::Numero,
              "primer parámetro: a: numero");
        CHECK(f->parametros[1].nombre == "b" && f->parametros[1].tipo == TipoAnotado::Numero,
              "segundo parámetro: b: numero");
    }
    CHECK(f->tipoRetorno == TipoAnotado::Numero, "tipo de retorno: numero");
}

static void prueba_parser_funcion_sin_tipos_retrocompat() {
    auto prog = parsear(
        "funcion suma(a, b)\n"
        "    retornar a + b\n"
        "fin\n");
    CHECK(prog != nullptr, "parsea sin error");
    if (!prog || prog->sentencias.empty()) return;
    auto* f = dynamic_cast<FuncionDef*>(prog->sentencias[0].get());
    CHECK(f != nullptr, "es una FuncionDef");
    if (!f) return;
    CHECK(f->parametros.size() == 2, "dos parámetros");
    if (f->parametros.size() == 2) {
        CHECK(f->parametros[0].tipo == TipoAnotado::Ninguno, "a sin tipo");
        CHECK(f->parametros[1].tipo == TipoAnotado::Ninguno, "b sin tipo");
    }
    CHECK(f->tipoRetorno == TipoAnotado::Ninguno, "sin tipo de retorno");
}

// ---------------------------------------------------------------------------
// Pruebas semánticas — errores en compilación con literales incompatibles
// ---------------------------------------------------------------------------

static void prueba_semantico_tipo_incompatible_numero_cadena() {
    std::string msg;
    bool ok = analizar("n: numero = \"hola\"\n", msg);
    CHECK(!ok, "debe fallar el análisis");
    CHECK(contiene(msg, "tipo incompatible"),
          "mensaje contiene 'tipo incompatible' (fue: " + msg + ")");
    CHECK(contiene(msg, "numero"), "menciona el tipo declarado 'numero'");
    CHECK(contiene(msg, "cadena"), "menciona el tipo recibido 'cadena'");
}

static void prueba_semantico_tipo_incompatible_logico_numero() {
    std::string msg;
    bool ok = analizar("b: logico = 42\n", msg);
    CHECK(!ok, "debe fallar el análisis");
    CHECK(contiene(msg, "tipo incompatible"), "mensaje contiene 'tipo incompatible'");
}

static void prueba_semantico_tipo_incompatible_lista_dic() {
    std::string msg;
    bool ok = analizar("lst: lista = {\"a\": 1}\n", msg);
    CHECK(!ok, "debe fallar el análisis");
    CHECK(contiene(msg, "tipo incompatible"), "mensaje contiene 'tipo incompatible'");
}

static void prueba_semantico_tipo_correcto_numero() {
    std::string msg;
    bool ok = analizar("n: numero = 42\n", msg);
    CHECK(ok, "número anotado como numero debe pasar (errores: " + msg + ")");
}

static void prueba_semantico_tipo_correcto_cadena() {
    std::string msg;
    bool ok = analizar("s: cadena = \"hola\"\n", msg);
    CHECK(ok, "cadena anotada como cadena debe pasar (errores: " + msg + ")");
}

static void prueba_semantico_tipo_correcto_lista() {
    std::string msg;
    bool ok = analizar("lst: lista = [1, 2, 3]\n", msg);
    CHECK(ok, "lista anotada como lista debe pasar (errores: " + msg + ")");
}

static void prueba_semantico_expresion_dinamica_no_falla() {
    // Una expresión dinámica (llamada a función) no se verifica estáticamente.
    std::string msg;
    bool ok = analizar("n: numero = leer()\n", msg);
    CHECK(ok, "expresión dinámica no debe fallar en análisis (errores: " + msg + ")");
}

static void prueba_semantico_retrocompat_sin_anotacion() {
    std::string msg;
    bool ok = analizar(
        "nombre = \"Juan\"\n"
        "edad = 30\n"
        "escribir(nombre)\n", msg);
    CHECK(ok, "código sin anotaciones debe seguir funcionando (errores: " + msg + ")");
}

// ---------------------------------------------------------------------------
// Pruebas de generación de código — lat_verificar_tipo() se emite
// ---------------------------------------------------------------------------

static void prueba_codegen_verifica_numero() {
    std::string c = generar("n: numero = 42\n");
    CHECK(contiene(c, "lat_verificar_tipo("), "emite lat_verificar_tipo");
    CHECK(contiene(c, "LAT_NUMERO"), "referencia LAT_NUMERO");
    CHECK(contiene(c, "\"n\""), "incluye nombre de variable");
}

static void prueba_codegen_verifica_cadena() {
    std::string c = generar("s: cadena = \"hola\"\n");
    CHECK(contiene(c, "lat_verificar_tipo("), "emite lat_verificar_tipo");
    CHECK(contiene(c, "LAT_CADENA"), "referencia LAT_CADENA");
}

static void prueba_codegen_sin_anotacion_no_verifica() {
    std::string c = generar("n = 42\n");
    CHECK(!contiene(c, "lat_verificar_tipo"), "sin anotación no emite lat_verificar_tipo");
}

static void prueba_codegen_parametro_anotado() {
    std::string c = generar(
        "funcion doble(n: numero): numero\n"
        "    retornar n + n\n"
        "fin\n");
    CHECK(contiene(c, "lat_verificar_tipo("), "emite verificación de parámetro");
    CHECK(contiene(c, "LAT_NUMERO"), "referencia LAT_NUMERO para parámetro");
    CHECK(contiene(c, "\"n\""), "nombre del parámetro en verificación");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== PRUEBAS DE TIPADO GRADUAL (Fase 27) ===\n\n";

    std::cout << "-- Parser --\n";
    prueba_parser_tipo_numero();
    prueba_parser_tipo_cadena();
    prueba_parser_tipo_logico();
    prueba_parser_tipo_lista();
    prueba_parser_sin_anotacion_retrocompat();
    prueba_parser_funcion_tipos_parametros();
    prueba_parser_funcion_sin_tipos_retrocompat();

    std::cout << "-- Análisis semántico --\n";
    prueba_semantico_tipo_incompatible_numero_cadena();
    prueba_semantico_tipo_incompatible_logico_numero();
    prueba_semantico_tipo_incompatible_lista_dic();
    prueba_semantico_tipo_correcto_numero();
    prueba_semantico_tipo_correcto_cadena();
    prueba_semantico_tipo_correcto_lista();
    prueba_semantico_expresion_dinamica_no_falla();
    prueba_semantico_retrocompat_sin_anotacion();

    std::cout << "-- Generación de código --\n";
    prueba_codegen_verifica_numero();
    prueba_codegen_verifica_cadena();
    prueba_codegen_sin_anotacion_no_verifica();
    prueba_codegen_parametro_anotado();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << "\n";
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE TIPADO PASARON.\n";
    return g_fallos == 0 ? 0 : 1;
}
