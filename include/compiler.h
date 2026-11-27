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
    static std::string varC(const std::string& nombre);   // v_<nombre>
    static std::string funC(const std::string& nombre);   // lat_fn_<nombre>

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
    // Nombres declarados "global" a nivel superior (ver recolectarGlobalesExplicitos).
    // genFuncion/genMetodo los excluyen del hoisting de locales para que una
    // asignación a ese nombre dentro del cuerpo mute la celda de nivel
    // superior en vez de sombrearla.
    std::set<std::string> globalesExplicitos;

    // Funciones anónimas (expresiones  fun (...) ... fin  ): se numeran en
    // orden de aparición y sus definiciones C se emiten a nivel superior,
    // antes de main(), para que cualquier sitio que las referencie (una
    // asignación, un literal de lista, una llamada dinámica) ya tenga el
    // símbolo disponible.
    int contadorAnonimo = 0;
    std::vector<FuncionAnonima*> anonimas;              // orden de aparición
    std::unordered_map<FuncionAnonima*, int> indiceAnonimo;  // nodo -> índice
    // PLAN_HILOS: solo los cuerpos de `hilo` necesitan un trampolín con firma
    // nativa de hilo; las anónimas normales siguen siendo callables LatFnModulo.
    std::set<FuncionAnonima*> cuerposHilo;
    // PLAN_HILOS: etiqueta C del finally del trampolín actual, para que un
    // `retornar` dentro del cuerpo de un hilo salga limpiamente del hilo.
    std::vector<std::string> etiquetaFinHilo;
    // PLAN_HILOS: cerrojos tomados por `bloquear` en la función actual, para
    // liberarlos también si el cuerpo termina con `retornar`.
    std::vector<std::string> bloqueosActivos;
    // Número de cerrojos activos al entrar en cada bucle: un `romper` debe
    // liberar solo los cerrojos tomados DENTRO del bucle, no los que lo
    // contienen (esos siguen vigentes hasta el final del `bloquear`).
    std::vector<size_t> bloqueosPorBucle;
    void emitirLiberarBloqueos(size_t desde = 0);

    void recolectarFunciones(Programa& programa);
    void recolectarExterno(Programa& programa);
    void recolectarTipos(Programa& programa);
    void recolectarAnonimas(Programa& programa);
    void recolectarAnonimasLista(const ListaSent& cuerpo);
    void recolectarAnonimasEnSentencia(Sentencia* s);
    void recolectarAnonimasEnExpresion(Expresion* e);

    // PLAN_HILOS.md: al registrar una función anónima se decide también si se
    // emite como callable normal (LatFnModulo) o como trampolín de hilo
    // (función con la firma que espera pthread/qthreads, que llama al callable
    // con los argumentos capturados).
    void registrarAnonima(FuncionAnonima* f);
    std::string genHilo(HiloDef* h);


    // Nombre C de la i-ésima función anónima: lat_fn_anon_<i>.
    std::string anonimoC(FuncionAnonima* f);

    void genFuncionAnonima(FuncionAnonima* f, const std::string& nombreC);
    void genTrampolinHilo(FuncionAnonima* f, const std::string& nombreC);

    void emitir(const std::string& linea);   // escribe con sangría + salto
    std::string nuevoTemp();

    // Generación
    std::string genExpr(Expresion* e);
    void genSentencia(Sentencia* s);
    void genSentenciaImpl(Sentencia* s);
    void genBloque(const ListaSent& cuerpo);

    // PLAN_PERF: marco de raíces y cierre de ámbito con recolección.
    // 'nombres' son variables C (ya con prefijo v_) que permanecen vivas
    // durante toda la función; 'este' y 'lat_resto' se agregan si aplica.
    // variablesGlobalesC contiene solo nombres con storage global real
    // (globalesExplicitos también incluye nombres de función).
    std::set<std::string> variablesGlobalesC;
    void emitirMarcoRaices(const std::set<std::string>& nombres,
                           bool conEste = false, bool conResto = false);
    void emitirRetorno(const std::string& valorC);
    void emitirRetornoNulo();
    void genFuncion(FuncionDef* f);
    void genClase(ClaseDef* c);
    void genEstructura(EstructuraDef* e);
    void genInterfaz(InterfazDef* i);
    void genMetodo(const std::string& claseNombre, MetodoDef* metodo,
                   const std::string& padreNombre);
    void generarCuerpo(Programa& programa);  // genera funciones + main (sin preámbulo)

    // Utilidades
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
