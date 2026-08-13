// test_semantico.cpp
//
// Pruebas unitarias del análisis semántico (Fase 4).

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "analizador_semantico.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

// --- Framework mínimo de aserciones ---------------------------------------
static int g_fallos = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      ++g_fallos;                                                              \
      std::cerr << "  FALLO [linea " << __LINE__ << "]: " << msg << "\n";      \
    }                                                                          \
  } while (0)

// Analiza el código y devuelve true si no hubo errores. Captura los mensajes
// de error (stderr) en `mensajes`.
static bool analizar(const std::string &src, std::string &mensajes) {
  Lexer lexer(src);
  Parser parser(lexer);
  auto prog = parser.parse();
  if (!prog) {
    mensajes = "<error de sintaxis>";
    return false;
  }
  std::ostringstream cap;
  std::streambuf *viejo = std::cerr.rdbuf(cap.rdbuf());
  AnalizadorSemantico sem;
  bool ok = sem.analizar(*prog);
  std::cerr.rdbuf(viejo);
  mensajes = cap.str();
  return ok;
}

static bool contiene(const std::string &t, const std::string &sub) {
  return t.find(sub) != std::string::npos;
}

// Comprueba que el análisis falla y que el mensaje contiene `fragmento`.
static void esperarError(const std::string &nombre, const std::string &src,
                         const std::string &fragmento) {
  std::string msg;
  bool ok = analizar(src, msg);
  CHECK(!ok, nombre << ": deberia fallar el analisis");
  CHECK(contiene(msg, fragmento), nombre << ": el mensaje deberia contener '"
                                         << fragmento << "' (fue: " << msg
                                         << ")");
}

// Comprueba que el análisis tiene éxito (sin errores).
static void esperarOK(const std::string &nombre, const std::string &src) {
  std::string msg;
  bool ok = analizar(src, msg);
  CHECK(ok, nombre << ": deberia pasar el analisis (errores: " << msg << ")");
}

// --- Pruebas ---------------------------------------------------------------

static void prueba_programa_valido() {
  esperarOK("valido", "nombre = \"Juan\"\n"
                      "edad = 30\n"
                      "escribir(nombre)\n"
                      "escribir(edad)\n");
}

static void prueba_variable_no_declarada() {
  esperarError("no_declarada", "escribir(x)\n", "variable no declarada 'x'");
}

static void prueba_constante_reasignada() {
  esperarError("constante",
               "PI = 3.14159\n"
               "PI = 3\n",
               "no se puede reasignar la constante 'PI'");
}

static void prueba_romper_fuera_de_bucle() {
  esperarError("romper", "romper\n", "'romper' fuera de un bucle");
}

static void prueba_romper_dentro_de_bucle_ok() {
  esperarOK("romper_ok", "desde (i = 0; i < 10; i++)\n"
                         "  si i == 5\n"
                         "    romper\n"
                         "  fin\n"
                         "fin\n");
}

static void prueba_retornar_fuera_de_funcion() {
  esperarError("retornar", "retornar 1\n", "'retornar' fuera de una función");
}

static void prueba_funcion_no_definida() {
  esperarError("no_definida", "foo(1, 2)\n", "función no definida 'foo'");
}

static void prueba_aridad_incorrecta() {
  esperarError("aridad",
               "fun sumar(a, b)\n"
               "  ret a + b\n"
               "fin\n"
               "sumar(1)\n",
               "espera 2 argumento(s), se pasaron 1");
}

static void prueba_aridad_correcta_ok() {
  esperarOK("aridad_ok", "fun sumar(a, b)\n"
                         "  ret a + b\n"
                         "fin\n"
                         "r = sumar(1, 2)\n"
                         "escribir(r)\n");
}

static void prueba_llamada_antes_de_definir_ok() {
  // Las funciones se "elevan" (hoisting): se pueden llamar antes de definirse.
  esperarOK("hoisting", "r = saluda()\n"
                        "fun saluda()\n"
                        "  ret \"hola\"\n"
                        "fin\n");
}

static void prueba_variadica_ok() {
  esperarOK("variadica_ok", "funcion f(a, b, ...)\n"
                            "  va = [...]\n"
                            "  ret a + b\n"
                            "fin\n"
                            "f(1, 2, 3, 4)\n");
}

static void prueba_varargs_fuera_de_variadica() {
  esperarError("varargs", "x = [...]\n",
               "'...' sólo es válido dentro de una función variádica");
}

