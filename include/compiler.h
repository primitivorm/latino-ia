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

    // PLAN_FFI.md (F5): bibliotecas nombradas por "externo enlazar \"lib\"",
    // recolectadas durante generar(). En MSVC alcanza el "#pragma comment"
    // que ya se emite en el propio .c; en GNU/Clang, invocador_c.cpp usa esto
    // para agregar "-l<lib>" a la línea de enlace.
    const std::set<std::string>& bibliotecasEnlazadas() const { return bibliotecasEnlazar; }

private:
    struct InfoFuncion {
        size_t numParametros;
        bool variadico;
    };

    // PLAN_FFI.md (F5): firma de una función "externo" ya recolectada, para
    // que genLlamada resuelva el marshalling sin volver a recorrer el AST.
    struct InfoFuncionExterna {
        std::vector<TipoFFI> parametrosTipo;
        TipoFFI tipoRetorno = TipoFFI::Nulo;
    };

    std::ostringstream salida;
    int indentacion = 0;
    int contadorTemp = 0;
    std::unordered_map<std::string, InfoFuncion> funciones;
    std::unordered_map<std::string, InfoFuncionExterna> funcionesExternas;
    std::set<std::string> bibliotecasEnlazar;  // nombres de "enlazar" (sin ".lib"/"-l")
    std::unordered_map<std::string, ClaseDef*> clases;
    std::unordered_map<std::string, EstructuraDef*> estructuras;
    std::unordered_map<std::string, InterfazDef*> interfaces;
    std::string actualClase;
    std::string actualPadre;
    bool enConstructor = false;
    std::set<std::string> libsUsadas;  // librerías detectadas durante generación

    void recolectarFunciones(Programa& programa);
    void recolectarExterno(Programa& programa);
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

    // PLAN_FFI.md (F5): marshalling de una llamada a una función "externo".
    std::string genLlamadaExterna(const std::string& nombre, const InfoFuncionExterna& info,
                                  Llamada* ll);
    // Extrae/convierte un argumento LatValor al tipo C esperado por la firma,
    // con el chequeo dinámico lat_ffi_verificar_tipo (mismo patrón que
    // lat_verificar_tipo para parámetros anotados de una función normal).
    std::string genArgumentoFFI(Expresion* arg, TipoFFI tipo, const std::string& nombreFn,
                                int indice);
};

#endif  // COMPILER_H
