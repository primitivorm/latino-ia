// analizador_semantico.h
//
// Análisis semántico (Fase 4). Recorre el AST con el patrón Visitante y valida:
//   - uso de variables no declaradas,
//   - reasignación de constantes (identificadores en MAYÚSCULAS),
//   - 'romper' fuera de un bucle,
//   - 'retornar' fuera de una función,
//   - '...' (varargs) fuera de una función variádica,
//   - funciones no definidas y número de argumentos incorrecto,
//   - redefinición de funciones y parámetros duplicados.
//
// Latino admite anotaciones de tipo opcionales (tipado gradual). Cuando una
// variable o parámetro lleva anotación, el analizador detecta errores obvios
// en compilación (literal incompatible); los casos dinámicos se verifican en
// runtime mediante lat_verificar_tipo().
//
// Los errores no abortan el análisis: se acumulan y se reportan todos al final.

#ifndef ANALIZADOR_SEMANTICO_H
#define ANALIZADOR_SEMANTICO_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"

class AnalizadorSemantico : public Visitante {
public:
    AnalizadorSemantico();

    // Analiza el programa. Devuelve true si no hubo errores semánticos.
    // Reporta cada error por stderr con su número de línea.
    bool analizar(Programa& programa);

    // Expresiones
    void visitar(LitNumero&) override;
    void visitar(LitCadena&) override;
    void visitar(LitLogico&) override;
    void visitar(LitNulo&) override;
    void visitar(Identificador&) override;
    void visitar(Binaria&) override;
    void visitar(Unaria&) override;
    void visitar(PostOperador&) override;
    void visitar(Ternaria&) override;
    void visitar(AccesoIndice&) override;
    void visitar(AccesoMiembro&) override;
    void visitar(Llamada&) override;
    void visitar(ListaLiteral&) override;
    void visitar(DiccionarioLiteral&) override;
    void visitar(VarArgs&) override;

    // Sentencias
    void visitar(Programa&) override;
    void visitar(Incluir&) override;
    void visitar(Asignacion&) override;
    void visitar(ExprSentencia&) override;
    void visitar(Si&) override;
    void visitar(Elegir&) override;
    void visitar(Desde&) override;
    void visitar(Mientras&) override;
    void visitar(Repetir&) override;
    void visitar(Romper&) override;
    void visitar(FuncionDef&) override;
    void visitar(Retornar&) override;

private:
    struct InfoFuncion {
        size_t numParametros;
        bool variadico;
        int linea;
    };
    struct ErrorSemantico {
        int linea;
        std::string mensaje;
    };

    // Cada ámbito mapea nombre de variable → tipo anotado (Ninguno si sin anotación).
    std::vector<std::unordered_map<std::string, TipoAnotado>> ambitos;
    std::unordered_map<std::string, InfoFuncion> funciones;
    std::unordered_set<std::string> constantes;
    std::vector<ErrorSemantico> errores;

    int profundidadBucle;
    int profundidadFuncion;
    int profundidadVariadica;

    void entrarAmbito();
    void salirAmbito();
    void declararVariable(const std::string& nombre, TipoAnotado tipo, int linea);
    bool estaDeclarada(const std::string& nombre) const;
    void usarIdentificador(const std::string& nombre, int linea);
    bool esIncorporada(const std::string& nombre) const;
    bool esLibreria(const std::string& nombre) const;
    void recolectarFunciones(Programa& programa);
    void analizarBloque(ListaSent& cuerpo);
    void agregarError(int linea, const std::string& mensaje);
};

#endif  // ANALIZADOR_SEMANTICO_H
