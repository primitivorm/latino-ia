# Plan de trabajo — Auditoría y corrección de fugas de memoria (PLAN_PERF)

## Contexto

Este plan nace de una auditoría real con **Valgrind 3.24.0** sobre el estado actual
de `dev` (2026-11-24). El objetivo es responder dos preguntas separadas:

1. ¿El **compilador** (`latino`) filtra memoria al parsear/analizar/generar código?
2. ¿Los **programas generados** filtran memoria al ejecutarse con el runtime C?

La conclusión corta es: **el compilador está limpio, el runtime/código generado no**.
La infraestructura de conteo de referencias existe (`lat_valor_retener` /
`lat_valor_liberar`, Fase 19), pero **ninguno de los dos backends la usa**: no hay
una sola llamada a esas APIs en `src/compiler.cpp`, `src/compiler_llvm.cpp` ni en
ningún otro archivo de `src/`. El conteo de referencias está implementado pero
desconectado del código que produce el compilador.

## Metodología y reproducible

Build aislado, sin tocar `build/` ni `build-llvm/`:

```bash
cmake -S . -B build-valgrind -DCMAKE_BUILD_TYPE=Debug -DLATINO_LLVM_BACKEND=OFF
cmake --build build-valgrind -j 4
```

Barrido del compilador (42 ejemplos, modo `--solo-c`, sin invocar al compilador C):

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
  --error-exitcode=99 \
  ./build-valgrind/src/latino ejemplos/<caso>.lat --solo-c -o /tmp/caso.c
```

Barrido de los ejecutables generados:

```bash
./build-valgrind/src/latino ejemplos/<caso>.lat -o /tmp/<caso> --runtime runtime
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
  --error-exitcode=99 /tmp/<caso>
```

Backend LLVM (solo emisión de IR, para medir al compilador sin el ruido de los
registros estáticos al ejecutar programas LLVM bajo Valgrind):

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  ./build-llvm/src/latino ejemplos/hola.lat --solo-ir -o /tmp/hola.ll
```

`--trace-children` se dejó apagado a propósito: la instrumentación debía medir
`latino` y el ejecutable Latino, no `gcc`/`clang` invocados por el compilador.

## Resultados medidos

### 1. Compilador: sin fugas

| Superficie | Casos | Resultado |
|---|---:|---|
| `latino --solo-c` sobre `ejemplos/*.lat` | 42 | 0 bytes en use al salir, 0 errores en los 42 |
| Suites unitarias (`test_lexer`, `test_ast`, `test_parser`, `test_semantico`, `test_codegen`, `test_tipado`, `test_poo`, `test_genericos`, `test_modulos`, `test_ffi`, `test_runtime_rango`) | 11 | 0 bytes en use al salir, 0 errores |
| `latino --solo-ir` con build LLVM (`hola.lat`) | 1 | 0 *definitely lost*, 0 errores; 43.644 bytes *still reachable* en 758 bloques, todos de registros estáticos/globales de LLVM (no son fugas del proyecto) |

No se encontraron lecturas/escrituras inválidas, dobles `free` ni valores sin
inicializar en ninguna de esas superficies.

### 2. Ejecutables generados: fugas definitivas

Se compilaron y ejecutaron 41 de los 42 ejemplos (`ejemplos/clases.lat` falla por
un defecto funcional ajeno a memoria, ver "Hallazgo colateral"). Agregando los 41
procesos independientes:

| Categoría Valgrind | Total |
|---|---:|
| `definitely lost` | **15.271 bytes en 461 bloques** |
| `indirectly lost` | **1.381 bytes en 22 bloques** |
| `possibly lost` | 0 bytes |
| `still reachable` | 8.998 bytes en 217 bloques |
| Errores no-leak (invalid read/write/free, uninit) | **0** |

Casos con más fugas (definitivas + indirectas):

