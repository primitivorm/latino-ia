// test_ast.cpp
//
// Pruebas unitarias del AST (Fase 2). Como todavía no hay parser, los árboles
// se construyen a mano y se verifican volcándolos con ImpresorAST. Esto valida
// que el patrón Visitante despacha correctamente a cada tipo de nodo.

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "ast.h"
#include "ast_impresor.h"

// --- Framework mínimo de aserciones ---------------------------------------
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

// --- Constructores de conveniencia ----------------------------------------
static ExprPtr num(double v, bool entero = true) {
    auto n = std::make_unique<LitNumero>();
    n->valor = v;
    n->esEntero = entero;
    return n;
}
static ExprPtr cad(std::string s) {
    auto n = std::make_unique<LitCadena>();
    n->valor = std::move(s);
    return n;
}
static ExprPtr id(std::string s) {
    auto n = std::make_unique<Identificador>();
    n->nombre = std::move(s);
    return n;
}
static ExprPtr bin(std::string op, ExprPtr a, ExprPtr b) {
    auto n = std::make_unique<Binaria>();
    n->op = std::move(op);
    n->izq = std::move(a);
    n->der = std::move(b);
    return n;
}

static std::string volcar(Nodo& n) {
    std::ostringstream os;
    ImpresorAST imp(os);
    imp.imprimir(n);
    return os.str();
}

static bool contiene(const std::string& texto, const std::string& sub) {
    return texto.find(sub) != std::string::npos;
}

// El sub `a` debe aparecer antes que `b` en el texto.
static bool enOrden(const std::string& texto, const std::string& a, const std::string& b) {
    size_t pa = texto.find(a);
    size_t pb = texto.find(b);
    return pa != std::string::npos && pb != std::string::npos && pa < pb;
}

// --- Pruebas ---------------------------------------------------------------

// Programa:
//   x = 1 + 2
//   escribir("hola")
static void prueba_asignacion_y_llamada() {
    Programa prog;

    auto asig = std::make_unique<Asignacion>();
    asig->destinos.push_back(id("x"));
    asig->valores.push_back(bin("+", num(1), num(2)));
    prog.sentencias.push_back(std::move(asig));

    auto llam = std::make_unique<Llamada>();
    llam->destino = id("escribir");
    llam->argumentos.push_back(cad("hola"));
    auto es = std::make_unique<ExprSentencia>();
    es->expr = std::move(llam);
    prog.sentencias.push_back(std::move(es));

    std::string t = volcar(prog);

    CHECK(contiene(t, "Programa"), "debe contener Programa");
    CHECK(contiene(t, "Asignacion (1 = 1)"), "asignacion 1=1");
    CHECK(contiene(t, "Identificador 'x'"), "destino x");
    CHECK(contiene(t, "Binaria '+'"), "binaria +");
    CHECK(contiene(t, "Numero 1") && contiene(t, "Numero 2"), "operandos 1 y 2");
    CHECK(contiene(t, "Llamada"), "llamada");
    CHECK(contiene(t, "Identificador 'escribir'"), "callee escribir");
    CHECK(contiene(t, "Cadena 'hola'"), "argumento hola");

    // Orden y anidamiento.
    CHECK(enOrden(t, "Binaria '+'", "Numero 1"), "binaria antes que su operando");
    CHECK(enOrden(t, "Asignacion", "Llamada"), "asignacion antes que llamada");

    // La sangría de los operandos debe ser mayor que la de la binaria.
    size_t pBin = t.find("Binaria '+'");
    size_t pNum = t.find("Numero 1");
    size_t sangBin = pBin - t.rfind('\n', pBin) - 1;
    size_t sangNum = pNum - t.rfind('\n', pNum) - 1;
    CHECK(sangNum > sangBin, "operando mas indentado que la binaria");
}

