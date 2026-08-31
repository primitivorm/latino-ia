// compiler_llvm.h
//
// Backend de generación de código basado en LLVM (ver input/PLAN_LLVM.md).
// GeneradorLLVM construye el llvm::Module equivalente al AST de Latino, en
// paralelo al backend de C existente (GeneradorC, ver compiler.h) — ninguno
// de los dos reemplaza al otro; se seleccionan con --backend=c|llvm.
//
// Fase L7 (estado actual): `generar()` todavía emite el módulo "hola mundo"
// de plumbing de la Fase L1 (el recorrido real de `programa` -- el "main"
// generado -- llega en la Fase L9, cuando el driver lo necesita de verdad).
// Lo nuevo de esta fase: `genExpr()` traduce `Llamada` a los builtins
// (`escribir`/`imprimir`/..., `imprimirf`) y a las 7 bibliotecas
// (`cadena`/`lista`/`dic`/`mate`/`sis`/`archivo`/`paquete`) vía
// `AccesoMiembro`, además del despacho dinámico uniforme
// `lat_obj_llamar_metodo` para métodos de instancia/módulos dinámicos.
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
    // no se manejan (control de flujo, llamadas a builtins/bibliotecas/
    // métodos, POO, ... -- fases futuras) o para un identificador/
    // "lat_resto" ausente de 'variables'.
    //
    // (Fase L6) `Llamada` cuyo destino es un `Identificador` que nombra una
    // función de usuario ya registrada (por `declararFuncion`/`genFuncion`,
    // más abajo) se traduce a una llamada real: argumentos fijos ausentes se
    // completan con `lat_nulo()` (paridad con `GeneradorC::genLlamada`) y,
    // si la función es variádica, los argumentos sobrantes se empaquetan con
    // `lat_lista_de` antes de la llamada -- igual mecanismo que ya usan
    // `ListaLiteral`/`DiccionarioLiteral` (Fase L4) para invocar una función
    // variádica real del runtime, aplicado aquí a una función de usuario.
    //
    // (Fase L7) Antes de resolver contra una función de usuario, `Llamada`
    // con destino `Identificador` prueba primero los builtins (`escribir`/
    // `imprimir`/`escribe`/`poner`, `acadena`/`alogico`/`anumero`/`tipo`/
    // `error`/`incluir`, `leer`/`limpiar`, `imprimirf`) -- paridad exacta con
    // `GeneradorC::genLlamada`. `imprimirf` es la única que devuelve
    // `nullptr` como valor de forma intencional (no por no estar soportada):
    // `lat_imprimirf` retorna `void` en el runtime real (sin `sret`, a
    // diferencia de todo el resto del runtime), así que no hay ninguna celda
    // que devolver -- solo es válida como `ExprSentencia`, igual que en
    // `GeneradorC`. `Llamada` con destino `AccesoMiembro` cuyo objeto es un
    // `Identificador` que nombra una de las 7 bibliotecas
    // (`cadena`/`lista`/`dic`/`mate`/`sis`/`archivo`/`paquete`) se traduce a
    // `lat_<lib>_<fn>(args...)` (`cadena.formato` es variádica, empaquetada
    // igual que `imprimirf`). Cualquier otro `AccesoMiembro` (llamada a
    // método de una instancia u objeto de módulo dinámico cargado con
    // `paquete.cargar`) se traduce al despacho dinámico uniforme
    // `lat_obj_llamar_metodo(objeto, nombre, nargs, args...)` -- el mismo
    // fallback que usa `GeneradorC` tanto para métodos de instancia como
    // para funciones exportadas de un módulo dinámico (Reto 7 del plan), sin
    // necesitar ningún seguimiento de clases/estructuras porque el
    // diccionario de métodos ya vive en el propio objeto en runtime.
    // Métodos ESTÁTICOS (`NombreClase.metodo(...)`, que sí exigen resolver
    // en tiempo de compilación si `NombreClase` es una clase/estructura
    // conocida) quedan pendientes de la Fase L8: `GeneradorLLVM` todavía no
    // lleva una tabla `clases_`/`estructuras_` análoga a la de `GeneradorC`.
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
    // (Fase L6) `Retornar` copia el valor evaluado (o `lat_nulo()` si
    // `retornar` no trae valor) a la celda de retorno (`sret`) de la función
    // contenedora -- el estado privado nuevo `celdaRetorno_`, que
    // `genFuncion` fija antes de traducir el cuerpo -- y cierra el bloque
    // actual con `CreateRetVoid`. Si no hay función contenedora
    // (`celdaRetorno_` es `nullptr`; no debería ocurrir con AST real: el
    // analizador semántico ya exige que `retornar` esté dentro de una
    // función) no emite nada, igual que `Romper` sin bucle.
    //
    // No maneja declaraciones de nivel superior distintas de FuncionDef
    // (ClaseDef/EstructuraDef/InterfazDef/Incluir/LlamadaBase -- Fases
    // L7/L8) ni la propia FuncionDef (ver `genFuncion`/`declararFuncion`,
    // que la traducen desde fuera de `genSentencia`/`genBloque`, nunca
    // dentro de un bloque -- igual que `GeneradorC::genSentencia`, que
    // tampoco emite una FuncionDef anidada).
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

    // (Fase L6) Declara (o recupera, si ya se declaró antes) el prototipo
    // LLVM de la función de usuario 'f': `void @lat_fn_<nombre>(ptr sret
    // %ret, ptr %param0, ..., [ptr %lat_resto si f.variadico])` -- misma
    // convención "siempre puntero" que ya usan las funciones del runtime
    // (Fase L2: en Windows x64/MSVC, LatValor se pasa/retorna por puntero,
    // nunca por valor) aplicada ahora a las funciones que define el propio
    // programa Latino. El parámetro variádico ("lat_resto") es, igual que
    // en GeneradorC, la lista ya empaquetada por el llamador (Fase L4:
    // ListaLiteral/genExpr(Llamada) usan lat_lista_de) -- nunca varargs
    // nativos de LLVM. Registra la función en la tabla interna nombre ->
    // {Function*, numParámetros, variadico} para que genExpr(Llamada) y una
    // futura llamada a genFuncion (incluida la de la propia 'f', para
    // recursión directa) puedan resolverla. Enlaza con linkage interno
    // (equivalente al 'static' que ya usa GeneradorC::funC) -- por eso toda
    // llamada a esta función dentro de un módulo debe emparejarse con una
    // llamada a genFuncion() que le dé cuerpo antes de verificar el módulo:
    // una función de linkage interno sin cuerpo es IR inválido.
    llvm::Function* declararFuncion(FuncionDef& f, llvm::Module& modulo);

    // (Fase L6) Traduce el cuerpo de 'f' -- declara su prototipo primero
    // (vía declararFuncion, que es idempotente) si no existía ya, ANTES de
    // traducir el cuerpo, precisamente para permitir que 'f' se llame a sí
    // misma (recursión directa) o que otra función ya declarada la llame a
    // ella (recursión indirecta, si el llamador de genFuncion declaró antes
    // los prototipos de todo el grupo). Si el prototipo ya tiene cuerpo (una
    // llamada anterior a genFuncion con el mismo nombre), no hace nada más
    // -- evita generar el cuerpo dos veces.
    //
    // Cada parámetro entrante se COPIA a una celda local fresca -- nunca se
    // usa directamente como la celda de la variable: el puntero que llega
    // podría ser la celda real de una variable del llamador (genExpr de un
    // Identificador devuelve el puntero de la celda tal cual, sin copiar,
    // ver Fase L3), y Latino tiene semántica de paso por valor -- reasignar
    // el parámetro dentro del cuerpo no debe mutar la variable del llamador.
    // El resto de variables locales se declara con declararLocales sobre
    // recolectarVariables(f.cuerpo, ..., excluir=parámetros).
    //
    // Fija el estado privado celdaRetorno_ a la celda de retorno (el
    // parámetro sret) mientras traduce el cuerpo con genBloque -- así
    // genSentencia(Retornar) sabe adónde copiar el valor -- y lo restaura al
    // salir (paridad con el guardado/restaurado de actualClase/actualPadre
    // en GeneradorC::genMetodo, por si en el futuro genFuncion se invoca de
    // forma anidada). Si el cuerpo no termina ya con un `retornar` explícito
    // en todas sus ramas, añade un `retornar nulo` implícito al final --
    // igual que GeneradorC::genFuncion, que siempre emite `return
    // lat_nulo();` tras el cuerpo.
    void genFuncion(FuncionDef& f, llvm::Module& modulo);

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

    // (Fase L6) Información de una función de usuario ya declarada --
    // equivalente a GeneradorC::InfoFuncion (compiler.h), con el
    // llvm::Function* del prototipo agregado (GeneradorC no lo necesita:
    // ahí "declarar" es solo emitir texto, nunca un objeto que haya que
    // reutilizar).
    struct InfoFuncionUsuario {
        llvm::Function* fn;
        size_t numParametros;
        bool variadico;
    };
    std::unordered_map<std::string, InfoFuncionUsuario> funciones_;

    // (Fase L6) Celda de retorno (el parámetro sret) de la función que
    // genFuncion esté traduciendo en este momento; nullptr fuera de la
    // traducción de una función -- ver genSentencia(Retornar).
    llvm::Value* celdaRetorno_ = nullptr;

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