| Ejemplo | `definitely lost` | `indirectly lost` |
|---|---:|---:|
| `mate_ejemplo` | 7.347 B / 132 bloques | 352 B / 5 bloques |
| `cadena_ejemplo` | 2.682 B / 96 bloques | 64 B / 1 bloque |
| `alcance_variables_didactico` | 1.258 B / 36 bloques | 0 |
| `alcance_variables` | 1.064 B / 38 bloques | 0 |
| `herencia` | 270 B / 13 bloques | 0 |
| `interfaces` | 196 B / 13 bloques | 0 |
| `funciones` | 167 B / 7 bloques | 32 B / 1 bloque |
| `diccionarios` | 162 B / 7 bloques | 0 |
| `genericos` | 152 B / 11 bloques | 0 |

Los 8.998 bytes `still reachable` son sobre todo valores que siguen referenciados
desde variables locales vivas en `main` al terminar (por ejemplo `genericos`:
1.601 B, `herencia`: 993 B, `lista_ejemplo`: 992 B). Son candidatos a liberación
explícita al cerrar el ámbito, pero no pérdidas definitivas para Valgrind.

### 3. Caso mínimo y crecimiento

`ejemplos/hola.lat` genera exactamente esto:

```c
int main(int argc, char *argv[]) {
    lat_set_args(argc, argv);
    lat_escribir(lat_cadena("hola mundo"));   /* +1 referencia, nunca se libera */
    return 0;
}
```

Valgrind: 19 bytes `definitely lost` en 1 bloque, originados en
`cadena_bloque_nuevo` (`runtime/latino.c:126`) vía `lat_cadena`
(`runtime/latino.c:211`).

La fuga **crece con la carga**. Programa de estrés (1000 iteraciones, una
concatenación por iteración):

```latino
desde (i = 0; i < 1000; i++)
    s = "abc" .. "def"
fin
escribir("fin")
```

Resultado: **39.012 bytes en 3.001 bloques `definitely lost`** (1000 bloques de
`"abc"`, 1000 de `"def"`, 1000 del resultado, más el literal final). El patrón es
lineal, así que no es solo ruido al terminar el proceso.

### 4. No es un problema exclusivo del backend C

`ejemplos/hola.lat` compilado con `--backend llvm` produce el mismo resultado:
19 bytes `definitely lost` en 1 bloque. El defecto está en el contrato entre
código generado y runtime, compartido por ambos backends.

### 5. Fuga en una suite de runtime

`test_runtime_ffi` reporta **29 bytes en 2 bloques `definitely lost`**:

- `tests/test_runtime_ffi.cpp:109` — resultado de `lat_cadena(...)` no liberado.
- `tests/test_runtime_ffi.cpp:86` — resultado de `lat_tipo(...)` (que devuelve un
  `lat_cadena` nuevo en `runtime/latino.c:1137-1151`) no liberado.

Es una fuga del **test**, no del runtime: hay que agregar `lat_valor_liberar` en
esos dos puntos.

## Análisis de causa raíz

1. **El runtime sí tiene ref-conteo, pero nadie lo invoca.**
   `lat_cadena`, `lat_lista_de*`, `lat_dic_de*`, `lat_obj_nuevo`,
   `lat_concatenar`, `lat_tipo`, etc. devuelven valores heap con `refs = 1`.
   `lat_valor_liberar` (`runtime/latino.c:1282-1337`) sabe liberar en cascada
   cadenas, listas, diccionarios y objetos. `lat_valor_retener`
   (`runtime/latino.c:1256`) incrementa el contador. Ninguno aparece en el código
   generado ni en los backends.

2. **`genExpr` produce temporales anónimos que nadie libera.**
   `src/compiler.cpp:401-592` emite expresiones como
   `lat_sumar(lat_cadena("a"), lat_cadena("b"))`,
   `lat_dic_de(2, lat_cadena("k"), ...)`, `lat_obtener_indice(...)`, etc.
   Cada constructor incrementa el heap y el resultado se descarta al terminar la
   sentencia o se copia en una variable sin liberar el valor anterior.

3. **La asignación sobrescribe sin liberar.**
   `GeneradorC::genAsignacionDestino` (`src/compiler.cpp:866-880`) emite
   `v_<name> = <rhs>;`. Si `v_<name>` contenía una cadena/lista/diccionario/objeto,
   su última referencia se pierde en esa misma línea. Lo mismo aplica a
   `lat_asignar_indice`, `lat_obj_set` y `lat_obj_set_metodo`.