// si edad >= 18 ... sino ... fin
static void prueba_si_sino() {
    auto si = std::make_unique<Si>();
    si->condicion = bin(">=", id("edad"), num(18));

    auto e1 = std::make_unique<ExprSentencia>();
    {
        auto l = std::make_unique<Llamada>();
        l->destino = id("escribir");
        l->argumentos.push_back(cad("mayor"));
        e1->expr = std::move(l);
    }
    si->entonces.push_back(std::move(e1));

    auto e2 = std::make_unique<ExprSentencia>();
    {
        auto l = std::make_unique<Llamada>();
        l->destino = id("escribir");
        l->argumentos.push_back(cad("menor"));
        e2->expr = std::move(l);
    }
    si->sino.push_back(std::move(e2));
    si->tieneSino = true;

    std::string t = volcar(*si);
    CHECK(contiene(t, "Si"), "si");
    CHECK(contiene(t, "Binaria '>='"), "condicion >=");
    CHECK(contiene(t, "entonces:") && contiene(t, "sino:"), "ramas entonces/sino");
    CHECK(enOrden(t, "entonces:", "sino:"), "entonces antes que sino");
    CHECK(contiene(t, "Cadena 'mayor'") && contiene(t, "Cadena 'menor'"), "cuerpos");
}

// fun sumar(a, b) ret a + b fin
static void prueba_funcion() {
    auto f = std::make_unique<FuncionDef>();
    f->nombre = "sumar";
    f->parametros = {ParamFuncion{"a"}, ParamFuncion{"b"}};
    auto r = std::make_unique<Retornar>();
    r->valor = bin("+", id("a"), id("b"));
    f->cuerpo.push_back(std::move(r));

    std::string t = volcar(*f);
    CHECK(contiene(t, "Funcion 'sumar' (a, b)"), "firma de la funcion");
    CHECK(contiene(t, "Retornar"), "retornar");
    CHECK(enOrden(t, "Funcion 'sumar'", "Retornar"), "cuerpo dentro de la funcion");
}

// funcion f(a, ...) ... fin  -> firma variádica
static void prueba_funcion_variadica() {
    auto f = std::make_unique<FuncionDef>();
    f->nombre = "varios";
    f->parametros = {ParamFuncion{"a"}};
    f->variadico = true;
    std::string t = volcar(*f);
    CHECK(contiene(t, "Funcion 'varios' (a, ...)"), "firma variadica");
}

