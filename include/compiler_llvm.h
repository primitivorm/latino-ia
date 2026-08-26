// compiler_llvm.h
//
// Backend de generación de código basado en LLVM (ver input/PLAN_LLVM.md).
// GeneradorLLVM construye el llvm::Module equivalente al AST de Latino, en
// paralelo al backend de C existente (GeneradorC, ver compiler.h) — ninguno
// de los dos reemplaza al otro; se seleccionan con --backend=c|llvm.
//
// Fase L5 (estado actual): `generar()` todavía emite el módulo "hola mundo"
// de plumbing de la Fase L1 (el recorrido real de `programa` -- funciones --
// llega en las Fases L6-L9). Lo nuevo de esta fase: `genSentencia()`/
// `genBloque()` traducen control de flujo (Si/Elegir/Mientras/Desde/Repetir/
// Romper) y sentencias-expresión a basic blocks reales de LLVM.
#ifndef COMPILER_LLVM_H
#define COMPILER_LLVM_H

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/IRBuilder.h>

#include "ast.h"
#include "runtime_abi_llvm.h"

namespace llvm {
class BasicBlock;
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

    // (Fase L5) Traduce una sentencia en el punto de inserción actual de
    // 'builder': Asignacion (delega en genAsignacion), ExprSentencia, y
    // control de flujo -- Si/osi/sino, Elegir, Mientras, Desde, Repetir,
    // Romper. Cada construcción de control de flujo emite basic blocks
    // reales (nunca los evita con trucos aritméticos) y deja 'builder'
    // posicionado en un bloque nuevo, sin terminador, listo para que la
    // sentencia siguiente continúe -- el mismo patrón que ya usan Ternaria
    // (Fase L4) y genBloque (más abajo) para saber cuándo un bloque ya
    // quedó terminado (p.ej. por un `romper` o, desde la Fase L6, un
    // `retornar`) y no debe recibir más instrucciones.
    //
    // No maneja Retornar (Fase L6: depende de la convención de retorno de
    // la función que contenga la sentencia, que todavía no existe) ni
    // declaraciones de nivel superior (FuncionDef/ClaseDef/EstructuraDef/
    // InterfazDef/Incluir/LlamadaBase -- Fases L6/L7/L8).
    //
    // `Elegir` se traduce como una cadena de comparaciones
    // (`lat_igual`+`lat_es_verdadero`), nunca como `SwitchInst` nativo de
    // LLVM: los valores de caso de Latino son expresiones dinámicas
    // evaluadas en runtime, no enteros constantes de tiempo de compilación
    // (Reto 8 del plan) -- usar `switch` cambiaría tanto la semántica como
    // el orden de evaluación de efectos secundarios.
    //
    // `Romper` emite un salto al bloque de salida del bucle contenedor más
    // interno (pila `pilaSalidasBucle_`, estado nuevo que no existe en
    // GeneradorC porque ahí el propio `break;` de C ya resuelve esto). Si
    // no hay bucle contenedor (no debería ocurrir: el analizador semántico
    // ya rechaza un `romper` fuera de un bucle antes de llegar aquí), no
    // emite nada.
    void genSentencia(Sentencia& s, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                       const std::unordered_map<std::string, llvm::Value*>& variables);

    // (Fase L5) Traduce cada sentencia de 'cuerpo' en orden, deteniéndose
    // (sin traducir el resto) en cuanto el bloque de inserción actual ya
    // tiene un terminador -- código muerto tras un `romper` (o, desde la
    // Fase L6, un `retornar`), exactamente como ese código ya es
    // inalcanzable en el `while`/`if` de C que emite GeneradorC, salvo que
    // en LLVM IR insertar instrucciones después de un terminador en el
    // mismo basic block es inválido (no solo muerto), así que hay que
    // detenerse explícitamente en vez de dejar que "no importe".
    void genBloque(const ListaSent& cuerpo, llvm::IRBuilder<>& builder, llvm::Module& modulo,
                    const std::unordered_map<std::string, llvm::Value*>& variables);

    RuntimeAbiLLVM& abi() { return *abi_; }
    llvm::LLVMContext& contexto() { return *contexto_; }

private:
    std::unique_ptr<llvm::LLVMContext> contexto_;
    std::unique_ptr<RuntimeAbiLLVM> abi_;

    // Pila de bloques de salida de los bucles que se están traduciendo en
    // este momento (Mientras/Desde/Repetir) -- el tope es el bucle más
    // interno, hacia donde salta un `romper` (ver genSentencia). Vacía
    // fuera de la traducción de un bucle.
    std::vector<llvm::BasicBlock*> pilaSalidasBucle_;

    // Evalúa 'celda' con lat_es_verdadero() y compara el resultado contra 0
    // -- el i1 que necesita CreateCondBr. Devuelve 'false' (i1) sin emitir
    // la llamada si 'celda' es nullptr (expresión de condición no
    // soportada; no debería ocurrir con AST real, pero evita construir IR
    // inválido). Centraliza un patrón que se repite en Ternaria (Fase L4) y
    // en Si/Mientras/Desde/Repetir (Fase L5).
    llvm::Value* genEsVerdadero(llvm::Value* celda, llvm::IRBuilder<>& builder, llvm::Module& modulo);

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