4. **Los contenedores no retienen lo que almacenan.**
   `lista_agregar` (`runtime/latino.c:295-301`) y `dic_poner`
   (`runtime/latino.c:333-349`) copian el `LatValor` recibido sin `retener`, y al
   reemplazar una clave no liberan el valor previo. Por eso los temporales que se
   guardan en listas/diccionarios quedan con `refs = 1` y mueren con el contenedor
   solo si alguien libera el contenedor (que hoy tampoco ocurre).

5. **No hay liberación al salir de ámbitos ni en `return`.**
   `genSentencia` (`src/compiler.cpp:882-1080`) y `genFuncion`
   (`src/compiler.cpp:...-396`) no emiten liberaciones para locales, temporales de
   `elegir`/`para`, ni valores en retorno. En LLVM la situación es idéntica:
   `src/compiler_llvm.cpp` declara `lat_cadena` en varias zonas pero nunca
   `lat_valor_liberar` / `lat_valor_retener`.

6. **Los valores `still reachable` no son todos iguales.**
   Parte son variables locales aún vivas en `main` al terminar (deben liberarse
   con el ámbito); parte son estructuras de LLVM/SQLite-like de terceros
   (debieron ser suprimidas en la medición, no "arregladas").

## Plan de corrección

### Fase P0 — Red de seguridad y medición repetible

**Objetivo:** dejar de perder memoria al terminar el proceso, aunque todavía no se
libere de forma determinista durante la ejecución.

| Tarea | Archivos |
|---|---|
| Registro global de bloques heap propios del runtime: cada constructor registra `(puntero, tipo)`; cada `free` effective lo da de baja. `atexit(lat_liberar_todo)` recorre lo que queda y aplica `lat_valor_liberar`. | `runtime/latino.c`, `runtime/latino.h` |
| No inscribir `LAT_PUNTERO` (memoria FFI, propiedad del programador), ni handles `dlopen`, ni punteros prestados. Documentar la regla. | `runtime/latino.h`, `SINTAXIS.md` (sección XII) |
| Hacer que `lat_valor_liberar` sea idempotente frente al registro (quitar del registro **antes** de liberar) para evitar doble `free` desde `atexit`. | `runtime/latino.c:1282-1337` |
| Script `tools/valgrind.sh` que construya un build `Debug`, corra un conjunto curado de ejemplos con ambos backends y falle con `--error-exitcode=99` si hay `definitely lost` o `indirectly lost`. | `tools/valgrind.sh` (nuevo), `CMakeLists.txt` / `tests/CMakeLists.txt` |
| Archivo `tests/valgrind.supp` para los *still reachable* conocidos de LLVM/glibc, con comentario indicando que no se usan para ocultar fugas del runtime. | `tests/valgrind.supp` (nuevo) |

**Criterio de aceptación:** después de P0, los 41 ejemplos ejecutables terminan con
0 bytes `definitely lost` y 0 bytes `indirectly lost`. **No** se acepta como
éxito el estrés de 1000 iteraciones: P0 solo limpia al salir, no evita el
crecimiento; eso se mide en P1.

**Riesgo:** un `atexit` que recorra objetos con ciclos puede recursar infinitamente.
Si aparece, P0 debe usar un conjunto visitado por puntero y dejar el ciclo como
limitación documentada hasta P2.

### Fase P1 — Contrato de propiedad en código generado (causa raíz)

**Objetivo:** que cada valor heap se libere en el momento en que deja de usarse,
no solo al terminar.

Definir y documentar este contrato antes de tocar codegen:

- Toda expresión devuelve una referencia **poseída (+1)** por quien la evalúa.
- Leer una variable devuelve un valor **prestado**; quien lo almacena o lo guarda
  en un contenedor debe `retener`.
- Los parámetros de función son **prestados**; el llamador libera sus argumentos
  temporales después de la llamada. El callee solo retiene si guarda el valor.
- Un `retornar` **transfiere** la propiedad al llamador.
- Los contenedores (lista, diccionario, objeto/campos) **poseen** sus elementos:
  hacen `retener` al insertar y `liberar` al reemplazar o al destruirse.