// Ejercita el resto de tipos de nodo para confirmar que el Visitante despacha
// a todos sin caer en un método no implementado.
static void prueba_todos_los_nodos() {
    Programa prog;

    // numeros = [1, 2.5, verdadero, nulo]
    {
        auto lista = std::make_unique<ListaLiteral>();
        lista->elementos.push_back(num(1));
        lista->elementos.push_back(num(2.5, /*entero=*/false));
        auto b = std::make_unique<LitLogico>(); b->valor = true;
        lista->elementos.push_back(std::move(b));
        lista->elementos.push_back(std::make_unique<LitNulo>());
        auto a = std::make_unique<Asignacion>();
        a->destinos.push_back(id("numeros"));
        a->valores.push_back(std::move(lista));
        prog.sentencias.push_back(std::move(a));
    }

    // d = { "k": numeros[-1] }   (Diccionario + AccesoIndice + Unaria)
    {
        auto dic = std::make_unique<DiccionarioLiteral>();
        ParDic par;
        par.clave = cad("k");
        auto idx = std::make_unique<AccesoIndice>();
        idx->objeto = id("numeros");
        auto neg = std::make_unique<Unaria>();
        neg->op = "-";
        neg->operando = num(1);
        idx->indice = std::move(neg);
        par.valor = std::move(idx);
        dic->pares.push_back(std::move(par));
        auto a = std::make_unique<Asignacion>();
        a->destinos.push_back(id("d"));
        a->valores.push_back(std::move(dic));
        prog.sentencias.push_back(std::move(a));
    }

    // m = (x < 0) ? obj.campo : [...]   (Ternaria + AccesoMiembro + VarArgs)
    {
        auto ter = std::make_unique<Ternaria>();
        ter->condicion = bin("<", id("x"), num(0));
        auto miembro = std::make_unique<AccesoMiembro>();
        miembro->objeto = id("obj");
        miembro->miembro = "campo";
        ter->siCierto = std::move(miembro);
        auto lst = std::make_unique<ListaLiteral>();
        lst->elementos.push_back(std::make_unique<VarArgs>());
        ter->siFalso = std::move(lst);
        auto a = std::make_unique<Asignacion>();
        a->destinos.push_back(id("m"));
        a->valores.push_back(std::move(ter));
        prog.sentencias.push_back(std::move(a));
    }

    // desde(i = 0; i < 10; i++) ... fin   (Desde + PostOperador + Romper)
    {
        auto desde = std::make_unique<Desde>();
        auto init = std::make_unique<Asignacion>();
        init->destinos.push_back(id("i"));
        init->valores.push_back(num(0));
        desde->inicio = std::move(init);
        desde->condicion = bin("<", id("i"), num(10));
        auto inc = std::make_unique<ExprSentencia>();
        auto pp = std::make_unique<PostOperador>();
        pp->op = "++";
        pp->operando = id("i");
        inc->expr = std::move(pp);
        desde->incremento = std::move(inc);
        desde->cuerpo.push_back(std::make_unique<Romper>());
        prog.sentencias.push_back(std::move(desde));
    }

    // mientras i < 10 ... fin
    {
        auto m = std::make_unique<Mientras>();
        m->condicion = bin("<", id("i"), num(10));
        prog.sentencias.push_back(std::move(m));
    }

    // repetir ... hasta i == 10
    {
        auto r = std::make_unique<Repetir>();
        r->condicionHasta = bin("==", id("i"), num(10));
        prog.sentencias.push_back(std::move(r));
    }

    // elegir(c) caso 'A': ... defecto: ...
    {
        auto el = std::make_unique<Elegir>();
        el->opcion = id("c");
        CasoElegir caso;
        caso.valor = cad("A");
        el->casos.push_back(std::move(caso));
        el->tieneDefecto = true;
        prog.sentencias.push_back(std::move(el));
    }

    std::string t = volcar(prog);

    CHECK(contiene(t, "Lista (4)"), "lista de 4 elementos");
    CHECK(contiene(t, "Numero 2.5"), "flotante 2.5");
    CHECK(contiene(t, "Logico cierto"), "logico cierto");
    CHECK(contiene(t, "Nulo"), "nulo");
    CHECK(contiene(t, "Diccionario (1)"), "diccionario");
    CHECK(contiene(t, "Par"), "par del diccionario");
    CHECK(contiene(t, "AccesoIndice"), "acceso por indice");
    CHECK(contiene(t, "Unaria '-'"), "unaria -");
    CHECK(contiene(t, "Ternaria"), "ternaria");
    CHECK(contiene(t, "AccesoMiembro '.campo'"), "acceso a miembro");
    CHECK(contiene(t, "VarArgs ..."), "varargs");
    CHECK(contiene(t, "Desde"), "desde");
    CHECK(contiene(t, "PostOperador '++'"), "post incremento");
    CHECK(contiene(t, "Romper"), "romper");
    CHECK(contiene(t, "Mientras"), "mientras");
    CHECK(contiene(t, "Repetir") && contiene(t, "hasta:"), "repetir-hasta");
    CHECK(contiene(t, "Elegir") && contiene(t, "caso:") && contiene(t, "defecto:"), "elegir");
}

int main() {
    prueba_asignacion_y_llamada();
    prueba_si_sino();
    prueba_funcion();
    prueba_funcion_variadica();
    prueba_todos_los_nodos();

    std::cout << "\nComprobaciones: " << g_checks
              << "   Fallos: " << g_fallos << std::endl;
    if (g_fallos == 0)
        std::cout << "TODAS LAS PRUEBAS DEL AST PASARON." << std::endl;
    return g_fallos == 0 ? 0 : 1;
}
