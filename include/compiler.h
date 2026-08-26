// compiler.h
//
// Generación de código (Fase 5). GeneradorC transpila el AST de Latino a código
// C que usa el runtime (runtime/latino.h). Cada variable de Latino se compila a
// un valor dinámico LatValor.

#ifndef COMPILER_H
#define COMPILER_H

#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "recolector_variables.h"

class GeneradorC {
public:
    // Devuelve el código C completo equivalente al programa.
    std::string generar(Programa& programa);

private:
    struct InfoFuncion {
        size_t numParametros;
        bool variadico;
    };

    std::ostringstream salida;
    int indentacion = 0;
    int contadorTemp = 0;
    std::unordered_map<std::string, InfoFuncion> funciones;
    std::unordered_map<std::string, ClaseDef*> clases;
    std::unordered_map<std::string, EstructuraDef*> estructuras;
    std::unordered_map<std::string, InterfazDef*> interfaces;
    std::string actualClase;
    std::string actualPadre;
    bool enConstructor = false;
    std::set<std::string> libsUsadas;  // librerías detectadas durante generación

    void recolectarFunciones(Programa& programa);
    void recolectarTipos(Programa& programa);

    void emitir(const std::string& linea);   // escribe con sangría + salto
    std::string nuevoTemp();

    // Generación
    std::string genExpr(Expresion* e);
    void genSentencia(Sentencia* s);
    void genBloque(const ListaSent& cuerpo);
    void genFuncion(FuncionDef* f);
    void genClase(ClaseDef* c);
    void genEstructura(EstructuraDef* e);
    void genInterfaz(InterfazDef* i);
    void genMetodo(const std::string& claseNombre, MetodoDef* metodo,
                   const std::string& padreNombre);
    void generarCuerpo(Programa& programa);  // genera funciones + main (sin preámbulo)

    // Utilidades
    static std::string varC(const std::string& nombre);   // v_<nombre>
    static std::string funC(const std::string& nombre);   // lat_fn_<nombre>
    static std::string escaparCadena(const std::string& s);
    std::string genLlamada(Llamada* ll);
    std::string genAsignacionDestino(Expresion* destino, const std::string& valorC);
};

#endif  // COMPILER_H
