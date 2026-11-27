# PLAN_HILOS.md — Hilos estilo C# para Latino (v1)

## Objetivo

Agregar concurrencia al lenguaje usando palabras reservadas en español,
con una semántica cercana a `System.Threading.Thread`, `Thread.Join()` y
`lock` de C#, y una API explícita equivalente a la librería `hilos`.

El diseño respeta las dos reglas centrales del compilador:

1. **Una sola transformación del AST antes de la bifurcación de backend.**
   Los nodos nuevos se resuelven en las fases existentes (macros, módulos,
   semántico) y ambos backends (`GeneradorC` y `GeneradorLLVM`) producen
   código equivalente sin que el runtime cambie su interfaz pública.
2. **Ningún hilo toca el estado del GC sin sincronización.** El registro de
   bloques es global y está protegido por un mutex; la pila de raíces y los
   temporales son `thread-local`.

## Sintaxis

```latino
hilo t(x: 41, s: "hola")
    imprimir(x)
    imprimir(s)
fin
esperar t

global cerrojo = hilos.cerrojo()
bloquear(cerrojo)
    total = total + 1
fin
```

- `hilo nombre(p: expr, ...) ... fin`: crea un hilo y guarda su manejar en
  `nombre`. Los argumentos se evalúan **en el hilo creador** y se capturan
  **por valor** (semántica de C#). Un `retornar` dentro del cuerpo termina
  el hilo (no produce un valor observable).
- `esperar expr`: espera a que el hilo termine (`Thread.Join`) y libera su
  manejar. Solo se puede esperar una vez.
- `bloquear(expr) ... fin`: adquiere un cerrojo reentrante antes del cuerpo
  y lo libera al salir, incluso si el cuerpo ejecuta `retornar` o `romper`.
- API explícita: `hilos.crear(fn, args)`, `hilos.esperar(t)`,
  `hilos.dormir(segundos)`, `hilos.rendir()`, `hilos.cerrojo()`,
  `hilos.entrar(cerrojo, fn, args)`, `hilos.salir(cerrojo)` y
  `hilos.cerrojo_liberar(cerrojo)`.

Palabras reservadas nuevas: **`hilo`, `esperar`, `bloquear`**.

## Representación en runtime

- `LatHiloArgs { LatFnHilo fn; uint64_t nargs; LatValor* args; }`.
- Un manejar de hilo es un `LAT_PUNTERO` opaco (`LatHilo*`) — no se registra
  en el GC porque es memoria nativa sin propiedad.
- Un cerrojo es otro `LAT_PUNTERO` opaco (`LatCerrojo*`) basado en
  `CRITICAL_SECTION` (Windows) o `pthread_mutex_t` recursivo (Unix).
- `lat_hilo_crear` copia profundamente el arreglo de argumentos; el hilo
  publica esa copia como raíz antes de ejecutar el cuerpo.
- `lat_hilo_iniciar` separa creación de arranque para que el generador pueda
  publicar el manejar como raíz antes de que el hilo toque el GC.
- `lat_hilo_esperar` hace join y libera el manejar (idempotente).
- `lat_hilo_liberar_recursos()` libera el arreglo TLS de temporales al
  terminar cada trampolín (y en `lat_liberar_todo` para el hilo principal).
  Cada hilo que usa temporales tiene su propio `realloc`; sin esta llamada
  el bloque escapaba con el TLS al morir el hilo.

### Sincronización del GC

| Estado | Ámbito | Protección |
|---|---|---|
| `g_registro`, `g_limpiando`, `g_registroInicializado` | global | `g_gcMutex` |
| `g_marcosActivos` | global | `g_gcMutex` |
| `g_raices`, `g_temporales`, `g_numTemporales` | por hilo | TLS (`__declspec(thread)` / `__thread`) |

`lat_recoger` solo barre cuando `g_marcosActivos == 1` y el hilo actual es
dueño del único marco publicado. `lat_hilo_iniciar` incrementa el contador y
`lat_hilo_esperar` lo decrementa, de modo que los argumentos capturados no
pueden ser liberados en la ventana entre `pthread_create` y la publicación
del marco del hilo.

## Backends

- **C**: cada cuerpo de `hilo` se registra como función anónima y además
  genera un trampolín `static void* ..._hilo(void*)` con el marco de raíces
  del hilo (parámetros, locales y globales). Un `retornar` se baja a
  `goto <fin_del_hilo>`.
- **LLVM**: mismo trampolín con firma `ptr @lat_hilo_tramp_N(ptr)`; construye
  `LatHiloArgs` con GEPs sobre una `alloca`, nunca como struct por registro,
  y usa un bloque de salida dedicado para `retornar`. `lat_hilo_*` y
  `lat_hilos_*` se declaran a través de `RuntimeAbiLLVM` (actualizando
  `tools/abi_probe.c`).
- El modo `--jit` usa el mismo módulo y la DLL de runtime con símbolos
  exportados, por lo que hereda la implementación sin cambios.

Enlazado: `find_package(Threads)` en CMake y `-pthread` en los invocadores
GNU/Clang; Windows usa la API Win32 directamente.

## Estado

Implementado y verificado con LLVM 18 real en Linux:

- Sintaxis `hilo`/`esperar`/`bloquear` y API `hilos.*` en los backends C,
  LLVM AOT y LLVM JIT.
- Captura por valor de números, cadenas y listas; globales compartidas;
  exclusión mutua con `bloquear`.
- Interacción con las fases anteriores: las capturas se reescriben como
  cualquier otra expresión en `ResolutorModulos` (incluido `ns.X` de un
  `importar * como ns`), y un `quote` de macro puede generar sentencias
  `hilo`/`esperar` con higiene aplicada al nombre del manejar y a los
  parámetros capturados.
- `romper` y `retornar` dentro de `bloquear` liberan los cerrojos; un
  `romper` solo suelta los cerrojos tomados dentro del bucle que abandona.
- `ctest` completo en serie con LLVM habilitado: **153/153 pruebas en verde**
  (3840.61 s), incluidas `e2e_hilos`, `e2e_hilos_llvm`, `test_hilos_e2e` y
  `test_hilos_e2e_llvm`. Tras los últimos ajustes (raíces temporales para las
  capturas y liberación de cerrojos en `retornar`/`romper`) se repitieron las 7
  pruebas afectadas — todas en verde — y se reverificó el modo `--jit`.

### Gate de memoria (Valgrind)

`tools/valgrind.sh` incluye `ejemplos/hilos.lat` en su lista curada, así que
el feature queda cubierto por el gate de memoria de `PLAN_PERF.md` con ambos
backends. Se ejecutó además sobre 10 programas (ejemplo, capturas, cerrojos,
`retornar`, `romper`, macros, módulos, API explícita y dos casos de estrés de
200 y 100 hilos) para C y LLVM: **20/20 ejecuciones sin errores ni fugas**
(`definite`/`indirect` = 0 bytes), con `in use at exit: 0` en los casos de
estrés. Los cerrojos que el programa no libera explícitamente aparecen como
*still reachable* (una global viva al salir), no como fuga. Lo mismo aplica a
un hilo que nunca se espera: su manejar queda *still reachable* y la pila que
le asigna glibc, *possibly lost* (no `definite`), porque `esperar` es quien
hace join y libera el manejar; no se considera una fuga del feature.

Hallazgo real corregido durante esta validación: el arreglo de temporales es
TLS y se realloca bajo demanda; al terminar un trampolín, el puntero
desaparecía con el TLS sin liberar el bloque (25,600 bytes en 200 bloques en
el caso de estrés). Se agregó `lat_hilo_liberar_recursos()` al final de ambos
trampolines y del trampolín de `hilos.crear`, más su declaración en
`tools/abi_probe.c` para LLVM/JIT.

## Fuera de alcance de v1

- Canales, `Task`/`async`-`await`, semáforos, `Monitor.Wait/Pulse`,
  `Interlocked` ni cancelación cooperativa.
- Detección de datos compartidos sin cerrojo (no hay análisis de razas).
- Identificador de hilo, prioridades, afinidad ni `ThreadLocal<T>`.
- Cerrojos automáticos sobre objetos arbitrarios (`bloquear(objeto)` solo
  acepta el manejar opaco devuelto por `hilos.cerrojo()`).