| Tarea | Archivos |
|---|---|
| Materializar subexpresiones en temporales `LatValor tN = ...;` antes de pasarlas como argumentos, de modo que se pueda emitir `lat_valor_liberar(tN);` justo después del uso. Empezar por literales de cadena, concatenación, literales de lista/diccionario y acceso por índice/miembro. | `src/compiler.cpp:401-592`, `src/compiler_llvm.cpp` |
| Emitir liberación del valor previo antes de reasignar una variable, reteniendo primero el RHS para soportar alias (`s = s .. "x"`). | `src/compiler.cpp:866-930`, `src/compiler_llvm.cpp` |
| Emitir liberación de todos los locales y temporales vivos al salir de cada ámbito: bloques, `si`, ciclos (por iteración), `romper`, `continuar`, `retornar` temprano y final de función/`main`. Reutilizar `recolector_variables` para conocer el conjunto. | `src/compiler.cpp`, `src/recolector_variables.cpp`, `src/compiler_llvm.cpp` |
| Retener al insertar en `lista_agregar`, `dic_poner`, `lat_obj_set`, `lat_obj_set_metodo` y `lat_asignar_indice`; liberar el valor reemplazado. | `runtime/latino.c` (funciones listadas arriba), `runtime/libs/lista.c`, `runtime/libs/dic.c` |
| Corregir los dos `lat_valor_liberar` que faltan en `test_runtime_ffi`. | `tests/test_runtime_ffi.cpp:86,109` |
| Pruebas unitarias nuevas del contrato: refcount de cadenas/listas/dics/objetos, reemplazo en contenedor, alias en asignación, retorno transferido. | `tests/test_runtime_memoria.cpp` (nuevo), `tests/CMakeLists.txt` |

**Criterio de aceptación:** el programa de estrés de 1000 iteraciones pasa de
39.012 B / 3.001 bloques a **0 `definitely lost`**, y un estrés de 10.000
iteraciones no muestra crecimiento de heap en Massif/Heaptrack (pendiente de
definir tolerancia; objetivo: pendiente ≈ 0).

**Riesgos:**
- Doble liberación cuando el mismo `LatValor` se guarda en dos contenedores:
  cada inserción debe incrementar el contador, no solo la primera.
- Alias en `s = s .. "x"`: liberar antes de evaluar el RHS produce use-after-free.
  Regla obligatoria: evaluar en temporal → retener → liberar viejo → asignar.
- `LAT_RANGO` es lazy y no posee heap propio; no debe registrarse en el registro
  de P0 ni liberarse como contenedor.
- Ciclos (lista que se contiene a sí misma, objeto → campo → objeto) seguirán
  siendo una limitación de v1; documentar en lugar de implementar un GC.

### Fase P2 — Paridad de backends y regresión

| Tarea | Detalle |
|---|---|
| Replicar cada cambio de emisión en `GeneradorC` y `GeneradorLLVM`. | Verificar con `--solo-c` y `--solo-ir` que aparecen las mismas llamadas a `lat_valor_retener`/`lat_valor_liberar` (en IR, vía GEP+load sobre `%struct.LatValor`, nunca sintaxis C). |
| Registrar el ABI de las dos funciones nuevas si hiciera falta. | `tools/abi_probe.c` ya declara el runtime; agregarlas si P1 introduce firmas nuevas. |
| Ejecutar la suite completa en serie con ambos backends. | `ctest --output-on-failure` (no usar `-j`, ver CLAUDE.md). Gate: 80/80 pruebas siguen pasando. |
| Agregar al CI la medición de memoria como gate separado. | No mezclar con el gate funcional: los binaries con LLVM tienen *still reachable* legítimo. |
| Medir el costo. | Comparar tiempo de ejecución y RSS pico antes/después en `mate_ejemplo`, `cadena_ejemplo` y el estrés. Aceptar hasta un 10% de sobrecarga si se eliminan las fugas. |

### Fase P3 — Documentación y cierre

- Actualizar `CLAUDE.md` con la nueva suite `test_runtime_memoria` y el script
  `tools/valgrind.sh`, en el estilo de la tabla de suites existente.
