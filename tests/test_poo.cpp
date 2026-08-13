// test_poo.cpp
//
// Pruebas unitarias de la Programación Orientada a Objetos (clases,
// estructuras, interfaces, herencia, `nuevo`, `este`, `base` y el operador
// `es`). Cubre parser, análisis semántico y generación de código C. Las
// pruebas de extremo a extremo (compilar + ejecutar) viven en
// test_poo_e2e.cpp.

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast.h"
#include "ast_impresor.h"
#include "compiler.h"
#include "lexer.h"
#include "parser.h"

// --- Framework mínimo de aserciones ----------------------------------------
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

static bool contiene(const std::string& t, const std::string& sub) {
    return t.find(sub) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Parser: parsea y vuelca el AST con ImpresorAST.
// ---------------------------------------------------------------------------
static std::unique_ptr<Programa> parsear(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    return parser.parse();
}

static std::string volcar(const std::string& src) {
    auto prog = parsear(src);
    if (!prog) return "<nullptr>";
    std::ostringstream os;
    ImpresorAST imp(os);
    imp.imprimir(*prog);
    return os.str();
}

static void prueba_parser_clase_simple() {
    std::string t = volcar("clase Animal\nfin\n");
    CHECK(contiene(t, "Clase 'Animal'"), "clase simple");
}

static void prueba_parser_clase_herencia() {
    std::string t = volcar("clase Perro extiende Animal\nfin\n");
    CHECK(contiene(t, "Clase 'Perro'") && contiene(t, "extiende Animal"), "herencia");
}

static void prueba_parser_clase_interfaces() {
    std::string t = volcar("clase P implementa I1, I2\nfin\n");
    CHECK(contiene(t, "implementa I1, I2"), "interfaces implementadas");
}

static void prueba_parser_interfaz() {
    std::string t = volcar("interfaz IComparable\n  funcion comparar(valor: numero): numero\nfin\n");
    CHECK(contiene(t, "Interfaz 'IComparable'"), "interfaz");
    CHECK(contiene(t, "Metodo 'comparar'"), "metodo de interfaz");
    CHECK(contiene(t, "[abstracto]"), "metodo de interfaz sin cuerpo");
}

static void prueba_parser_estructura() {
    std::string t = volcar("estructura Punto\n  x: numero\n  y: numero\nfin\n");
    CHECK(contiene(t, "Estructura 'Punto'"), "estructura");
    CHECK(contiene(t, "Campo 'x'") && contiene(t, "Campo 'y'"), "campos de estructura");
}

static void prueba_parser_clase_abstracta() {
    std::string t = volcar("abstracto clase Figura\n  abstracto funcion area(): numero\nfin\n");
    CHECK(contiene(t, "Clase 'Figura' [abstracta]"), "clase abstracta");
    CHECK(contiene(t, "Metodo 'area'") && contiene(t, "[abstracto]"), "metodo abstracto sin cuerpo");
}

static void prueba_parser_nuevo_este_es() {
    std::string t = volcar(
        "clase A\n"
        "  funcion A()\n"
        "    este.x = 1\n"
        "  fin\n"
        "fin\n"
        "a = nuevo A()\n"
        "si a es A\n"
        "  escribir(\"si\")\n"
        "fin\n");
    CHECK(contiene(t, "Nuevo 'A'"), "nuevo");
    CHECK(contiene(t, "Este"), "este");
    CHECK(contiene(t, "Es 'A'"), "es");
}

static void prueba_parser_base() {
    std::string t = volcar(
        "clase A\n"
        "  funcion A()\n"
        "  fin\n"
        "fin\n"
        "clase B extiende A\n"
        "  funcion B()\n"
        "    base()\n"
        "  fin\n"
        "fin\n");
    CHECK(contiene(t, "LlamadaBase"), "llamada a base()");
}

// ---------------------------------------------------------------------------
// Análisis semántico.
// ---------------------------------------------------------------------------
static bool analizar(const std::string& src, std::string& mensajes) {
    Lexer lexer(src);
    Parser parser(lexer);
    auto prog = parser.parse();
    if (!prog) {
        mensajes = "<error de sintaxis>";
        return false;
    }
    std::ostringstream cap;
    std::streambuf* viejo = std::cerr.rdbuf(cap.rdbuf());
    AnalizadorSemantico sem;
    bool ok = sem.analizar(*prog);
    std::cerr.rdbuf(viejo);
    mensajes = cap.str();
    return ok;
}

static void esperarOK(const std::string& nombre, const std::string& src) {
    std::string msg;
    bool ok = analizar(src, msg);
    CHECK(ok, nombre << ": deberia pasar el analisis (errores: " << msg << ")");
}

static void esperarError(const std::string& nombre, const std::string& src,
                         const std::string& fragmento) {
    std::string msg;
    bool ok = analizar(src, msg);
    CHECK(!ok, nombre << ": deberia fallar el analisis");
    CHECK(contiene(msg, fragmento), nombre << ": el mensaje deberia contener '"
                                           << fragmento << "' (fue: " << msg << ")");
}

static void prueba_sem_herencia_no_definida() {
    esperarError("herencia_no_def", "clase Perro extiende NoExiste\nfin\n",
                 "tipo base desconocido 'NoExiste'");
}

static void prueba_sem_interfaz_no_definida() {
    esperarError("interfaz_no_def", "clase P implementa NoExiste\nfin\n",
                 "interfaz desconocida 'NoExiste'");
}

static void prueba_sem_interfaz_incompleta() {
    esperarError("interfaz_incompleta",
                 "interfaz I\n"
                 "  funcion f()\n"
                 "fin\n"
                 "clase C implementa I\n"
                 "fin\n",
                 "no implementa el método 'f'");
}

static void prueba_sem_clase_abstracta_no_instanciable() {
    esperarError("abstracta_no_instanciable",
                 "abstracto clase A\n"
                 "  abstracto funcion f(): numero\n"
                 "fin\n"
                 "x = nuevo A()\n",
                 "no se puede instanciar la clase abstracta 'A'");
}

static void prueba_sem_herencia_ok() {
    esperarOK("herencia_ok",
              "clase Animal\n"
              "  funcion Animal()\n"
              "  fin\n"
              "fin\n"
              "clase Perro extiende Animal\n"
              "  funcion Perro()\n"
              "    base()\n"
              "  fin\n"
              "fin\n"
              "p = nuevo Perro()\n"
              "si p es Animal\n"
              "  escribir(\"ok\")\n"
              "fin\n");
}

static void prueba_sem_metodo_estatico_ok() {
    esperarOK("metodo_estatico_ok",
              "clase Animal\n"
              "  funcion Animal()\n"
              "  fin\n"
              "  estatico funcion crear(): Animal\n"
              "    retornar nuevo Animal()\n"
              "  fin\n"
              "fin\n"
              "a = Animal.crear()\n");
}

// ---------------------------------------------------------------------------
// Generación de código C.
// ---------------------------------------------------------------------------
static std::string generar(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer);
    auto prog = parser.parse();
    if (!prog) return "<nullptr>";
    GeneradorC gen;
    return gen.generar(*prog);
}

