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
    // (Fase L8) Métodos ESTÁTICOS (`NombreClase.metodo(...)`): dentro del
    // mismo bloque que resuelve `AccesoMiembro` con objeto `Identificador`,
    // se busca primero si el nombre nombra una clase/estructura conocida
    // (tablas `clases_`/`estructuras_`, pobladas por `recolectarTipos`) con
    // un método estático de ese nombre -- recorriendo la cadena de herencia
    // hacia arriba para una clase, igual que
    // `GeneradorC::genLlamada`. Si se encuentra, se traduce a una llamada
    // directa (`declararMetodo`) con los argumentos empaquetados en un
    // array contiguo (sin celda para "este": un método estático no la
    // recibe). Si no se encuentra, cae al despacho dinámico uniforme de
    // siempre (`lat_obj_llamar_metodo`).
    //
    // (Fase L8) `NuevoExpr` (`nuevo Clase(args...)`): crea la instancia
    // (`lat_obj_nuevo` con el ancestro más antiguo si es una clase con
    // herencia, luego `lat_obj_set_clase` por cada nivel hacia el hijo, para
    // que `lat_obj_es_instancia` reconozca a los ancestros), registra campos
    // con valor por defecto (`lat_obj_set`) y métodos de instancia
    // (`lat_obj_set_metodo` + `lat_funcion_nueva`) recorriendo la cadena de
    // herencia de la raíz hacia la hoja (para que los métodos/campos de la
    // clase derivada queden registrados en último lugar, sobreescribiendo a
    // los del padre), y por último invoca el constructor si existe --
    // mismo mapeo 1:1 que `GeneradorC::genExpr(NuevoExpr)`. Una estructura
    // sigue el mismo patrón sin herencia (sin recorrer ninguna cadena).
    // `EsExpr` (`expr es Clase`) es una llamada directa a
    // `lat_obj_es_instancia` envuelta en `lat_logico` -- no necesita
    // ninguna tabla, es una comprobación en tiempo de ejecución por nombre.
    // `AccesoEste` (`este`) busca la celda `"este"` en `variables`, poblada
    // por `genMetodo` para métodos de instancia.
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
    // (Fase L8) `LlamadaBase` (`base(args...)`, solo válida dentro de un
    // constructor): empaqueta `este` (celda `variables["este"]`) más los
    // argumentos evaluados en un array contiguo, igual convención que
    // `NuevoExpr`, y llama al constructor de `actualPadre_` (`declararMetodo`
    // sobre el `MetodoDef` marcado `esConstructor` de esa clase, buscada en
    // `clases_`) si existe uno -- si `actualPadre_` está vacío o la clase
    // padre no tiene constructor, no emite ninguna llamada (paridad exacta
    // con `GeneradorC::genSentencia(LlamadaBase)`, que en ese caso solo deja
    // un comentario).
    //
    // No maneja declaraciones de nivel superior distintas de FuncionDef
    // (ClaseDef/EstructuraDef/InterfazDef/Incluir -- Fase L8 las traduce vía
    // `genClase`/`genEstructura`/`genInterfaz`, siempre desde fuera de
    // `genSentencia`/`genBloque`) ni la propia FuncionDef (ver
    // `genFuncion`/`declararFuncion`, que la traducen desde fuera de
    // `genSentencia`/`genBloque`, nunca dentro de un bloque -- igual que
    // `GeneradorC::genSentencia`, que tampoco emite una FuncionDef anidada).
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

    // (Fase L8) Registra en las tablas internas clases_/estructuras_/
    // interfaces_ cada ClaseDef/EstructuraDef/InterfazDef de nivel superior
    // de 'programa' -- equivalente a GeneradorC::recolectarTipos. Debe
    // llamarse antes de traducir cualquier NuevoExpr/llamada a método
    // estático/genClase/genEstructura que dependa de esas tablas, igual que
    // GeneradorC::generarCuerpo llama a recolectarTipos antes de generar
    // cualquier cuerpo -- así una clase puede referenciar (con `nuevo` o un
    // método estático) otra definida más adelante en el mismo programa. No
    // desciende dentro de cuerpos de función/método (solo mira las
    // sentencias de nivel superior de 'programa').
    void recolectarTipos(Programa& programa);

    // (Fase L8) Declara (o recupera, si ya se declaró antes) el prototipo
    // LLVM del método 'metodo' de la clase/estructura 'claseNombre':
    // `void @lat_fn_<claseNombre>_<metodo.nombre>(ptr sret %ret, i32 %nargs,
    // ptr %args)`. Firma distinta de la que usa declararFuncion (Fase L6,
    // un puntero por parámetro fijo): aquí los argumentos llegan
    // empaquetados como un array contiguo de %struct.LatValor (`args`) más
    // su longitud real (`nargs`) -- necesario porque el despacho dinámico
    // (`lat_obj_llamar_metodo`, Fase L7) solo conoce el número de argumentos
    // de una llamada concreta en tiempo de EJECUCIÓN, nunca en tiempo de
    // compilación (a diferencia de una llamada directa a una función de
    // usuario). Esta firma es, a nivel de ABI, la misma que `LatFnModulo`
    // (`typedef LatValor (*LatFnModulo)(int, LatValor*)` en runtime/latino.h)
    // exige en la práctica en este target (LatValor por valor siempre vía
    // sret, Fase L2) -- por eso un puntero a esta función es válido para
    // pasarse a `lat_funcion_nueva`/`lat_obj_set_metodo` sin ningún ajuste.
    // Enlaza con linkage interno, con el atributo `sret` puesto a mano en el
    // parámetro 0 (igual que declararFuncion, Fase L6 -- aquí tampoco hay
    // ningún `.ll` de origen del que copiarlo). No consulta
    // `metodo.esAbstracto` (eso es responsabilidad de quien llama --
    // genMetodo/genClase/genEstructura -- igual que declararFuncion tampoco
    // sabe nada de si su función de usuario "debería" existir).
    llvm::Function* declararMetodo(const std::string& claseNombre, MetodoDef& metodo, llvm::Module& modulo);

    // (Fase L8) Traduce el cuerpo de 'metodo' (declarando su prototipo
    // primero, vía declararMetodo, idempotente igual que genFuncion). No
    // hace nada si 'metodo.esAbstracto' -- paridad exacta con
    // GeneradorC::genMetodo, que tampoco emite nada para un método
    // abstracto.
    //
    // Dentro del cuerpo:
    //  - Si el método no es estático, "este" es una celda local fresca
    //    poblada con una rama real (nargs > 0) ? args[0] : lat_nulo() --
    //    igual patrón de basic blocks que ya usa Ternaria (Fase L4): nargs
    //    es un i32 de EJECUCIÓN (parámetro real de la función), nunca una
    //    constante de compilación, así que hace falta una comprobación en
    //    tiempo de ejecución, a diferencia del relleno con lat_nulo() de una
    //    llamada directa a función de usuario (Fase L6), que sí conoce el
    //    conteo de argumentos en tiempo de compilación. `AccesoEste` (más
    //    abajo, en genExpr) busca esta celda en `variables["este"]`.
    //  - Cada parámetro declarado recibe el mismo tratamiento -- celda local
    //    fresca con (nargs > idx) ? args[idx] : lat_nulo() -- paridad exacta
    //    con GeneradorC::genMetodo. Ningún parámetro se lee directamente de
    //    `args` sin esa comprobación: hacerlo sería leer más allá del
    //    tamaño real del array que construyó el llamador cuando la llamada
    //    trae menos argumentos que parámetros declarados.
    //  - actualClase_/actualPadre_ (estado nuevo) se fijan a claseNombre/
    //    padreNombre mientras se traduce el cuerpo -- los usa
    //    genSentencia(LlamadaBase) para resolver el constructor de la clase
    //    padre -- y se restauran al salir, igual que
    //    GeneradorC::genMetodo/genClase/genEstructura.
    // Si el cuerpo no termina ya con un `retornar` explícito en todas sus
    // ramas, añade un `retornar nulo` implícito al final -- igual criterio
    // que genFuncion (Fase L6) y GeneradorC::genMetodo.
    void genMetodo(const std::string& claseNombre, MetodoDef& metodo,
                   const std::string& padreNombre, llvm::Module& modulo);

    // (Fase L8) genMetodo sobre cada método de 'c' (fijando
    // actualClase_/actualPadre_ a c.nombre/c.padre durante toda la
    // traducción, además del fijado interno de cada genMetodo individual --
    // doble fijado redundante pero inofensivo, replica exactamente
    // GeneradorC::genClase). Un método abstracto se salta (genMetodo no
    // emite nada para él).
    void genClase(ClaseDef& c, llvm::Module& modulo);

    // (Fase L8) Igual que genClase, pero sin clase padre: actualPadre_ se
    // limpia (una estructura nunca hereda) -- paridad exacta con
    // GeneradorC::genEstructura.
    void genEstructura(EstructuraDef& e, llvm::Module& modulo);

    // (Fase L8) No-op: una interfaz no tiene ningún cuerpo que traducir
    // (todos sus métodos son abstractos, sin excepción) -- paridad exacta
    // con GeneradorC::genInterfaz, que tampoco emite nada.
    void genInterfaz(InterfazDef& i);

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

    // (Fase L8) Tablas de tipos POO de nivel superior -- equivalentes a
    // GeneradorC::clases/estructuras/interfaces -- pobladas por
    // recolectarTipos(). NuevoExpr, la resolución de método estático
    // (genExpr(Llamada)) y LlamadaBase (genSentencia) las consultan.
    std::unordered_map<std::string, ClaseDef*> clases_;
    std::unordered_map<std::string, EstructuraDef*> estructuras_;
    std::unordered_map<std::string, InterfazDef*> interfaces_;

    // (Fase L8) Nombre de la clase/estructura y de la clase padre (vacío si
    // no hay o es una estructura) que genMetodo/genClase/genEstructura están
    // traduciendo en este momento -- equivalente a
    // GeneradorC::actualClase/actualPadre. LlamadaBase (genSentencia) usa
    // actualPadre_ para resolver el constructor de la clase base.
    std::string actualClase_;
    std::string actualPadre_;

    // (Fase L8) Copia (nargs > idx) ? args[idx] : lat_nulo() a una celda
    // local fresca -- el patrón que necesitan "este" y cada parámetro de un
    // método (genMetodo), replicando la lectura condicional de
    // GeneradorC::genMetodo con basic blocks reales (Ternaria, Fase L4) en
    // vez de un acceso directo a args[idx]: nargs es un i32 de EJECUCIÓN
    // (no se conoce en tiempo de compilación cuántos argumentos trajo la
    // llamada dinámica real), así que leer args[idx] sin comprobar primero
    // sería leer más allá del array que construyó el llamador cuando la
    // llamada trae menos argumentos que los que el método declara.
    llvm::Value* genArgumentoDeArray(llvm::Value* argsPtr, llvm::Value* nargs, size_t idx,
                                     llvm::IRBuilder<>& builder, llvm::Module& modulo,
                                     const std::string& nombreCelda);

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