static void prueba_funcion_redefinida() {
  esperarError("redefinida",
               "fun f()\n"
               "fin\n"
               "fun f()\n"
               "fin\n",
               "ya está definida");
}

static void prueba_parametro_duplicado() {
  esperarError("param_dup",
               "fun f(a, a)\n"
               "  ret a\n"
               "fin\n",
               "parámetro duplicado 'a'");
}

static void prueba_acceso_indice_variable_declarada() {
  esperarOK("indice_ok", "numeros = [1, 2, 3]\n"
                         "numeros[0] = 99\n"
                         "escribir(numeros[-1])\n");
}

static void prueba_acceso_indice_no_declarada() {
  esperarError("indice_no_decl", "lista[0] = 1\n",
               "variable no declarada 'lista'");
}

static void prueba_semantico_var_y_const() {
  esperarOK("var_reasignacion_ok", "var x = 10\n"
                                   "x = 20\n");

  esperarOK("var_sin_ini_ok", "var x\n"
                              "x = 30\n");

  esperarError("const_reasignacion_error",
               "const pi = 3.1416\n"
               "pi = 3\n",
               "no se puede reasignar la constante 'pi'");

  esperarError("const_mayus_reasignacion_error",
               "const PI = 3.14\n"
               "PI = 3\n",
               "no se puede reasignar la constante 'PI'");
}

static void prueba_poo_nuevo_clase_ok() {
  esperarOK("poo_nuevo_ok",
            "clase Persona\n"
            "  funcion Persona(nombre)\n"
            "    este.nombre = nombre\n"
            "  fin\n"
            "fin\n"
            "persona = nuevo Persona(\"Ana\")\n");
}

static void prueba_poo_este_fuera_de_metodo() {
  esperarError("poo_este_fuera",
               "clase A\n"
               "  funcion A()\n"
               "  fin\n"
               "fin\n"
               "x = este\n",
               "'este' sólo puede usarse dentro de un método de instancia");
}

static void prueba_poo_nuevo_interfaz_error() {
  esperarError("poo_nuevo_interfaz",
               "interfaz I\n"
               "  funcion f()\n"
               "fin\n"
               "x = nuevo I()\n",
               "no se puede instanciar la interfaz 'I'");
}

static void prueba_poo_base_fuera_constructor() {
  esperarError("poo_base_fuera",
               "clase Padre\n"
               "  funcion Padre()\n"
               "  fin\n"
               "fin\n"
               "clase Hijo extiende Padre\n"
               "  funcion metodo()\n"
               "    base()\n"
               "  fin\n"
               "fin\n",
               "'base' sólo puede llamarse dentro de un constructor");
}

static void prueba_poo_nuevo_clase_abstracta_error() {
  esperarError("poo_nuevo_abstracta",
               "abstracto clase Figura\n"
               "  abstracto funcion area(): numero\n"
               "fin\n"
               "x = nuevo Figura()\n",
               "no se puede instanciar la clase abstracta 'Figura'");
}

int main() {
  prueba_programa_valido();
  prueba_variable_no_declarada();
  prueba_constante_reasignada();
  prueba_romper_fuera_de_bucle();
  prueba_romper_dentro_de_bucle_ok();
  prueba_retornar_fuera_de_funcion();
  prueba_funcion_no_definida();
  prueba_aridad_incorrecta();
  prueba_aridad_correcta_ok();
  prueba_llamada_antes_de_definir_ok();
  prueba_variadica_ok();
  prueba_varargs_fuera_de_variadica();
  prueba_funcion_redefinida();
  prueba_parametro_duplicado();
  prueba_acceso_indice_variable_declarada();
  prueba_acceso_indice_no_declarada();
  prueba_semantico_var_y_const();
  prueba_poo_nuevo_clase_ok();
  prueba_poo_este_fuera_de_metodo();
  prueba_poo_nuevo_interfaz_error();
  prueba_poo_base_fuera_constructor();
  prueba_poo_nuevo_clase_abstracta_error();

  std::cout << "\nComprobaciones: " << g_checks << "   Fallos: " << g_fallos
            << std::endl;
  if (g_fallos == 0)
    std::cout << "TODAS LAS PRUEBAS DEL ANALISIS SEMANTICO PASARON."
              << std::endl;
  return g_fallos == 0 ? 0 : 1;
}