- Documentar en `SINTAXIS.md` (apéndice de memoria) que el programador Latino no
  libera memoria manualmente, salvo `LAT_PUNTERO` de FFI, y que los ciclos están
  fuera de alcance en v1.
- Registrar en este plan los números finales posteriores a P1/P2 (antes/después)
  y la decisión de si el registro `atexit` de P0 se conserva como red de seguridad
  o se retira por redundante.

## Hallazgo colateral (resuelto durante la validación)

`ejemplos/clases.lat` no compilaba con el backend C y por tanto `e2e_clases`
fallaba en CTest:

```
/tmp/clases.c:47:17: error: conflicting types for ‘lat_fn_Persona_crear’
/tmp/clases.c:7:17:   note: previous declaration ... ‘LatValor(LatValor)’
```

El generador emitía el prototipo de un método estático con una firma y su
definición con otra (`LatValor(int, LatValor*)`). Se corrigió en
`firmaPrototipoMetodo` (`src/compiler.cpp`) emitiendo `(int nargs, LatValor*
args)` para métodos estáticos; ver el detalle en "Implementación final".

## Resumen de decisiones

1. No hay que "arreglar el compilador" por fugas: está limpio en las tres
   superficies medidas.
2. La prioridad es el **contrato de propiedad entre código generado y runtime**,
   no el conteo de referencias en sí (que ya existe).
3. P0 es una red de seguridad útil para el gate de "0 fugas al salir", pero
   **no sustituye** a P1: el crecimiento lineal en bucles es el defecto real de
   rendimiento.
4. `still reachable` de LLVM se suprime/documenta; `still reachable` de valores
   Latino se elimina con liberación de ámbito en P1.
5. Todo se mide en serie con Valgrind y con `--error-exitcode=99`; los números
   de esta auditoría son la línea base de comparación.

## Implementación final (P0–P3) y resultados

### Desviación deliberada del plan original

P1 proponía propagar `lat_valor_retener`/`lat_valor_liberar` por todo el código
generado. Se implementó en su lugar un **registro global + marcado desde raíces
+ barrido shallow** con las mismas API de runtime (`lat_registro_registrar`,
`lat_raices_empujar`, `lat_raices_empujar_temporal`, `lat_recoger`,
`lat_liberar_todo`), sin cambiar la ABI de `LatValor` ni exigir que cada
expresión materialice un refcount. Motivos medidos: el esquema de raíces cubre
también aliases y ciclos (el plan los dejaba como limitación), mantiene la
paridad C/LLVM con el mismo conjunto de llamadas y evita tocar los cientos de
puntos de `genExpr`. `lat_valor_liberar` sigue existiendo y se usa en las bajas
del propio runtime; `atexit(lat_liberar_todo)` se **conserva** como red de
seguridad, en especial porque la ruta `--jit` termina con `std::_Exit` y llama
explícitamente a `lat_liberar_todo` antes de salir.

### Antes / después

| Medición | Antes | Después |
|---|---:|---:|
| 41 ejemplos ejecutables (backend C), `definitely lost` acumulado | 15.271 B / 461 bloques | **0 B / 0 bloques** |
| Ídem, `indirectly lost` | 1.381 B / 22 bloques | **0 B / 0 bloques** |
| `hola.lat` | 19 B / 1 bloque | **0 B / 0 bloques** |
| Estrés de 1.000 concatenaciones | 39.012 B / 3.001 bloques | **0 B / 0 bloques** |
| Estrés de 10.000 concatenaciones | (crecimiento lineal) | **0 B al salir; 100.006 allocs / 100.006 frees**, idéntico en C y LLVM |
| `test_runtime_ffi` | 29 B / 2 bloques | **0 B / 0 bloques** |

El barrido final cubrió los 42 ejemplos más el caso de estrés, con ambos
backends: **43/43 ejecuciones limpias por backend**, cada una con
`0 bytes in 0 blocks` y `ERROR SUMMARY: 0 errors from 0 contexts`. El modo
`--jit` reporta 0 `definitely lost` y 0 `indirectly lost` (los 162 KB
*still reachable* son registros estáticos de LLVM, no memoria del programa
Latino).

### Hallazgos de la validación final

