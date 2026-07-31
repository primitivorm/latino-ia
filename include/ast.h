// ast.h
//
// Árbol de Sintaxis Abstracta (AST) del lenguaje Latino.
//
// Diseño: jerarquía de nodos con el patrón Visitante. Toda fase que recorra el
// árbol (impresor de depuración, análisis semántico, generador de código C) se
// implementa como una subclase de `Visitante`, sin tocar las clases de nodo.
//
// Propiedad de la memoria: cada nodo posee a sus hijos mediante std::unique_ptr.

#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>

// --- Tipado gradual opcional -----------------------------------------------
// Tipos de anotación que el usuario puede escribir junto a una variable o
// parámetro de función (p.ej.  n: numero = 42  ó  funcion f(a: cadena): logico).
// Ninguno == sin anotación; la variable sigue siendo completamente dinámica.
enum class TipoAnotado {
    Ninguno,
    Numero,
    Cadena,
    Logico,
    Lista,
    Dic,
    Nulo
};

// Parámetro de función con anotación de tipo opcional.
struct ParamFuncion {
    std::string nombre;
    TipoAnotado tipo = TipoAnotado::Ninguno;
};

// --- Declaraciones adelantadas de todos los nodos concretos ---------------
struct LitNumero;
struct LitCadena;
struct LitLogico;
struct LitNulo;
struct Identificador;
struct Binaria;
struct Unaria;
struct PostOperador;
struct Ternaria;
struct AccesoIndice;
struct AccesoMiembro;
struct Llamada;
struct ListaLiteral;
struct DiccionarioLiteral;
struct VarArgs;

struct Programa;
struct Incluir;
struct Asignacion;
struct ExprSentencia;
struct Si;
struct Elegir;
struct Desde;
struct Mientras;
struct Repetir;
struct Romper;
struct FuncionDef;
struct Retornar;

// --- Interfaz del Visitante -----------------------------------------------
struct Visitante {
    virtual ~Visitante() = default;

    // Expresiones
    virtual void visitar(LitNumero&) = 0;
    virtual void visitar(LitCadena&) = 0;
    virtual void visitar(LitLogico&) = 0;
    virtual void visitar(LitNulo&) = 0;
    virtual void visitar(Identificador&) = 0;
    virtual void visitar(Binaria&) = 0;
    virtual void visitar(Unaria&) = 0;
    virtual void visitar(PostOperador&) = 0;
    virtual void visitar(Ternaria&) = 0;
    virtual void visitar(AccesoIndice&) = 0;
    virtual void visitar(AccesoMiembro&) = 0;
    virtual void visitar(Llamada&) = 0;
    virtual void visitar(ListaLiteral&) = 0;
    virtual void visitar(DiccionarioLiteral&) = 0;
    virtual void visitar(VarArgs&) = 0;

    // Sentencias
    virtual void visitar(Programa&) = 0;
    virtual void visitar(Incluir&) {}   // no-op por defecto; los visitantes que no la necesiten no deben sobreescribirla
    virtual void visitar(Asignacion&) = 0;
    virtual void visitar(ExprSentencia&) = 0;
    virtual void visitar(Si&) = 0;
    virtual void visitar(Elegir&) = 0;
    virtual void visitar(Desde&) = 0;
    virtual void visitar(Mientras&) = 0;
    virtual void visitar(Repetir&) = 0;
    virtual void visitar(Romper&) = 0;
    virtual void visitar(FuncionDef&) = 0;
    virtual void visitar(Retornar&) = 0;
};

// --- Clases base -----------------------------------------------------------
struct Nodo {
    int linea = 0;
    virtual ~Nodo();  // anclaje de vtable definido en ast.cpp
    virtual void aceptar(Visitante& v) = 0;
};

struct Expresion : Nodo {};
struct Sentencia : Nodo {};

using ExprPtr = std::unique_ptr<Expresion>;
using SentPtr = std::unique_ptr<Sentencia>;
using ListaSent = std::vector<SentPtr>;

// Macro de conveniencia para implementar `aceptar` en cada nodo concreto.
#define LATINO_ACEPTAR \
    void aceptar(Visitante& v) override { v.visitar(*this); }

// --- Expresiones -----------------------------------------------------------

// Literal numérico. Latino usa double; `esEntero` recuerda si el literal se
// escribió sin parte decimal (útil para formatear la salida).
struct LitNumero : Expresion {
    double valor = 0.0;
    bool esEntero = true;
    std::string lexema;  // texto original (para emitir el literal C exacto)
    LATINO_ACEPTAR
};

struct LitCadena : Expresion {
    std::string valor;  // sin las comillas
    LATINO_ACEPTAR
};

struct LitLogico : Expresion {
    bool valor = false;  // cierto/verdadero -> true, falso -> false
    LATINO_ACEPTAR
};

struct LitNulo : Expresion {
    LATINO_ACEPTAR
};

struct Identificador : Expresion {
    std::string nombre;
    LATINO_ACEPTAR
};

// Operación binaria: izq <op> der.  op es el lexema del operador
// (p.ej. "+", "..", "==", "&&", "^").
struct Binaria : Expresion {
    std::string op;
    ExprPtr izq;
    ExprPtr der;
    LATINO_ACEPTAR
};

// Operador unario prefijo (p.ej. "-x").
struct Unaria : Expresion {
    std::string op;
    ExprPtr operando;
    LATINO_ACEPTAR
};

