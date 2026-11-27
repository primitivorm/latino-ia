// test_macros.cpp — Pruebas unitarias del front-end y del expandedor de macros
// al estilo de Julia (input/PLAN_MACROS.md, fases M1-M6).

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "ast.h"
#include "compiler.h"
#include "expandidor_macros.h"
#include "impresor_fuente.h"
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

namespace {

struct Resultado {
    bool ok = false;
    std::string reporte;
    std::string c;
    std::string fuente;
};

Resultado expanderYGenerar(const std::string& codigo) {
    Resultado r;
    Lexer lexer(codigo);
    Parser parser(lexer);
    auto programa = parser.parse();
    if (!programa) return r;
    std::ostringstream reporte;
    if (!ExpandidorMacros::expandir(*programa, reporte)) return r;
    r.reporte = reporte.str();
    GeneradorC generador;
    r.c = generador.generar(*programa);
    r.fuente = ImpresorFuente::aTexto(*programa);
    r.ok = true;
    return r;
}

bool contiene(const std::string& texto, const std::string& trozo) {
    return texto.find(trozo) != std::string::npos;
}

}  // namespace

int main() {
    std::cout << "Pruebas unitarias de macros (PLAN_MACROS.md)\n";

    // --- Parser: macro / quote / @ / $ / :simbolo ---------------------------
    {
        Lexer lexer("macro doble(x)\n retornar quote $x * 2 fin\nfin\n");
        Parser parser(lexer);
        auto programa = parser.parse();
        CHECK(programa != nullptr, "el parser acepta una definicion de macro");
        if (programa) {
            CHECK(programa->sentencias.size() == 1, "una sola sentencia MacroDef");
            auto* def = dynamic_cast<MacroDef*>(programa->sentencias[0].get());
            CHECK(def != nullptr, "el nodo es MacroDef");
            if (def) {
                CHECK(def->nombre == "doble", "nombre de la macro");
                CHECK(def->parametros.size() == 1 && def->parametros[0] == "x", "un parametro 'x'");
                CHECK(def->cuerpo.size() == 1, "cuerpo con un retornar");
                auto* ret = dynamic_cast<Retornar*>(def->cuerpo[0].get());
                CHECK(ret != nullptr, "el cuerpo retorna");
                if (ret && !ret->valores.empty()) {
                    auto* cita = dynamic_cast<CitaExpr*>(ret->valores[0].get());
                    CHECK(cita != nullptr, "retorna una quote");
                    CHECK(cita && cita->cuerpo.size() == 1, "la quote tiene una sentencia");
                    if (cita && !cita->cuerpo.empty()) {
                        auto* es = dynamic_cast<ExprSentencia*>(cita->cuerpo[0].get());
                        CHECK(es != nullptr, "la quote contiene una expresion");
                        auto* bin = es ? dynamic_cast<Binaria*>(es->expr.get()) : nullptr;
                        CHECK(bin != nullptr && bin->op == "*", "la expresion es una multiplicacion");
                        CHECK(bin && dynamic_cast<InterpolacionExpr*>(bin->izq.get()) != nullptr,
                              "$x se parseo como InterpolacionExpr");
                    }
                }
            }
        }
    }

    {
        Lexer lexer("escribir(@suma 2, 3)\n");
        Parser parser(lexer);
        auto programa = parser.parse();
        CHECK(programa != nullptr, "el parser acepta invocacion con dos argumentos");
        if (programa && !programa->sentencias.empty()) {
            auto* es = dynamic_cast<ExprSentencia*>(programa->sentencias[0].get());
            auto* llam = es ? dynamic_cast<Llamada*>(es->expr.get()) : nullptr;
            auto* inv = llam && !llam->argumentos.empty()
                            ? dynamic_cast<MacroInvocacion*>(llam->argumentos[0].get())
                            : nullptr;
            CHECK(inv != nullptr && inv->nombre == "suma", "@suma es MacroInvocacion");
            CHECK(inv && inv->argumentos.size() == 2, "dos argumentos sin evaluar");
        }
    }

    {
        Lexer lexer("e = Expr(:llamada, :escribir, 1)\n");
        Parser parser(lexer);
        auto programa = parser.parse();
        CHECK(programa != nullptr, "el parser acepta :simbolo");
        if (programa && !programa->sentencias.empty()) {
            auto* asig = dynamic_cast<Asignacion*>(programa->sentencias[0].get());
            auto* llam = asig && !asig->valores.empty()
                             ? dynamic_cast<Llamada*>(asig->valores[0].get())
                             : nullptr;
            CHECK(llam != nullptr, "Expr(...) se parsea como llamada");
            if (llam && llam->argumentos.size() >= 2) {
                CHECK(dynamic_cast<SimboloExpr*>(llam->argumentos[0].get()) != nullptr,
                      ":llamada es SimboloExpr");
                CHECK(dynamic_cast<SimboloExpr*>(llam->argumentos[1].get()) != nullptr,
                      ":escribir es SimboloExpr");
            }
        }
    }

    // --- Expansión básica ---------------------------------------------------
    {
        auto r = expanderYGenerar(
            "macro doble(x)\n"
            "  retornar quote $x * 2 fin\n"
            "fin\n"
            "escribir(@doble 21)\n");
        CHECK(r.ok, "expansion de @doble");
        CHECK(contiene(r.reporte, "@doble") == false, "no queda MacroInvocacion en el fuente impreso");
        CHECK(contiene(r.fuente, "macro doble") == false, "MacroDef se elimina del programa");
        CHECK(contiene(r.c, "lat_escribir"), "se genera la llamada a escribir");
        CHECK(contiene(r.c, "lat_multiplicar"), "se genera la multiplicacion expandida");
        CHECK(contiene(r.fuente, "21 * 2"), "la expansion contiene el argumento 21");
    }

    {
        auto r = expanderYGenerar(
            "@expandir_macro @doble 5\n"
            "macro doble(x)\n"
            "  retornar quote $x * 2 fin\n"
            "fin\n");
        CHECK(r.ok, "@expandir_macro con macro definida despues");
        CHECK(contiene(r.reporte, "(5 * 2)"), "el reporte contiene la expansion impresa");
    }

    // --- Higiene: temporal introducido renombrado, argumento conservado -----
    {
        auto r = expanderYGenerar(
            "macro con_temporal(x)\n"
            "  retornar quote\n"
            "    temporal = $x\n"
            "    temporal + 1\n"
            "  fin\n"
            "fin\n"
            "@expandir_macro @con_temporal y\n");
        CHECK(r.ok, "expansion higienica");
        CHECK(contiene(r.reporte, "__m"), "la variable introducida se renombra");
        CHECK(contiene(r.reporte, "temporal = y"), "el argumento conserva su nombre");
        CHECK(contiene(r.reporte, "_temporal + 1"), "la variable se renombra en todos sus usos");
    }

    // --- esc() dentro de quote ---------------------------------------------
    {
        auto r = expanderYGenerar(
            "macro usar(x)\n"
            "  retornar quote esc(x) + 1 fin\n"
            "fin\n"
            "escribir(@usar y)\n");
        CHECK(r.ok, "esc(x) se evalúa en tiempo de compilación");
        CHECK(contiene(r.fuente, "y + 1"), "esc(x) inserta el argumento sin renombrar");
        CHECK(contiene(r.fuente, "esc(") == false, "esc() no llega al código generado");
    }

    // --- Construccion programatica del AST ----------------------------------
    {
        auto r = expanderYGenerar(
            "macro llamar(f, x)\n"
            "  retornar Expr(:llamada, f, x)\n"
            "fin\n"
            "escribir(@llamar acadena, 42)\n");
        CHECK(r.ok, "Expr(:llamada, ...) construye una llamada");
        CHECK(contiene(r.fuente, "acadena(42)"), "la llamada construida se imprime como codigo");
    }

    // --- Inspeccion de AST desde el cuerpo de macro -------------------------
    {
        auto r = expanderYGenerar(
            "macro tipo_de(e)\n"
            "  retornar quote $(string(e.tipo)) fin\n"
            "fin\n"
            "escribir(@tipo_de 1 + 2)\n");
        CHECK(r.ok, "acceso a Expr.tipo dentro de una macro");
        CHECK(contiene(r.fuente, "\"binaria\""), "e.tipo devuelve el simbolo 'binaria'");
    }

    // --- Funciones definidas dentro del cuerpo de macro ---------------------
    {
        auto r = expanderYGenerar(
            "macro generar(n)\n"
            "  funcion pieza(x)\n"
            "    retornar Expr(:binaria, :+, x, 1)\n"
            "  fin\n"
            "  retornar pieza(n)\n"
            "fin\n"
            "escribir(@generar 7)\n");
        CHECK(r.ok, "funcion auxiliar dentro de la macro");
        CHECK(contiene(r.fuente, "(7 + 1)"), "la funcion auxiliar construye la expresion");
    }

    // --- Macros anidadas ----------------------------------------------------
    {
        auto r = expanderYGenerar(
            "macro doble(x)\n"
            "  retornar quote $x * 2 fin\n"
            "fin\n"
            "macro cuadruple(x)\n"
            "  retornar Expr(:macro, :doble, Expr(:macro, :doble, x))\n"
            "fin\n"
            "escribir(@cuadruple 3)\n");
        CHECK(r.ok, "macro que invoca a otra macro");
        CHECK(contiene(r.fuente, "(3 * 2) * 2"), "ambas macros se expanden");
        CHECK(contiene(r.fuente, "@doble") == false, "no quedan invocaciones sin expander");
    }

    // --- Macros integradas --------------------------------------------------
    {
        auto r = expanderYGenerar("@mostrar 2 + 2\n");
        CHECK(r.ok, "@mostrar se expande");
        CHECK(contiene(r.fuente, "escribir("), "@mostrar genera una llamada a escribir");
        CHECK(contiene(r.fuente, "(2 + 2) = "), "@mostrar incluye el texto de la expresion");
    }

    {
        auto r = expanderYGenerar("@afirmar x > 0\n");
        CHECK(r.ok, "@afirmar se expande");
        CHECK(contiene(r.fuente, "si ((x > 0) == falso)"), "@afirmar genera el chequeo");
        CHECK(contiene(r.fuente, "error("), "@afirmar termina con error()");
    }

    {
        auto r = expanderYGenerar("@tiempo 1 + 1\n");
        CHECK(r.ok, "@tiempo se expande");
        CHECK(contiene(r.fuente, "tiempo_ns()"), "@tiempo usa el reloj monotónico");
        CHECK(contiene(r.c, "lat_tiempo_ns()"), "el backend C emite lat_tiempo_ns()");
    }

    {
        auto r = expanderYGenerar(
            "@asegurar x > 0\n"
            "@show x\n"
            "macro numerar()\n"
            "  v = gensimb(\"v\")\n"
            "  retornar Expr(:asignacion, v, 5)\n"
            "fin\n"
            "@numerar()\n");
        CHECK(r.ok, "los alias @asegurar, @show y gensimb siguen funcionando");
        CHECK(contiene(r.fuente, "afirmación falló:"), "@asegurar es alias de @afirmar");
        CHECK(contiene(r.fuente, "escribir("), "@show es alias de @mostrar");
        CHECK(contiene(r.fuente, "= 5"), "gensimb produce un nombre usable en Expr");
    }

    // --- Errores reportados -------------------------------------------------
    {
        Lexer lexer("escribir(@no_existe 1)\n");
        Parser parser(lexer);
        auto programa = parser.parse();
        CHECK(programa != nullptr, "parser de macro inexistente");
        CHECK(!ExpandidorMacros::expandir(*programa),
              "una macro no definida se reporta como error de expansion");
    }

    std::cout << "Resultado: " << (g_checks - g_fallos) << "/" << g_checks
              << " comprobaciones correctas\n";
    return g_fallos == 0 ? 0 : 1;
}