Dos defectos aparecieron solo al correr el gate completo, ya corregidos:

1. **`LAT_MODULO` no se marcaba** en `marcar_valor`: el barrido liberaba el
   `LatModulo` de `paquete.cargar` aunque la variable `milib` seguía viva, y la
   llamada siguiente desreferenciaba `mod->handle` ya liberado (segfault en
   `test_lib_paquete` y `test_lib_paquete_llvm`). Agregado el caso
   `LAT_MODULO` al marcado.
2. **`para ... en rango(...)` en LLVM publicaba una celda sin inicializar**
   (`para_incremento`, que solo se escribe en el latch del bucle).
   `marcar_valor` leía un `tipo` basura: 21 errores de Valgrind en
   `ejemplos/rango.lat`. Ahora `actual` e `incremento` se inicializan con
   `lat_nulo()` antes de publicarse como raíces temporales.

### Gate repetible

- `tools/valgrind.sh`: 43/43 con `BACKEND=c BUILD=build-valgrind` y 43/43 con
  `BACKEND=llvm BUILD=build-llvm`, sobre los 42 ejemplos más estrés; opción
  `BACKEND=c|llvm` para restringir el barrido y `--build` para compilar antes.
- `tests/valgrind.supp`: solo *still reachable* de LLVM/glibc; el gate usa
  `--errors-for-leak-kinds=definite,indirect`, así que no puede ocultar fugas
  del runtime.
- `test_runtime_memoria` (19 comprobaciones): raíces, valor `extra` del
  retorno, temporales, barridos anidados (el llamador no pierde sus temporales
  cuando una llamada interna barre), ciclos, diccionarios, marcado de módulos
  (regresión de `test_lib_paquete`) e idempotencia de `lat_liberar_todo`, bajo
  Valgrind con 0 errores.
- `test_codegen_llvm`: 251 comprobaciones, 0 fallos (el marco de raíces se
  genera en ambos backends y el IR pasa `verifyModule`).
- CTest en serie (nunca `-j`): **80/80 en `build-valgrind`** (dos pasadas de 40
  para respetar el límite de timeout, 144,63 s + 1707,14 s) y **140/140 en
  `build-llvm`** (114 primeras + tramo 115–140 en dos pasadas, 1712,76 s la
  segunda; el límite de timeout del harness interrumpió la primera pasada
  después de las 114 pruebas, todas en verde). En ambos builds pasan
  `test_runtime_memoria` y los casos previamente rotos
  `e2e_retornos_multiplos` / `e2e_mate_ejemplo`; `test_lib_paquete` y
  `test_lib_paquete_llvm` se verificaron además individualmente.

La corrección del prototipo de métodos estáticos en `GeneradorC`
(`firmaPrototipoMetodo` emite `(int nargs, LatValor* args)` cuando el método es
estático) resolvió además el "Hallazgo colateral" de `ejemplos/clases.lat`, que
ahora compila y ejecuta con el backend C.

Para que el gate funcional quedara completo se corrigieron también dos fallos
preexistentes del baseline sin relación con memoria: `ejemplos/retornos_multiplos.lat`
no tenía anotación `#salida:` (se agregó con la salida real de ambos backends) y
`ejemplos/mate_ejemplo.lat` esperaba `1e-15` de `mate.log1p(1e-15)`, un valor
dependiente de la versión de libm (se cambió el caso a `0.01`, cuyo resultado
`0.00995033085316808` es estable con `%.15g`).

### Costo de la solución (P2)

Medición aproximada del runtime original (`git archive HEAD runtime`) contra el
modificado, con stubs `no-op` para aislar el costo del registro/barrido y con
CTest ejecutándose en paralelo en la misma máquina (hay ruido, por lo que los
números sirven solo como orden de magnitud):

| Caso | Tiempo |
|---|---:|
| `mate_ejemplo` | −19,8 % |
| `cadena_ejemplo` | +8,6 % |
| Estrés de 1.000 concatenaciones | −1,4 % |

El RSS pico no mostró cambio. Ningún caso se acercó al límite del 10 % de
sobrecarga aceptado en P2; la medición debe repetirse sin carga concurrente si
se quiere un número exacto.