static void prueba_codegen_constructor_y_nuevo() {
    std::string c = generar(
        "clase P\n"
        "  publico n: cadena\n"
        "  funcion P(n: cadena)\n"
        "    este.n = n\n"
        "  fin\n"
        "fin\n"
        "p = nuevo P(\"Juan\")\n");
    CHECK(contiene(c, "lat_fn_P_P"), "constructor generado");
    CHECK(contiene(c, "lat_obj_nuevo(\"P\")"), "creacion de objeto");
}

static void prueba_codegen_es() {
    std::string c = generar(
        "clase Persona\n"
        "  funcion Persona()\n"
        "  fin\n"
        "fin\n"
        "p = nuevo Persona()\n"
        "si p es Persona\n"
        "  escribir(\"si\")\n"
        "fin\n");
    CHECK(contiene(c, "lat_obj_es_instancia(v_p, \"Persona\")"), "codegen de es");
}

static void prueba_codegen_herencia_ascendencia() {
    std::string c = generar(
        "clase Animal\n"
        "  funcion Animal()\n"
        "  fin\n"
        "fin\n"
        "clase Perro extiende Animal\n"
        "  funcion Perro()\n"
        "    base()\n"
        "  fin\n"
        "fin\n"
        "p = nuevo Perro()\n");
    CHECK(contiene(c, "lat_obj_nuevo(\"Animal\")"), "el objeto se crea como la clase base");
    CHECK(contiene(c, "lat_obj_set_clase("), "reclasificacion hacia la clase derivada");
}

static void prueba_codegen_metodo_estatico() {
    std::string c = generar(
        "clase Animal\n"
        "  funcion Animal()\n"
        "  fin\n"
        "  estatico funcion crear(): Animal\n"
        "    retornar nuevo Animal()\n"
        "  fin\n"
        "fin\n"
        "a = Animal.crear()\n");
    CHECK(contiene(c, "lat_fn_Animal_crear(0, NULL)"), "llamada a metodo estatico sin 'este'");
}

int main() {
    prueba_parser_clase_simple();
    prueba_parser_clase_herencia();
    prueba_parser_clase_interfaces();
    prueba_parser_interfaz();
    prueba_parser_estructura();
    prueba_parser_clase_abstracta();
    prueba_parser_nuevo_este_es();
    prueba_parser_base();

    prueba_sem_herencia_no_definida();
    prueba_sem_interfaz_no_definida();
    prueba_sem_interfaz_incompleta();
    prueba_sem_clase_abstracta_no_instanciable();
    prueba_sem_herencia_ok();
    prueba_sem_metodo_estatico_ok();

    prueba_codegen_constructor_y_nuevo();
    prueba_codegen_es();
    prueba_codegen_herencia_ascendencia();
    prueba_codegen_metodo_estatico();

    std::cout << "\nComprobaciones: " << g_checks << "   Fallos: " << g_fallos
              << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DE POO PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
