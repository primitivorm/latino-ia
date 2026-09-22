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
    void visitar(NuevoExpr&) override;
    void visitar(EsExpr&) override;
    void visitar(AccesoEste&) override;

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
    void visitar(LlamadaBase&) override;
    void visitar(ClaseDef&) override;
    void visitar(EstructuraDef&) override;
    void visitar(InterfazDef&) override;
    void visitar(Retornar&) override;
    void visitar(InseguroBloque&) override;  // PLAN_FFI.md (F4)

private:
    struct InfoFuncion {
        size_t numParametros;
        bool variadico;
        int linea;
        // PLAN_GENERICOS.md: vacío si la función no es genérica.
        std::vector<ParametroGenerico> genericos;
        std::vector<TipoAnotado> parametrosTipo;
        std::vector<std::string> parametrosClase;  // tipoClase si Objeto (puede ser un nombre genérico)
        TipoAnotado tipoRetorno = TipoAnotado::Ninguno;
        std::string tipoRetornoClase;
    };
    struct ErrorSemantico {
        int linea;
        std::string mensaje;
    };

    // PLAN_FFI.md (F4): firma de una función declarada dentro de un bloque
    // "externo" (nombre -> clave del mapa funcionesExternas).
    struct InfoFuncionExterna {
        std::vector<TipoFFI> parametrosTipo;
        TipoFFI tipoRetorno = TipoFFI::Nulo;
        int linea = 0;
    };

    // Cada ámbito mapea nombre de variable → tipo anotado (Ninguno si sin anotación).
    std::vector<std::unordered_map<std::string, TipoAnotado>> ambitos;
    // PLAN_POO.md (Reto 6 / 4.10): pila paralela a 'ambitos' -- nombre de
    // variable → nombre de clase, solo para variables cuyo tipo concreto se
    // conoce en compilación (parámetro anotado "p: Perro", o última
    // asignación vista "p = nuevo Perro(...)"). Es "mejor esfuerzo", no un
    // análisis de flujo real: una reasignación a otra cosa borra el dato en
    // vez de intentar fusionar ramas -- el objetivo es habilitar el control
    // de acceso del Reto 6 solo cuando hay certeza razonable, nunca inventar
    // un tipo. Se empuja/saca junto con 'ambitos' en entrarAmbito/salirAmbito.
    std::vector<std::unordered_map<std::string, std::string>> clasesVariable;
    std::unordered_map<std::string, InfoFuncion> funciones;
    // PLAN_FFI.md (F4): tabla separada de funciones "externo" -- un nombre no
    // puede estar a la vez en `funciones` y en `funcionesExternas` (ver
    // recolectarFunciones, que detecta la colisión en cualquier orden de
    // declaración dentro del mismo Programa).
    std::unordered_map<std::string, InfoFuncionExterna> funcionesExternas;
    std::unordered_set<std::string> constantes;
    std::vector<ErrorSemantico> errores;

    enum class TipoInfoKind { Clase, Estructura, Interfaz };

    struct InfoMetodo {
        std::string nombre;
        std::vector<TipoAnotado> parametros;
        std::vector<std::string> parametrosClase;
        TipoAnotado tipoRetorno = TipoAnotado::Ninguno;
        std::string tipoRetornoClase;
        bool esConstructor = false;
        bool esAbstracto = false;
        bool esEstatico = false;
        bool esSobreescritura = false;
        // PLAN_POO.md (Reto 6 / 4.10): modificador de acceso del método, para
        // el control de acceso "mejor esfuerzo" de visitar(AccesoMiembro&).
        ModificadorAcceso acceso = ModificadorAcceso::Publico;
        int linea = 0;
    };

    struct InfoTipo {
        TipoInfoKind tipo = TipoInfoKind::Clase;
        bool esAbstracta = false;
        std::string padre;
        std::vector<std::string> interfaces;
        // PLAN_POO.md (Reto 6 / 4.10): nombre -> modificador de acceso (antes
        // era un unordered_set<string>, solo de nombres -- se necesita el
        // modificador para el control de acceso).
        std::unordered_map<std::string, ModificadorAcceso> campos;
        std::unordered_map<std::string, InfoMetodo> metodos;
        std::vector<ParametroGenerico> genericos;  // PLAN_GENERICOS.md: <T, U: Bound> de la propia clase/estructura/interfaz
        int linea = 0;
    };

    std::unordered_map<std::string, InfoTipo> tipos;
    std::string tipoActual;
    bool enClase = false;
    bool enMetodoInstancia = false;
    bool enConstructor = false;

    // PLAN_GENERICOS.md: pila de parámetros genéricos activos (nombre -> info),
    // uno por ámbito de declaración genérica anidado (clase genérica + su
    // propio método genérico, por ejemplo). Un nombre es válido como tipo si
    // aparece en CUALQUIER nivel de la pila, no solo en el tope.
    std::vector<std::unordered_map<std::string, ParametroGenerico>> genericosActivos;

    int profundidadBucle;
    int profundidadFuncion;
    int profundidadVariadica;
    // PLAN_FFI.md (F4): > 0 dentro de un bloque "inseguro" o de una función
    // marcada "inseguro" (incluye bloques normales anidados adentro, igual
    // que profundidadFuncion/profundidadVariadica -- no se resetea al entrar
    // a un "si"/"mientras" normal). Toda Llamada resuelta contra
    // funcionesExternas se valida contra este contador.
    int profundidadInseguro;

    void entrarAmbito();
    void salirAmbito();
    void declararVariable(const std::string& nombre, TipoAnotado tipo, int linea, bool esConst = false);
    bool estaDeclarada(const std::string& nombre) const;
    void usarIdentificador(const std::string& nombre, int linea);
    bool esIncorporada(const std::string& nombre) const;
    bool esLibreria(const std::string& nombre) const;
    void recolectarFunciones(Programa& programa);
    void recolectarTipos(Programa& programa);
    void analizarBloque(ListaSent& cuerpo);
    void analizarMetodo(MetodoDef& metodo);
    void validarTipoObjeto(TipoAnotado tipo, const std::string& clase, int linea);
    bool estaTipoDefinido(const std::string& nombre) const;
    const InfoTipo* obtenerTipo(const std::string& nombre) const;
    void agregarError(int linea, const std::string& mensaje);

    // --- Genéricos (PLAN_GENERICOS.md) -------------------------------------
    // Empuja un nuevo ámbito de parámetros genéricos activos (declaración de
    // clase/estructura/interfaz/función/método genérica), validando que cada
    // bound nombre una interfaz ya definida. Debe emparejarse con salirGenericos().
    void entrarGenericos(const std::vector<ParametroGenerico>& genericos);
    void salirGenericos();
    bool esGenericoActivo(const std::string& nombre) const;

    // Verifica que el tipo concreto `concreto` (nombre de tipo primitivo o de
    // clase/estructura) satisfaga los bounds de `g`. No hace nada si `g` no
    // tiene bounds. Sin efecto (degradación gradual) si `concreto` está vacío.
    void validarBoundConcreto(const std::string& concreto, const ParametroGenerico& g, int linea);
    bool tipoImplementaInterfaz(const std::string& nombreTipo, const std::string& interfaz) const;

    // Nombre de tipo concreto estático de una expresión, usado para inferir
    // parámetros genéricos en un sitio de llamada: nombre de tipo primitivo
    // para literales ("numero", "cadena", ...) o nombre de clase para
    // "nuevo Clase(...)". Cadena vacía si la expresión es dinámica (no se
    // puede determinar en compilación) — igual filosofía que Fase 27.
    static std::string nombreConcretoDeExpr(Expresion* e);

    // Tipo anotado de una variable ya declarada (TipoAnotado::Ninguno si no
    // está declarada o no tiene anotación). Usado para inferir un parámetro
    // genérico a partir de un argumento identificador ya anotado
    // (p.ej. "x: numero = 5" seguido de "identidad(x)").
    TipoAnotado tipoDeVariable(const std::string& nombre) const;

    // --- Control de acceso (PLAN_POO.md, Reto 6 / 4.10) --------------------
    // Registra (o borra, si 'clase' está vacío) el nombre de clase concreto
    // de 'nombre' en el ámbito activo -- ver el comentario de
    // 'clasesVariable' en la sección de miembros.
    void registrarClaseVariable(const std::string& nombre, const std::string& clase);
    // Busca 'nombre' en la pila de clasesVariable, de adentro hacia afuera.
    // "" si no hay ningún hint registrado (tipo dinámico/desconocido).
    std::string tipoClaseDeVariable(const std::string& nombre) const;
    // Nombre de clase estático de 'e' para control de acceso: "nuevo Clase()"
    // se resuelve directo; un Identificador consulta tipoClaseDeVariable().
    // "" si no se puede determinar (se salta el chequeo -- gradual).
    std::string nombreClaseEstaticaAcceso(Expresion* e) const;
    // Busca 'miembro' (campo o método) en 'tipoObjeto' y su cadena de
    // 'padre'; si lo encuentra y su modificador de acceso no es Publico,
    // reporta un error a menos que 'tipoActual' tenga permiso (privado: debe
    // ser exactamente la clase declarante; protegido: la clase declarante o
    // una subclase). Sin efecto si el miembro no se encuentra en ningún
    // nivel de la cadena -- esta función no valida existencia de miembros,
    // eso queda fuera de alcance (ver comentario en visitar(AccesoMiembro&)).
    void verificarAccesoMiembro(const std::string& tipoObjeto, const std::string& miembro, int linea);
};

#endif  // ANALIZADOR_SEMANTICO_H
