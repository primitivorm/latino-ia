// compiler_llvm.h
//
// Backend de generación de código basado en LLVM (ver input/PLAN_LLVM.md).
// GeneradorLLVM construye el llvm::Module equivalente al AST de Latino, en
// paralelo al backend de C existente (GeneradorC, ver compiler.h) — ninguno
// de los dos reemplaza al otro; se seleccionan con --backend=c|llvm.
//
// Fase L4 (estado actual): `generar()` todavía emite el módulo "hola mundo"
// de plumbing de la Fase L1 (el recorrido real de `programa` -- funciones,
// control de flujo -- llega en las Fases L5-L9). Lo nuevo de esta fase:
// `genExpr()` ahora traduce también operadores binarios/unario/postfijo,
// ternario, acceso por índice/miembro y literales de lista/diccionario;
// `declararLocales()` implementa el hoisting total de variables (alloca en
// el entry block, inicializadas a lat_nulo(), ver Reto 3 y Reto 4 del plan);
// `genAsignacion()` traduce la sentencia de asignación simple y múltiple,
// incluida la verificación de tipo del tipado gradual opcional.
#ifndef COMPILER_LLVM_H
#define COMPILER_LLVM_H

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include <llvm/IR/IRBuilder.h>

#include "ast.h"
#include "runtime_abi_llvm.h"

namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm

class GeneradorLLVM {
public:
    GeneradorLLVM();
    ~GeneradorLLVM();

    // Devuelve el llvm::Module equivalente al programa, o nullptr si el
    // módulo construido no pasa la verificación de LLVM (llvm::verifyModule)
    // — eso indicaría un bug del propio generador, no un error del programa
    // de usuario (los errores de sintaxis/semántica ya se descartaron antes
    // de llegar aquí). El módulo devuelto vive mientras viva este
    // GeneradorLLVM: ambos comparten el mismo llvm::LLVMContext.
    std::unique_ptr<llvm::Module> generar(Programa& programa);

    // Traduce una expresión aislada a IR en el punto de inserción actual de
    // 'builder', declarando en 'modulo' las funciones del runtime que haga
    // falta (vía RuntimeAbiLLVM). Devuelve un puntero a la celda
    // %struct.LatValor resultante -- NUNCA un valor LLVM de struct por
    // registro: mantener esa representación uniforme (puntero a celda) es
    // lo único consistente con el ABI real descubierto en la Fase L2 (en
    // Windows x64/MSVC, LatValor se pasa/retorna por puntero, nunca por
    // valor) y con la firma uniforme por puntero que usarán los
    // métodos/funciones de usuario a partir de la Fase L8.
    //
    // Además de los literales/identificadores de la Fase L3, esta fase (L4)
    // traduce: Binaria/Unaria/PostOperador/Ternaria (Ternaria emite basic
    // blocks reales -- if/else con una celda de resultado compartida -- para
    // preservar la evaluación perezosa de un solo lado, igual que el
    // operador ?: de C que genera GeneradorC), AccesoIndice/AccesoMiembro
    // (ambos vía lat_obtener_indice), ListaLiteral/DiccionarioLiteral (vía
    // lat_lista_de/lat_dic_de, funciones variádicas reales del runtime -- se
    // les pasa el puntero de celda de cada elemento, igual que a cualquier
    // otra función que reciba LatValor, nunca el struct por valor) y VarArgs
    // ("..." dentro de [...] -- busca la celda "lat_resto" en 'variables',
    // que la Fase L6 poblará con el parámetro variádico de la función).
    //
    // 'variables' mapea identificadores ya declarados al puntero de su
    // celda; declararLocales() (más abajo) es quien las declara -- hasta
    // entonces quien llame arma la tabla a mano (así se prueba esta fase de
    // forma aislada). Devuelve nullptr para nodos de expresión que todavía
    // no se manejan (control de flujo, llamadas, POO, ... -- fases futuras)
    // o para un identificador/"lat_resto" ausente de 'variables'.
    llvm::Value* genExpr(Expresion& expr, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                         const std::unordered_map<std::string, llvm::Value*>& variables = {});

    // (Fase L4) Declara, en el entry block en el que ya esté posicionado
    // 'entryBuilder', una celda %LatValor inicializada con lat_nulo() por
    // cada nombre de 'nombres' -- Reto 3 del plan ("alloca fuera del entry
    // block" impide la promoción a SSA de mem2reg; por eso se exige un
    // IRBuilder separado del que avanza por basic blocks de control de
    // flujo) y Reto 4 ("hoisting total sin scoping por bloque": paridad
    // semántica exacta con GeneradorC::genFuncion/genMetodo/generarCuerpo,
    // no una mejora de lenguaje). 'nombres' se obtiene típicamente de
    // recolectarVariables() (recolector_variables.h) sobre el cuerpo de la
    // función/método/programa. Devuelve nombre -> celda, para fusionar (si
    // hace falta) con la tabla de parámetros que arme quien llame -- esta
    // función no sabe nada de parámetros, solo de variables locales.
    std::unordered_map<std::string, llvm::Value*> declararLocales(
        const std::set<std::string>& nombres, llvm::IRBuilder<>& entryBuilder, llvm::Module& modulo);

    // (Fase L4) Traduce una sentencia de asignación (simple: 1 destino/1
    // valor; o múltiple: "a, b = 1, 2", evaluando primero todos los valores
    // a celdas temporales y solo después asignando, igual que
    // GeneradorC::genSentencia) en el punto de inserción actual de
    // 'builder'. Si 'asign' trae una anotación de tipo (tipado gradual,
    // Fase 27), antepone una llamada a lat_verificar_tipo antes de asignar.
    // Cada destino Identificador debe tener ya su celda en 'variables'
    // (declararLocales la crea) -- esta función solo traduce la sentencia,
    // nunca declara variables nuevas.
    void genAsignacion(Asignacion& asign, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                        const std::unordered_map<std::string, llvm::Value*>& variables);

    RuntimeAbiLLVM& abi() { return *abi_; }
    llvm::LLVMContext& contexto() { return *contexto_; }

private:
    std::unique_ptr<llvm::LLVMContext> contexto_;
    std::unique_ptr<RuntimeAbiLLVM> abi_;

    // Traduce la asignación a un único destino (Identificador/AccesoIndice/
    // AccesoMiembro) del valor ya evaluado en 'valorCelda' -- equivalente a
    // GeneradorC::genAsignacionDestino. Identificador hace una copia de
    // struct (load+store, no una llamada al runtime: es una copia dentro
    // del mismo módulo, no un cruce de la frontera FFI, así que no hace
    // falta pasar por el ABI importado); AccesoIndice/AccesoMiembro llaman a
    // lat_asignar_indice.
    void genAsignacionDestino(Expresion& destino, llvm::Value* valorCelda,
                              llvm::IRBuilder<>& builder, llvm::Module& modulo,
                              const std::unordered_map<std::string, llvm::Value*>& variables);
};

#endif  // COMPILER_LLVM_H