// Operador postfijo: i++ / i-- (en Latino sólo válidos en post).
struct PostOperador : Expresion {
    std::string op;  // "++" o "--"
    ExprPtr operando;
    LATINO_ACEPTAR
};

// Operador ternario: (condicion) ? siCierto : siFalso
struct Ternaria : Expresion {
    ExprPtr condicion;
    ExprPtr siCierto;
    ExprPtr siFalso;
    LATINO_ACEPTAR
};

// Acceso por índice: objeto[indice]  (listas y diccionarios).
struct AccesoIndice : Expresion {
    ExprPtr objeto;
    ExprPtr indice;
    LATINO_ACEPTAR
};

// Acceso a miembro: objeto.miembro
struct AccesoMiembro : Expresion {
    ExprPtr objeto;
    std::string miembro;
    LATINO_ACEPTAR
};

// Llamada a función: destino(arg0, arg1, ...)
struct Llamada : Expresion {
    ExprPtr destino;
    std::vector<ExprPtr> argumentos;
    LATINO_ACEPTAR
};

// Literal de lista: [a, b, c]
struct ListaLiteral : Expresion {
    std::vector<ExprPtr> elementos;
    LATINO_ACEPTAR
};

// Par clave-valor de un diccionario literal.
struct ParDic {
    ExprPtr clave;
    ExprPtr valor;
};

// Literal de diccionario: { clave: valor, ... }
struct DiccionarioLiteral : Expresion {
    std::vector<ParDic> pares;
    LATINO_ACEPTAR
};

// El "..." usado dentro de [...] para recolectar los argumentos variables.
struct VarArgs : Expresion {
    LATINO_ACEPTAR
};

// --- Sentencias ------------------------------------------------------------

// Raíz del árbol: lista de sentencias del programa.
struct Programa : Sentencia {
    ListaSent sentencias;
    LATINO_ACEPTAR
};

// incluir "nombre"  o  incluir "archivo.lat"
struct Incluir : Sentencia {
    std::string modulo;  // nombre de la librería o ruta del archivo .lat
    LATINO_ACEPTAR
};

// Asignación (posiblemente múltiple): destinos = valores
//   a = 1                -> 1 destino, 1 valor
//   a, b, c = 1, 2, 3    -> 3 destinos, 3 valores
//   n: numero = 42       -> 1 destino con tipo anotado
// Cada destino es una expresión-lvalue (Identificador, AccesoIndice, AccesoMiembro).
// tiposDestino[i] corresponde a destinos[i]; Ninguno == sin anotación.
struct Asignacion : Sentencia {
    std::vector<ExprPtr> destinos;
    std::vector<ExprPtr> valores;
    std::vector<TipoAnotado> tiposDestino;
    bool esVar = false;
    bool esConst = false;
    LATINO_ACEPTAR
};

// Sentencia-expresión: una expresión usada como sentencia (p.ej. escribir(x), i++).
struct ExprSentencia : Sentencia {
    ExprPtr expr;
    LATINO_ACEPTAR
};

// Rama "osi" de un si: condición + cuerpo.
struct RamaOsi {
    ExprPtr condicion;
    ListaSent cuerpo;
};

// si / osi* / sino?
struct Si : Sentencia {
    ExprPtr condicion;
    ListaSent entonces;
    std::vector<RamaOsi> osis;
    ListaSent sino;
    bool tieneSino = false;
    LATINO_ACEPTAR
};

// Un caso de un elegir: valor + cuerpo.
struct CasoElegir {
    ExprPtr valor;
    ListaSent cuerpo;
};

// elegir(opcion) caso..: ... defecto/otro: ...
struct Elegir : Sentencia {
    ExprPtr opcion;
    std::vector<CasoElegir> casos;
    ListaSent defecto;
    bool tieneDefecto = false;
    LATINO_ACEPTAR
};

// desde(inicio; condicion; incremento) ... fin
struct Desde : Sentencia {
    SentPtr inicio;        // p.ej. Asignacion  i = 0
    ExprPtr condicion;     // p.ej. i <= 10
    SentPtr incremento;    // p.ej. ExprSentencia  i++  ó  Asignacion i = i + 10
    ListaSent cuerpo;
    LATINO_ACEPTAR
};

// mientras condicion ... fin
struct Mientras : Sentencia {
    ExprPtr condicion;
    ListaSent cuerpo;
    LATINO_ACEPTAR
};

// repetir ... hasta condicion
struct Repetir : Sentencia {
    ListaSent cuerpo;
    ExprPtr condicionHasta;
    LATINO_ACEPTAR
};

// romper
struct Romper : Sentencia {
    LATINO_ACEPTAR
};

// funcion/fun nombre(param0, param1, ...) ... fin
// Soporta anotaciones de tipo opcionales en parámetros y tipo de retorno:
//   funcion suma(a: numero, b: numero): numero
struct FuncionDef : Sentencia {
    std::string nombre;
    std::vector<ParamFuncion> parametros;
    TipoAnotado tipoRetorno = TipoAnotado::Ninguno;
    bool variadico = false;  // true si el último parámetro es "..."
    ListaSent cuerpo;
    LATINO_ACEPTAR
};

// regresar/retornar/ret  [valor]
struct Retornar : Sentencia {
    ExprPtr valor;  // puede ser nulo (retornar sin valor)
    LATINO_ACEPTAR
};

#endif  // AST_H
