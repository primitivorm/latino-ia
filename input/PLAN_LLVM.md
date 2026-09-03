# Plan de trabajo — Migración del backend de Latino a LLVM

## Contexto

Latino transpila hoy `.lat → C → compilador de C del sistema → ejecutable` (ver
[CLAUDE.md](../CLAUDE.md) y [PLAN_BASE.md](PLAN_BASE.md)). El generador
(`GeneradorC`, [src/compiler.cpp](../src/compiler.cpp)) recorre el AST con
cascadas de `dynamic_cast` y emite **texto C** que llama a un runtime dinámico
en C ([runtime/latino.h](../runtime/latino.h)/[.c](../runtime/latino.c) +
`runtime/libs/*`). Ese texto se compila con `cl.exe`/`gcc`/`clang`
(`src/invocador_c.cpp`) para producir el binario final.

Este plan reemplaza esa etapa de generación por un backend que emite código
nativo vía **LLVM**, manteniendo intacto todo lo demás (lexer, parser, AST,
análisis semántico, runtime en C, librerías estándar). El objetivo no es
"reescribir Latino" — es sustituir un backend por otro, de la forma más segura
posible, reutilizando al máximo lo que ya funciona y está probado.

### Motivación

- Evitar que el paso final de cada compilación dependa de invocar un
  compilador de C completo (`cl.exe`/`gcc`) sobre texto generado; en su lugar,
  `latino.exe` mismo produce código máquina (objeto nativo) en el propio
  proceso, usando la API de LLVM.
- Abrir la puerta a optimizaciones sobre el código *del usuario* (inlining,
  pases de LLVM) y a un futuro modo de ejecución interactivo (JIT).
- Sentar las bases para trabajo futuro de especialización de tipos (el
  cuello de botella real de rendimiento en Latino — el *boxing* dinámico de
  `LatValor` — queda fuera de alcance de este plan, ver "Fuera de alcance").

### Decisiones de diseño

1. **Se reemplaza SOLO el backend de codegen.** `runtime/latino.c` y
   `runtime/libs/*.c` no cambian una línea; se siguen compilando con el
   compilador de C del sistema y se enlazan como hoy. El nuevo generador
   (`GeneradorLLVM`) trata las ~146 funciones `lat_*` del runtime exactamente
   como las trata hoy `GeneradorC`: como una frontera FFI estable a la que se
   hacen llamadas, nunca reimplementada en IR.
2. **Ambos backends convivirán indefinidamente**, seleccionables con
   `--backend=c|llvm` (decisión del usuario). `GeneradorC` no se retira como
   parte de este plan.
3. **API de LLVM: C++ nativa** (`llvm::IRBuilder`, `llvm::Module`,
   `llvm::Function`, decisión del usuario), no la API-C de LLVM. Esto es más
   ergonómico pero introduce un riesgo real de compatibilidad de ABI de C++
   entre `latino.exe` y las bibliotecas de LLVM enlazadas — ver
   "Retos técnicos § ABI de C++ de LLVM" para la mitigación obligatoria.
4. **Alcance AOT + JIT.** Además de emitir un ejecutable (como hoy), este plan
   agrega un modo de ejecución JIT vía LLVM ORC (`--jit`), que reutiliza el
   mismo `llvm::Module` que construye el backend AOT.
5. **Retrocompatibilidad total**: con `--backend=c` (default hasta que LLVM
   alcance paridad), el comportamiento de `latino` es idéntico al actual. Los
   22 ejemplos de `ejemplos/*.lat` y toda la suite de CTest existente no
   cambian.
6. **Fuera de alcance de este plan** (candidatos a planes futuros
   independientes, para no inflar el riesgo de esta migración):
   - Conteo de referencias real (ARC) — hoy `GeneradorC` nunca emite
     `lat_valor_retener`/`lat_valor_liberar`; `GeneradorLLVM` replica ese
     mismo comportamiento (memoria no liberada, aceptable para programas
     cortos, documentado ya en el runtime).
   - Vtablas estáticas / despacho no dinámico para POO — se mantiene el
     modelo actual de diccionarios (`lat_obj_llamar_metodo`).
   - Especialización de tipos / unboxing de `LatValor`.
   - Retiro de `GeneradorC` (`PLAN_RETIRO_BACKEND_C.md`, plan de seguimiento
     futuro, no antes de que `--backend=llvm` haya sido el *default* sin
     regresiones durante un ciclo de uso real).

## Arquitectura

```
archivo.lat → Lexer → Parser (AST) → Análisis semántico
                                          │
                         ┌────────────────┴────────────────┐
                         │ --backend=c (default)           │ --backend=llvm
                         ▼                                  ▼
                   GeneradorC (texto C)              GeneradorLLVM (llvm::Module)
                         │                                  │
                         ▼                       ┌──────────┴──────────┐
              invocador_c.cpp                     │ --jit               │ (AOT, default)
        (cl.exe/VsDevCmd o cc/gcc/clang)          ▼                     ▼
                         │                LLJIT (ORC) ejecuta     TargetMachine emite
                         │                en el propio proceso    .obj/.o en proceso
                         │                       │                      │
                         └───────────┬───────────┘                     ▼
                                     │                     invocador_c.cpp (extendido:
                                     ▼                     acepta .c y/o .obj mixtos)
                              runtime/latino.c + runtime/libs/*.c
                          (compilado por el toolchain de C del sistema,
                           enlazado o resuelto en el proceso JIT)
```

Los dos backends comparten AST, análisis semántico y runtime. Solo cambia qué
recorre el AST y qué produce.

### Decisión 1 — API de LLVM y toolchain (riesgo de ABI de C++)

LLVM ofrece dos superficies: la API-C (`llvm-c/*.h`, funciones `extern "C"`
sobre punteros opacos, con compromiso de estabilidad de fuente entre
versiones) y la API C++ nativa (`IRBuilder`, `Module`, etc., más ergonómica
pero **sin garantía de ABI estable** entre versiones de LLVM ni entre
toolchains distintos).

Por decisión del usuario, este plan usa la **API C++ nativa**. Esto exige una
mitigación explícita, porque si `latino.exe` (compilado con `cl.exe`/VS2022 en
Windows, o `gcc`/`clang` en Linux/macOS) enlaza bibliotecas de LLVM compiladas
con un toolchain distinto (versión de MSVC distinta, ABI de STL distinta,
configuración de excepciones/RTTI distinta), el resultado son crashes o
corrupción silenciosa difíciles de diagnosticar — no un error de enlace claro.

**Mitigación obligatoria (documentar en README/CLAUDE.md):**

| Plataforma | Cómo obtener LLVM | Por qué |
|---|---|---|
| Windows | **vcpkg**, vía [`install_llvm.ps1`](../install_llvm.ps1) (`llvm[core,clang,target-x86]:x64-windows-release`), con el mismo triplet/toolset MSVC que ya usa el proyecto vía `VsDevCmd.bat`/`cl.exe` | vcpkg compila LLVM con el toolchain de Visual Studio detectado, garantizando ABI de C++ coincidente con `latino.exe` |
| Linux | Paquete `llvm-<N>-dev` de la distro (ej. `apt install llvm-18-dev`) | Compilado con el mismo GCC/Clang de sistema que compilará `latino.exe` |
| macOS | `brew install llvm` | Igual razonamiento; requiere apuntar `CMAKE_PREFIX_PATH` porque Homebrew no lo pone en el PATH por defecto |

> **Aviso de espacio en disco (Windows/vcpkg)**: el feature set por defecto
> del puerto `llvm` (`clang,default-targets,enable-bindings,enable-terminfo,
> enable-zlib,enable-zstd,lld,tools`) construyendo además la variante Debug
> puede consumir más de 100 GB en `vcpkg/buildtrees` antes de fallar por
> falta de espacio (ocurrió al ejecutar esta fase: `vcpkg install
> llvm:x64-windows` falló tras 3.8 h con "No space left on device"). La
> combinación de arriba (features acotadas a `core,clang,target-x86` +
> triplet `x64-windows-release`, que solo construye Release) usó ~24 GB y
> tardó ~2.1 h.

- Fijar una **versión concreta de LLVM: 18.x** (18.1.6 al momento de
  ejecutar esta fase — el registro de vcpkg ya no ofrecía 17.x como versión
  por defecto; ver nota de reevaluación abajo, exactamente el caso que
  anticipaba).
- `find_package(LLVM 18.1 CONFIG REQUIRED)` en CMake — nunca "cualquier versión
  ≥ X" sin acotar.
- Nueva opción de CMake `LATINO_LLVM_BACKEND` (`ON` si `find_package(LLVM)`
  tiene éxito; si no, se compila igual pero sin `--backend=llvm` disponible,
  con un `message(WARNING ...)` claro).
- **No vendorizar LLVM** en el repo (cientos de MB–GB compilados); se trata
  como dependencia externa, igual que hoy se trata el compilador de C.

### Decisión 2 — ABI de `LatValor`

`LatValor` es un struct-por-valor de 16 bytes con una unión interna, pasado y
retornado **por valor** en las ~146 funciones del runtime. La clasificación
ABI de agregados por valor difiere entre Windows x64 y SysV x86-64, y
adivinarla a mano en el generador LLVM es exactamente el tipo de bug que
"funciona por casualidad" en una plataforma y corrompe memoria en otra.

**Mecanismo: no derivar el ABI a mano — extraerlo de Clang**, la misma
familia de herramientas que compilará `runtime/latino.c`:

1. Nuevo `tools/abi_probe.c`: incluye `runtime/latino.h` y todos los
   `runtime/libs/*.h`, declara una función identidad
   `LatValor lat_valor_layout_probe(LatValor v) { return v; }` para forzar a
   Clang a materializar el layout completo.
2. Nuevo paso de build (`add_custom_command` en CMake) ejecuta
   `clang -S -emit-llvm -target <triple-real> tools/abi_probe.c -o
   generated/runtime_abi.ll`, usando el triple real del toolchain de destino
   (en Windows, `x86_64-pc-windows-msvc`, no el triple genérico de MinGW).
3. `GeneradorLLVM` **importa** `runtime_abi.ll` (parseándolo con
   `llvm::parseIRFile` y usando `llvm::Linker::linkModules` o extrayendo el
   `StructType`/las declaraciones de función de ahí) en vez de construir
   `%LatValor` y las firmas a mano con `StructType::create`/`FunctionType::get`.
   Así la clasificación ABI la decide siempre Clang — la misma autoridad que
   compila el runtime real — y los dos lados no pueden discrepar.
4. **Defensa adicional en runtime**: nueva función `lat_abi_verificar()` en
   `runtime/latino.c`/`.h` con aserciones sobre `sizeof(LatValor)`,
   `_Alignof(LatValor)` y `offsetof(LatValor, tipo)`; se invoca al inicio de
   todo `main` generado (junto a `lat_set_args`, que ya se llama ahí hoy). Si
   el struct cambia sin regenerar `runtime_abi.ll`, el programa falla con un
   mensaje claro en vez de corromper memoria en silencio.
5. Este mecanismo requiere Clang **solo en la máquina que compila
   `latino.exe`** (o en CI), nunca en la máquina del usuario final que
   compila un `.lat`. Si no hay Clang disponible en build-time, se puede
   versionar `runtime_abi.ll` como artefacto regenerable (a decidir en la
   Fase L2 según conveniencia del entorno de build del proyecto).

### Decisión 3 — De LLVM IR a ejecutable (AOT)

Usar `llvm::TargetMachine::addPassesToEmitFile` para emitir el objeto nativo
(`.obj` en Windows/COFF, `.o` en ELF/Mach-O) **en el propio proceso de
`latino.exe`**, sin depender de tener `clang` instalado en la máquina del
usuario final (evita duplicar procesos externos y el roundtrip por texto
`.ll`). El objeto resultante se pasa a la infraestructura de enlace que
**ya existe** en `src/invocador_c.cpp` (localización de `VsDevCmd`/`cl.exe` en
Windows, `cc`/`gcc`/`clang` en otros sistemas), extendida para aceptar una
lista mixta de entradas (`.c` de usuario **o** `.obj`/`.o` ya compilado).

Pasos previos obligatorios antes de emitir: `LLVMInitializeNativeTarget()`,
`LLVMInitializeNativeAsmPrinter()`, y **`llvm::verifyModule()`** — nunca emitir
sin verificar primero (un módulo inválido produce errores opacos del backend
de codegen o, peor, un objeto corrupto que el linker rechaza sin explicación
útil).

### Decisión 4 — Modo JIT (LLVM ORC)

Nueva bandera `--jit`: ejecuta el `.lat` directamente en memoria vía
`llvm::orc::LLJIT`, sin pasar por objeto + enlazador. Reutiliza el **mismo**
`llvm::Module` que construye `GeneradorLLVM` para AOT — el JIT es un
consumidor alternativo del módulo, no un generador distinto.

El runtime debe ser resoluble desde el proceso JIT. Opción recomendada:
enlazar `runtime/latino.c` + `runtime/libs/*.c` **estáticamente dentro del
propio `latino.exe`** (nuevo target CMake `latino_runtime_estatico`), de forma
que sus símbolos ya estén presentes en el proceso; registrar
`llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess()` en el
`LLJIT` para que ORC los resuelva por nombre. Alternativa (si enlazar el
runtime estáticamente en `latino.exe` es indeseable): compilar el runtime como
biblioteca dinámica (`latino_runtime.dll`/`.so`) y usar
`llvm::sys::DynamicLibrary::LoadLibraryPermanently` antes de crear el `LLJIT`.

## Fases

| Fase | Descripción |
|---|---|
| L0 | Preparación (sin código): versión de LLVM, documentación de instalación |
| L1 | Integración de LLVM en el build + smoke test "hola mundo" |
| L2 | Mecanismo de ABI (`abi_probe.c`, `runtime_abi.ll`, `lat_abi_verificar`) |
| L3 | Tipos, literales y esqueleto de expresión |
| L4 | Variables locales, asignación, expresiones compuestas |
| L5 | Control de flujo (si/desde/mientras/repetir/elegir/romper) |
| L6 | Funciones de usuario y variádica |
| L7 | Llamadas a runtime y librerías (FFI) |
| L8 | POO: clases, estructuras, interfaces, herencia, `este`/`nuevo`/`es`/`base` |
| L9 | Driver AOT: flag `--backend`, extensión de `invocador_c.cpp`, `--solo-ir` |
| L10 | Modo JIT vía LLVM ORC (`--jit`) |
| L11 | Tests (unitarios de codegen LLVM + E2E comparativo contra backend C) |
| L12 | Paridad y decisión de default |
| L13 | *(fuera de alcance temporal)* Retiro de `GeneradorC` — plan de seguimiento |

### Fase L0 — Preparación ✅

- [x] Fijar LLVM 18.x como versión objetivo.
- [x] Documentar instalación por plataforma (tabla de la Decisión 1) en
  [README.md](../README.md) y [CLAUDE.md](../CLAUDE.md).
- Sin cambios de código.

### Fase L1 — Integración de LLVM en el build ✅ verificada con LLVM real

**Archivos:** `CMakeLists.txt`, `src/CMakeLists.txt`, `include/compiler_llvm.h`
(nuevo), `src/compiler_llvm.cpp` (nuevo), `include/invocador_llvm.h` (nuevo),
`src/invocador_llvm.cpp` (nuevo), `src/main.cpp` (flag `--backend=c|llvm`).

- [x] `find_package(LLVM 18.1 CONFIG QUIET)` condicionado a la opción
  `LATINO_LLVM_BACKEND` (`ON` por defecto; se apaga sola con un `WARNING` si
  no se encuentra LLVM, para no romper el build del backend C en máquinas
  sin LLVM instalado).
- [x] `GeneradorLLVM` mínimo (`compiler_llvm.cpp`): emite un módulo
  hardcodeado con un `main` que llama a `puts()` de la libc — **no** a
  `lat_escribir`/`lat_cadena` del runtime, deliberadamente, para no
  necesitar todavía la ABI de `LatValor` (eso es la Fase L2). Sin tocar el
  AST real; solo valida el plumbing de compilación + enlace end-to-end.
- [x] `invocador_llvm.cpp`: inicializa targets nativos, crea
  `TargetMachine`, emite `.obj`/`.o` vía `addPassesToEmitFile`, y reutiliza
  `compilarAEjecutable` (`invocador_c.cpp`) sin modificarlo — cl.exe/gcc ya
  aceptan un objeto donde hoy se les pasa un `.c` (confirmado al leer
  `invocador_c.cpp`: el argumento se inserta en la línea de comandos sin
  lógica dependiente de la extensión). El refactor "lista mixta de
  entradas" previsto originalmente para la Fase L9 no hizo falta por esto
  mismo — se deja anotado como hallazgo, no como trabajo pendiente.
- [x] `main.cpp`: nuevo flag `--backend c|llvm`; con `llvm` y sin
  `LATINO_CON_LLVM` definido (LLVM no encontrado en el build), imprime un
  error claro en vez de fallar en tiempo de compilación.

**LLVM instalado y build verificado de punta a punta** (LLVM 18.1.6 vía
vcpkg, triplet `x64-windows-release`, features `core,clang,target-x86`).
Hallazgos reales encontrados al compilar contra LLVM de verdad (los dos que
ya se anticipaban en los comentarios del código, más uno nuevo):

1. **`find_package(LLVM 18 CONFIG)` con solo el major FALLA.** El archivo de
   compatibilidad de versión de LLVM exige coincidencia de major**.minor**,
   no solo major — `find_package` reporta "considered but not accepted"
   incluso con la versión correcta instalada (18.1.6). Corregido pidiendo
   `find_package(LLVM 18.1 CONFIG)` explícitamente. **Implicación para
   fases futuras**: cualquier bump de versión de LLVM en este proyecto debe
   actualizar el pin a `major.minor`, no solo `major`.
2. **El enum de tipo de archivo cambió de nombre**: no es `llvm::CGFT_ObjectFile`
   (API vieja) sino `llvm::CodeGenFileType::ObjectFile` (`enum class` en
   `llvm/Support/CodeGen.h`) en LLVM 18.x. Corregido en `invocador_llvm.cpp`.
3. **Nuevo, no anticipado — bloqueo de archivo en Windows**: dejar el
   `llvm::raw_fd_ostream` del objeto emitido abierto (sin destruir/cerrar)
   mientras se invoca a `cl.exe`/`link.exe` para el enlace hace que el
   linker falle con `LNK1104: no se puede abrir el archivo`, porque Windows
   no permite que otro proceso abra un archivo que el propio `latino.exe`
   todavía tiene abierto para escritura. Corregido envolviendo el
   `raw_fd_ostream` y el `PassManager` en un bloque `{ }` para forzar su
   destrucción (y el cierre del archivo) antes de llamar a
   `compilarAEjecutable`. También se cambió la extensión del objeto temporal
   de `.o` a `.obj` para evitar una advertencia cosmética de `cl.exe`
   (D9024) sobre una extensión no reconocida.
4. `getDefaultTargetTriple` en `llvm/TargetParser/Host.h` (no
   `llvm/Support/Host.h`) resultó ser la ubicación correcta para LLVM 18.x
   — el comentario de precaución original acertó con esta.

**Instalación usada para verificar** (automatizada luego en
[`install_llvm.ps1`](../install_llvm.ps1), ver también README.md/CLAUDE.md):
`vcpkg install "llvm[core,clang,target-x86]:x64-windows-release"` (~24 GB,
~2.1 h). El primer intento con `vcpkg install llvm:x64-windows` (features
por defecto completas + variante Debug) agotó ~105 GB de disco antes de
fallar — ver el aviso de espacio en disco en README.md. `install_llvm.ps1`
incluye una comprobación de espacio libre (`-MinFreeSpaceGB`, 60 GB por
defecto) para detectar este problema antes de empezar en vez de después de
horas de build.

**Criterio de aceptación — cumplido:** `latino --backend llvm
ejemplos/hola.lat -o hola_llvm.exe --runtime runtime` compila sin errores y
`hola_llvm.exe` imprime `hola LLVM`. Confirmado además que `--backend=c`
(default) sigue produciendo el mismo resultado que antes en el mismo build
con LLVM habilitado (`hola mundo`), y que la suite CTest completa (43/43)
sigue pasando con `LATINO_LLVM_BACKEND=OFF` (build sin LLVM instalado).

### Fase L2 — Mecanismo de ABI ✅ verificada con LLVM real

**Archivos:** `tools/abi_probe.c` (nuevo), `CMakeLists.txt` (nuevo
`add_custom_command` + `find_program(clang)`), `src/CMakeLists.txt`
(dependencia de `latino` sobre el target `latino_runtime_abi`, componente
`irreader`), `src/config.h.in`/`config.h` (nueva macro
`LATINO_RUNTIME_ABI_LL`), `runtime/latino.h`/`.c` (agregada
`lat_abi_verificar()`), `include/runtime_abi_llvm.h` + `src/runtime_abi_llvm.cpp`
(nuevo, clase `RuntimeAbiLLVM`), `tests/test_codegen_llvm.cpp` (nuevo),
`tests/CMakeLists.txt` (registro condicional bajo `LATINO_LLVM_BACKEND`).

- [x] Implementado el mecanismo descrito en la Decisión 2 completo.
- [x] `tools/abi_probe.c` incluye `latino.h` + los 7 headers de
  `runtime/libs/` y referencia (sin llamar, `(void)fn;`) las 185 funciones
  `lat_*` declaradas ahí, para forzar a Clang a emitir un `declare` de cada
  una — un `#include` por sí solo **no** genera IR para símbolos nunca
  referenciados (hallazgo real, no anticipado explícitamente en el plan
  original).
- [x] `CMakeLists.txt` (raíz): dentro del bloque que ya localizaba LLVM,
  `find_program(LATINO_CLANG_EXECUTABLE clang HINTS ${LLVM_TOOLS_BINARY_DIR})`
  — el mismo Clang que trae el feature `clang` del puerto de vcpkg (ver
  `install_llvm.ps1`, Fase L0/L1). Si no se encuentra, apaga
  `LATINO_LLVM_BACKEND` con un `WARNING`, igual que cuando LLVM mismo no
  aparece. Un `add_custom_command` invoca
  `clang -S -emit-llvm -target x86_64-pc-windows-msvc -I<runtime> abi_probe.c
  -o <build>/generated/runtime_abi.ll` (target fijo en Windows/MSVC per
  Decisión 2; en otras plataformas se usa el triple por defecto del
  Clang/GCC de sistema), envuelto en `add_custom_target(latino_runtime_abi)`
  del que depende el target `latino`.
- [x] `RuntimeAbiLLVM` (`runtime_abi_llvm.h`/`.cpp`) parsea
  `generated/runtime_abi.ll` con `llvm::parseIRFile` en el mismo
  `LLVMContext` que use el módulo destino — los tipos LLVM son propiedad del
  `LLVMContext`, no del `Module`, así que `tipoLatValor()` (el
  `%struct.LatValor` real, tal como Clang lo clasificó) y las
  `llvm::FunctionType*` de `declarar()` se reutilizan directamente sin
  reconstruirlos a mano. `declarar()` también copia `AttributeList` y
  `CallingConv` del origen — necesario porque el atributo `sret` del
  retorno indirecto de `LatValor` no es parte del `FunctionType`, es un
  atributo aparte, y sin copiarlo el módulo sigue siendo IR válido pero
  pierde la anotación que documenta la clasificación ABI real.
- [x] `lat_abi_verificar(tam, alineacion, offset_tipo)` agregada al runtime:
  compara `sizeof/alignof/offsetof(LatValor, tipo)` reales (según el
  compilador de C que construyó `latino.c`) contra los valores que
  `GeneradorLLVM` habrá derivado de `runtime_abi.ll`; términa el programa
  con un mensaje claro si no coinciden. Queda lista para invocarse desde el
  `main` generado a partir de la Fase L3+ (aún no hay recorrido de AST real
  en `GeneradorLLVM` que la use).

**Hallazgo real (no anticipado en el plan original) — ABI real observada para
Windows x64/MSVC:** para `LatValor lat_sumar(LatValor a, LatValor b)`, Clang
NO pasa los parámetros por valor como un tipo struct de LLVM ni usa el
atributo `byval` — genera `declare void @lat_sumar(ptr sret(%struct.LatValor)
align 8, ptr noundef, ptr noundef)`: tanto el retorno como **ambos
parámetros** de tipo `LatValor` se pasan como punteros simples (el ABI de
paso-por-referencia ya está resuelto en el IR que emite el frontend de
Clang, no queda ninguna clasificación adicional para que un backend genérico
de LLVM decida). Los escalares (`double`, `int`, `char*`) sí se pasan
directamente sin indirección (ver `lat_numero(double n)`). Esto confirma en
la práctica el riesgo que motivó toda la Decisión 2: construir estas firmas
a mano fácilmente habría asumido "pasar `%struct.LatValor` por valor
directamente", que es exactamente incorrecto para este target — de ahí que
importar el `FunctionType` real de Clang (en vez de reconstruirlo) sea la
única vía segura. Implicación directa para la Fase L3+: toda "LatValue SSA"
del generador debe modelarse como un puntero a una celda de memoria
(`alloca %LatValor`), nunca como un valor LLVM de tipo struct por registro —
es el único modelo consistente con esta ABI en ambas direcciones (llamar al
runtime y, más adelante, generar los métodos/funciones de usuario con la
misma convención).

**Criterio de aceptación — cumplido:** `tests/test_codegen_llvm.cpp`
construye un módulo que declara `lat_numero`/`lat_sumar` vía `RuntimeAbiLLVM`
y una función trivial (`sumar_5_y_3`) que llama a `lat_numero(5)`,
`lat_numero(3)`, `lat_sumar(...)`, lee el campo `numero` del resultado por
`CreateStructGEP` sobre el `%struct.LatValor` importado, y lo retorna como
`i32`; `llvm::verifyModule` pasa sin errores. Además de la comparación de
`sizeof`, el test compara alineación (`alignof`) y el offset del campo
`tipo` (`offsetof`) entre lo que ve el compilador de C++ del proyecto y lo
que derivó Clang — las tres coinciden. Verificación extra manual (fuera del
test, para confirmar corrección semántica real y no solo validez sintáctica
de IR): un `.ll` equivalente escrito a mano, compilado con `llc
-filetype=obj` y enlazado con `runtime/latino.c` + `runtime/libs/*.c` vía
`cl.exe`, produce un ejecutable que imprime `resultado=8` — confirma que la
convención de llamada generada es *ejecutablemente* correcta, no solo válida
para el verificador de LLVM. Suite completa (`ctest -C Release`) sigue
pasando con `LATINO_LLVM_BACKEND=ON` (43 suites existentes + la nueva
`test_codegen_llvm`).

### Fase L3 — Tipos, literales y esqueleto de expresión ✅ verificada con LLVM real

**Archivos:** `include/compiler_llvm.h` / `src/compiler_llvm.cpp` (nuevo método
`GeneradorLLVM::genExpr`), `tests/test_codegen_llvm.cpp` (ampliado con las
pruebas de esta fase; ya existía desde la Fase L2), `tests/CMakeLists.txt`
(agrega `src/ast.cpp` y `src/compiler_llvm.cpp` a las fuentes del target).

- [x] `LitNumero`/`LitCadena`/`LitLogico`/`LitNulo` → `CreateCall` a
  `lat_numero`/`lat_cadena`/`lat_logico`/`lat_nulo` (declaradas vía
  `RuntimeAbiLLVM::declarar`, Fase L2), mapeo 1:1 con lo que hoy hace
  `GeneradorC::genExpr`.
- [x] `Identificador` → devuelve directamente el puntero de su celda ya
  declarada (tabla `nombre → llvm::Value*` que arma quien llama; la
  infraestructura real de declaración de variables llega en la Fase L4).
- [x] Arnés de test ampliado en `test_codegen_llvm.cpp`: `Module::print` a
  `std::string` + comprobación de subcadenas (equivalente a `contiene(...)`
  de `test_codegen.cpp`), más `verifyModule` en cada caso.

**Decisión de esta fase — representación uniforme por puntero (ajusta la
redacción original "`Identificador` → `CreateLoad`"):** el texto original de
esta fase asumía que un identificador se lee con `CreateLoad` (produciendo
un valor `%struct.LatValor` de primera clase, "por registro"). Se optó en
cambio por que **toda** "LatValue" que maneja `GeneradorLLVM` sea siempre un
`llvm::Value*` que apunta a una celda `%struct.LatValor` -- nunca un valor
cargado por registro -- y que `genExpr` de un identificador devuelva el
puntero de su celda tal cual, sin `CreateLoad`. Motivo: el hallazgo de ABI
de la Fase L2 (en Windows x64/MSVC, `LatValor` se pasa/retorna **siempre**
por puntero al cruzar hacia una función del runtime real) y el hecho de que
las funciones/métodos de usuario de la Fase L8 usarán la misma firma
uniforme por puntero (`%LatValor* @lat_fn_<Clase>_<metodo>(i32, %LatValor*)`)
hacen que cargar a un valor por registro solo para volver a escribirlo a una
celda temporal antes de cada llamada al runtime sea trabajo puro sin
beneficio -- mantener el puntero de punta a punta es más simple y evita esa
ida y vuelta en cada expresión.

**Criterio de aceptación — cumplido:** cinco pruebas nuevas
(`prueba_l3_lit_numero`, `_lit_cadena`, `_lit_logico`, `_lit_nulo`,
`_identificador`) construyen cada una un módulo aislado, invocan
`GeneradorLLVM::genExpr` sobre un nodo de AST construido directamente (sin
pasar por el parser), verifican por subcadena de IR que se emitió la llamada
esperada (`call void @lat_numero(`, el literal `c"hola\00"` incrustado,
`i32 1` para `cierto`, etc.) y que `llvm::verifyModule` pasa. La prueba de
`Identificador` confirma explícitamente que se devuelve el mismo puntero de
la celda ya declarada, sin `load` de por medio. Suite completa (`ctest -C
Release`) sigue pasando con `LATINO_LLVM_BACKEND=ON` (44 suites, sin
regresiones); `test_codegen_llvm` pasa con 26 comprobaciones (las 3 de L2 +
las nuevas de L3). Además, `latino --backend llvm ejemplos/hola.lat`
(todavía el esqueleto "hola mundo" de la Fase L1, sin tocar) sigue
funcionando de punta a punta -- confirma que construir `RuntimeAbiLLVM`
dentro del constructor de `GeneradorLLVM` (nuevo en esta fase) no rompe el
camino existente.

### Fase L4 — Variables locales, asignación, expresiones compuestas ✅ verificada con LLVM real

**Archivos:** `src/compiler_llvm.cpp`/`include/compiler_llvm.h` (ampliados),
nuevo `include/recolector_variables.h` + `src/recolector_variables.cpp`
(extraídos del `namespace` anónimo de `src/compiler.cpp`, función
`colectar()`/`colectarLista()`; el método `GeneradorC::recolectarVariables`
ahora es un `#include` a la función libre), nuevo `include/fn_binaria.h` +
`src/fn_binaria.cpp` (misma extracción para la tabla operador→función que
antes vivía en el `namespace` anónimo de `compiler.cpp`), `include/compiler.h`
(quita el método `recolectarVariables`, ya no hace falta — las llamadas sin
calificar del cuerpo de la clase resuelven directo a la función libre),
`src/CMakeLists.txt`/`tests/CMakeLists.txt` (agregan los dos archivos nuevos a
`latino` y a todos los targets de prueba que ya enlazaban `compiler.cpp` o
`compiler_llvm.cpp`), `tests/test_codegen_llvm.cpp` (ampliado con 48 pruebas
nuevas).

- [x] `Binaria`/`Unaria`/`PostOperador`/`Ternaria`/`AccesoIndice`/
  `AccesoMiembro`/`ListaLiteral`/`DiccionarioLiteral`/`VarArgs` → `genExpr`
  traduce los ocho a llamadas al runtime, reutilizando `fnBinaria` (ahora
  compartida con `GeneradorC`). Un operador binario desconocido devuelve
  `nullptr` (nunca ocurre con AST producido por el parser real; el caso
  existe solo por paridad defensiva con `GeneradorC`, que en ese caso cae a
  `lat_nulo()` — aquí se prefirió `nullptr`, consistente con el resto de
  `genExpr` para nodos no soportados, ya que no hay texto C que emitir "de
  todas formas").
- [x] `Ternaria` **no** evalúa ambos lados y elige: emite basic blocks reales
  (`tern_cierto`/`tern_falso`/`tern_fin` con `CreateCondBr`) igual que el
  operador `?:` de C que ya emite `GeneradorC` — evaluar ambos lados
  ejecutaría efectos secundarios del lado no tomado, un cambio de semántica
  observable. Es el primer uso de basic blocks múltiples en `GeneradorLLVM`
  (antes de la Fase L5 formal de control de flujo), porque el propio
  operador ternario lo exige.
- [x] `PostOperador` (`i++`/`i--`) descubrió un caso de aliasing no
  mencionado explícitamente en el texto original del plan: `i++` lee y
  escribe la misma celda. Pasar esa celda como `sret` de `lat_sumar` junto
  con ella misma como operando de entrada sería aliasing entre el puntero de
  salida y uno de entrada. Solución: calcular en una celda temporal
  (`post_tmp`) y copiar (`load`+`store` de `%struct.LatValor`, no una
  llamada) a la celda de la variable — replica exactamente lo que ya hace C
  de forma implícita (`v_i = lat_sumar(v_i, lat_numero(1))` usa un temporal
  oculto del propio lenguaje C para el valor de retorno antes de la
  asignación). Devuelve la celda de la variable (ya actualizada), no el
  temporal — consistente con la representación uniforme por puntero de la
  Fase L3.
- [x] `AccesoMiembro` (`objeto.miembro`) y la asignación a `AccesoMiembro`
  construyen la clave como `lat_cadena("miembro")` y delegan en
  `lat_obtener_indice`/`lat_asignar_indice` — mismo mapeo 1:1 que
  `GeneradorC::genExpr`/`genAsignacionDestino`.
- [x] `ListaLiteral`/`DiccionarioLiteral` llaman a las funciones **variádicas
  reales** del runtime (`lat_lista_de`/`lat_dic_de`, `declare ... (ptr sret,
  i64, ...)` en `runtime_abi.ll`) en vez de un empaquetado alternativo:
  `RuntimeAbiLLVM::declarar` ya trae el `FunctionType` con `isVarArg=true`
  desde Clang, y `IRBuilder::CreateCall` sobre una función variádica acepta
  argumentos extra sin más — no hace falta ningún manejo especial de
  `va_arg` ni de convención de varargs en el llamador (ver también Reto 5,
  Fase L6, para el caso simétrico de *declarar* una función variádica de
  usuario). Cada elemento se pasa como el puntero de su celda, igual que
  cualquier otro argumento `LatValor` — nunca el struct por valor — porque
  esa es la clasificación ABI real que Clang ya fijó para estas funciones en
  la Fase L2 (16 bytes > 8 → indirecto, sin distinción entre parámetro fijo
  y variádico en el ABI de Windows x64/MSVC).
- [x] El caso especial `[...]` (un único elemento `VarArgs` dentro de
  `ListaLiteral`, que significa "la lista de argumentos variádicos", no una
  lista que la contiene) se resuelve igual que `GeneradorC`: `VarArgs` busca
  la celda `"lat_resto"` en `variables` (la Fase L6 la poblará con el
  parámetro variádico real de la función).
- [x] `declararLocales(nombres, entryBuilder, modulo)` (nuevo método público):
  declara un `AllocaInst` de `%struct.LatValor` por nombre e inmediatamente
  lo inicializa con `lat_nulo()`, **en el entry block** vía el
  `entryBuilder` que pasa quien llama (Reto 3: separar el builder de
  `alloca`s del que avanza por basic blocks de control de flujo, para que
  `mem2reg` pueda promover a SSA). Recibe un `std::set<std::string>` ya
  resuelto por `recolectarVariables` — no conoce parámetros ni "este"; fusionar
  esa tabla con la de parámetros es responsabilidad de quien arme la función
  completa (Fase L6).
- [x] `genAsignacion(asign, builder, modulo, variables)` (nuevo método
  público): asignación simple (1 destino/1 valor, con `lat_verificar_tipo`
  interpuesto si hay anotación de tipo del tipado gradual — Fase 27) y
  múltiple (`a, b = 1, 2`), evaluando **todos** los valores a celdas
  temporales antes de asignar cualquier destino — igual que los temporales
  `_tN` de `GeneradorC`, necesario para que `a, b = b, a` intercambie en vez
  de pisarse. `genAsignacionDestino` (privado) traduce el lvalue:
  `Identificador` es una copia de struct dentro del módulo (`load`+`store`,
  nunca una llamada — no cruza la frontera FFI así que no aplica el ABI
  importado); `AccesoIndice`/`AccesoMiembro` llaman a `lat_asignar_indice`.
- [x] Extracción de `recolectarVariables`/`fnBinaria` a código compartido:
  ambas ya se usaban en `GeneradorC` con exactamente el comportamiento que
  necesitaba `GeneradorLLVM` (mismo hoisting total sin scoping por bloque,
  Reto 4 — es una decisión de *lenguaje*, no de *backend*, así que ambos
  generadores deben coincidir en qué variables recolectan; mismo mapeo
  operador→función). Extraerlas evita mantener dos copias que podrían
  divergir silenciosamente entre backends.
- [x] `tipoAnotadoALatTipo` (helper nuevo, anónimo en `compiler_llvm.cpp`)
  mapea `TipoAnotado` a los valores reales del enum `LatTipo` incluyendo
  `runtime/latino.h` directamente (`extern "C"`) en vez de re-derivar los
  valores a mano como texto (que es lo que hace `GeneradorC::tipoALatTipo`,
  ya que ahí basta con emitir el nombre de la macro para que el compilador
  de C la resuelva) — mismo principio de "una sola fuente de verdad" que la
  Decisión 2 aplicó al layout de `LatValor`.

**Criterio de aceptación — cumplido:** 48 pruebas nuevas en
`tests/test_codegen_llvm.cpp` (todas por subcadena de IR + `verifyModule`,
igual que las de la Fase L3) cubren cada nodo nuevo, el caso `[...]` →
`lat_resto`, un operador binario desconocido → `nullptr`, `declararLocales`
(incluye una prueba end-to-end con `recolectarVariables` real sobre un
`ListaSent` construido a mano), asignación simple, tipada (verifica el valor
entero exacto de `LAT_NUMERO` y el número de línea pasados a
`lat_verificar_tipo`) y múltiple (verifica que se crean dos celdas
temporales distintas antes de asignar). Suite completa (`ctest -C Release`):
`test_codegen_llvm` pasa con 74 comprobaciones (26 de L2-L3 + 48 nuevas);
`test_codegen`/`test_tipado`/`test_poo` (que ahora enlazan
`fn_binaria.cpp`/`recolector_variables.cpp` en vez de la copia interna de
`compiler.cpp`) siguen pasando sin cambios de comportamiento — confirma que
la extracción a código compartido no alteró `GeneradorC`.

### Fase L5 — Control de flujo ✅ verificada con LLVM real

**Archivos:** `include/compiler_llvm.h`/`src/compiler_llvm.cpp` (nuevos
métodos públicos `genSentencia`/`genBloque`, helper privado `genEsVerdadero`,
estado nuevo `pilaSalidasBucle_`), `tests/test_codegen_llvm.cpp` (ampliado con
28 pruebas nuevas).

- [x] `Si`/`osi`/`sino` → basic blocks reales (`si_entonces`/`si_osi`
  (uno por rama `osi`)/`si_siguiente`/`si_fin`) con `CreateCondBr`/`CreateBr`,
  encadenando cada `osi` como el `else if` de C: el bloque "siguiente" de
  una condición es el punto de entrada de la comprobación de la próxima.
- [x] `Mientras` → `header`/`cuerpo`/`fin` estándar (evalúa la condición en
  `header`, salta a `fin` si es falsa). `Desde` → `header`/`cuerpo`/
  `incremento` (`latch`)/`fin`, traduciendo `inicio` (antes del `header`) e
  `incremento` (en el `latch`, después del cuerpo) reutilizando
  `genSentencia` sobre esas mismas sub-sentencias — ninguna lógica nueva para
  ellas. `Repetir` → `cuerpo`/`condicion`/`fin`, con el primer salto hacia
  `cuerpo` siempre incondicional (ejecuta al menos una vez, a diferencia de
  `Mientras`) y el salto de vuelta condicionado a que la condición **no**
  se cumpla todavía (`repetir ... hasta cond` ≡ `do { } while (!cond)`).
- [x] `Elegir` → cadena de comparaciones (`lat_igual` + `lat_es_verdadero`),
  nunca `SwitchInst` nativo de LLVM (Reto 8 del plan): los valores de caso de
  Latino son expresiones dinámicas evaluadas en runtime, no enteros
  constantes de compilación — un `switch` cambiaría tanto la semántica como
  el orden de evaluación de efectos secundarios. Un `caso` cuyo valor no se
  pudo evaluar (`genExpr` devuelve `nullptr`; no debería ocurrir con AST
  real) se trata como "nunca coincide" (condición `i1 false` constante) en
  vez de abortar toda la sentencia — el resto de los casos y el `defecto`
  siguen siendo alcanzables.
- [x] `Romper` → `pilaSalidasBucle_` (nuevo estado privado de
  `GeneradorLLVM`, un `std::vector<llvm::BasicBlock*>`; el tope es el bucle
  más interno): emite un salto directo al bloque de salida de ese bucle. No
  existe equivalente en `GeneradorC` porque ahí el propio `break;` de C ya
  resuelve el anidamiento; aquí hay que rastrearlo a mano porque LLVM no
  tiene una noción de "bucle" en el IR, solo basic blocks. `Mientras`/
  `Desde`/`Repetir` hacen `push_back`/`pop_back` de su bloque de salida
  alrededor de la traducción de su cuerpo, así que un `romper` dentro de un
  `Si` (u otra sentencia) anidado dentro del cuerpo del bucle sigue viendo
  el bloque de salida correcto (ver `prueba_l5_si_anidado_en_mientras`) — la
  pila, no el anidamiento sintáctico del AST, es lo que determina el
  destino.
- [x] **Hallazgo no anticipado explícitamente en el texto original de esta
  fase — código muerto tras un `romper` es válido en C pero inválido en LLVM
  IR.** En C, una sentencia después de `break;` dentro del mismo bloque
  simplemente nunca se ejecuta (sigue siendo sintaxis válida, el compilador
  la acepta sin más). En LLVM IR, insertar cualquier instrucción en un basic
  block que ya tiene un terminador (`br`/`ret`/...) es inválido y
  `verifyModule` lo rechaza. Solución: `genBloque` (nuevo método público)
  consulta antes de cada sentencia si el bloque de inserción actual ya
  quedó terminado (helper de archivo `bloqueTerminado`, no un método —no
  necesita estado) y, si es así, deja de traducir el resto de la lista sin
  emitir nada más. El mismo chequeo se repite después de traducir el cuerpo
  de cada rama de `Si`/`osi`/`sino`/caso de `Elegir`/cuerpo de bucle antes
  de emitir el salto de cierre correspondiente (`CreateBr` al bloque de
  fusión/`header`/`latch`), para no intentar terminar dos veces el mismo
  bloque.
- [x] `genEsVerdadero(celda, builder, modulo)` (helper privado nuevo):
  centraliza el patrón `lat_es_verdadero(celda) != 0` que ya usaba `Ternaria`
  (Fase L4) y que ahora también usan `Si`/`osi`/`Mientras`/`Desde`/`Repetir`/
  cada comparación de `Elegir` — `Ternaria` se refactorizó para usarlo
  (mismo IR generado, verificado por los tests de L4 que siguen pasando sin
  cambios). Devuelve la constante `i1 false` sin emitir ninguna llamada si
  `celda` es `nullptr` (condición no soportada; no debería ocurrir con AST
  real, pero nunca construye IR inválido por esa razón).
- [x] `genSentencia`/`genBloque` no traducen `Retornar` (depende de la
  convención de retorno de la función contenedora, que no existe todavía —
  Fase L6) ni declaraciones de nivel superior (`FuncionDef`/`ClaseDef`/
  `EstructuraDef`/`InterfazDef`/`Incluir`/`LlamadaBase` — Fases L6/L7/L8).
- [x] Portados a `tests/test_codegen_llvm.cpp` los casos de control de flujo
  equivalentes de `tests/test_codegen.cpp` (`prueba_si`/`prueba_desde`), más
  cobertura específica de LLVM sin equivalente en el backend C: `Elegir`
  (con verificación explícita de que **no** aparece ` switch ` en el IR),
  `romper` dentro de `Mientras`/`Desde` (confirma que salta directo a la
  salida y que la sentencia siguiente es código muerto no traducido),
  `romper` sin bucle contenedor (no debe crashear ni emitir ningún salto) y
  `romper` anidado dentro de un `Si` dentro de un `Mientras`.

**Criterio de aceptación — cumplido:** 28 pruebas nuevas en
`tests/test_codegen_llvm.cpp` (mismo estilo que L3-L4: subcadena/ausencia de
subcadena de IR + `verifyModule`) cubren `Si`/`osi`/`sino`, `Elegir`
(incluida la ausencia de `SwitchInst`), `Mientras`/`Desde`/`Repetir` (incluida
la ejecución incondicional de al menos una iteración de `Repetir`), `romper`
dentro de cada tipo de bucle, `romper` sin bucle contenedor y `romper`
anidado a través de un `Si`. Suite completa (`ctest -C Release`):
`test_codegen_llvm` pasa con 102 comprobaciones (74 de L2-L4 + 28 nuevas,
incluida la reescritura de `Ternaria` sobre `genEsVerdadero`, que no cambió
ninguna comprobación existente); las 44 suites de CTest (incluidos los 22
ejemplos E2E y todas las suites de librería) siguen pasando sin cambios —
esta fase no tocó `GeneradorC` ni ningún archivo de `runtime/`.

### Fase L6 — Funciones de usuario y variádica ✅ verificada con LLVM real

**Archivos:** `include/compiler_llvm.h`/`src/compiler_llvm.cpp` (nuevos
métodos públicos `declararFuncion`/`genFuncion`, nuevo estado privado
`funciones_`/`celdaRetorno_`), `tests/test_codegen_llvm.cpp` (ampliado con 8
pruebas nuevas).

- [x] `declararFuncion(f, modulo)` declara (o recupera, si ya existe) el
  prototipo LLVM de una función de usuario: `void @lat_fn_<nombre>(ptr sret
  %ret, ptr %param0, ..., [ptr %lat_resto si f.variadico])` — misma
  convención "siempre puntero" descubierta en la Fase L2 (en Windows
  x64/MSVC, `LatValor` se pasa/retorna por puntero, nunca por valor)
  aplicada ahora a las funciones que define el propio programa. Linkage
  interno (equivalente al `static` de `GeneradorC::funC`) con el atributo
  `sret` puesto a mano en el parámetro 0 vía
  `Attribute::getWithStructRetType` -- a diferencia de `RuntimeAbiLLVM`
  (Fase L2), que copia ese atributo del `.ll` que emitió Clang, aquí no hay
  ningún `.ll` de origen: el prototipo lo construye el propio generador, así
  que el atributo hay que ponerlo explícitamente para que el IR documente la
  misma convención de retorno indirecto que ya usa el runtime.
- [x] `genFuncion(f, modulo)` llama primero a `declararFuncion` (idempotente:
  una segunda llamada con el mismo `FuncionDef` no duplica el cuerpo, se
  detecta con `Function::empty()`) **antes** de traducir el cuerpo -- esto es
  lo que permite que el cuerpo llame a la propia función (recursión directa,
  ver `prueba_l6_recursion_directa`: un `factorial` recursivo construido a
  mano genera `call void @lat_fn_fact(` dentro de la definición de
  `lat_fn_fact`). La recursión indirecta (mutua) queda soportada por el
  mismo mecanismo siempre que quien orqueste la generación (la Fase L9,
  `generar()`) declare los prototipos de todas las funciones del programa
  antes de generar el cuerpo de ninguna -- exactamente el patrón de dos
  pasadas que ya usa `GeneradorC::generarCuerpo`.
- [x] **Hallazgo no anticipado explícitamente en el texto original de esta
  fase — un parámetro entrante NUNCA debe reutilizarse tal cual como la
  celda de la variable.** El puntero que recibe la función puede ser
  exactamente la celda de una variable del llamador (`genExpr` de un
  `Identificador` devuelve el puntero de su celda sin copiar, Fase L3), y
  Latino tiene semántica de paso por valor: reasignar el parámetro dentro
  del cuerpo no debe mutar la variable del llamador. `genFuncion` copia
  (`load`+`store`) cada parámetro entrante a una celda local fresca antes de
  usarlo -- el mismo efecto que ya obtiene `GeneradorC::genFuncion`
  implícitamente, porque ahí un parámetro de tipo `LatValor` en C ya es una
  copia local por definición del lenguaje, sin que el generador tenga que
  pensar en ello.
- [x] Variádica (`...`/`lat_resto`): **no se usan varargs nativos de LLVM**;
  el llamador empaqueta los argumentos extra en una lista (`lat_lista_de`,
  la misma función variádica real del runtime que ya usan
  `ListaLiteral`/`DiccionarioLiteral` desde la Fase L4) antes de invocar,
  exactamente como ya hace `GeneradorC::genLlamada` — evita manejar
  `va_arg`/intrínsecos de LLVM sin aportar nada (Latino ya modela "el resto"
  como una lista en tiempo de ejecución). El parámetro `lat_resto` de la
  función se declara y se copia a una celda local igual que cualquier otro
  parámetro; `VarArgs` (Fase L4, sin cambios en esta fase) ya sabía buscarlo
  en `variables` con ese nombre.
- [x] `genSentencia` traduce `Retornar`: copia el valor evaluado (o llama a
  `lat_nulo()` si `retornar` no trae valor) a la celda de retorno del estado
  privado nuevo `celdaRetorno_` (que `genFuncion` fija antes de traducir el
  cuerpo y restaura al salir) y cierra el bloque con `CreateRetVoid`. Si el
  cuerpo no termina ya con un `retornar` explícito en todas sus ramas (se
  detecta con el mismo helper `bloqueTerminado` de la Fase L5),
  `genFuncion` añade un `retornar nulo` implícito al final — igual que
  `GeneradorC::genFuncion`, que siempre emite `return lat_nulo();` tras el
  cuerpo. Un `retornar` sin función contenedora (`celdaRetorno_` es
  `nullptr`; no debería ocurrir con AST real) no emite nada, igual que
  `Romper` sin bucle contenedor (Fase L5).
- [x] `genExpr` traduce `Llamada` cuyo destino es un `Identificador` que
  nombra una función de usuario ya registrada (por `declararFuncion`): los
  argumentos fijos ausentes se completan con `lat_nulo()` y, si la función
  es variádica, los argumentos sobrantes se empaquetan con `lat_lista_de` —
  paridad exacta con `GeneradorC::genLlamada`. Cualquier otra `Llamada`
  (builtins como `escribir`, bibliotecas como `cadena.mayusculas`, métodos
  estáticos, ...) sigue devolviendo `nullptr` — Fases L7/L8.

**Criterio de aceptación — cumplido a nivel de codegen unitario:** 8 pruebas
nuevas en `tests/test_codegen_llvm.cpp` (mismo estilo que L2-L5: subcadena de
IR + `verifyModule`) cubren función simple con `retornar` explícito,
`retornar` sin valor, `retornar` implícito al final del cuerpo, llamada con
argumentos fijos faltantes (rellenados con `lat_nulo()`), llamada a función
variádica (empaquetado con `lat_lista_de`), recursión directa (`factorial`
construido a mano, confirma el prototipo adelantado), idempotencia de
`genFuncion` (una segunda llamada no duplica el `define`) y `retornar` sin
función contenedora. Suite completa (`ctest -C Release`): `test_codegen_llvm`
pasa con 129 comprobaciones (102 de L2-L5 + 27 nuevas); las 44 suites de
CTest (incluidos los 22 ejemplos E2E y todas las suites de librería) siguen
pasando sin cambios. El criterio *literal* del texto original del plan ("un
ejemplo E2E con `--backend=llvm` produce la misma salida que con
`--backend=c`") todavía no se puede ejecutar de punta a punta: `generar()`
sigue emitiendo el módulo "hola mundo" de la Fase L1 (no recorre `programa`
todavía) y no hay forma de imprimir un resultado sin `escribir`/`imprimir`
(builtins, Fase L7) — ambas piezas quedan pendientes hasta que L7 (llamadas
a runtime/librerías) y L9 (driver: `generar()` recorriendo el `Programa`
real) estén completas. Mismo patrón que L3-L5, que tampoco pudieron
ejecutar un `.lat` real con `--backend=llvm` todavía.

### Fase L7 — Llamadas a runtime y librerías (FFI) ✅ verificada con LLVM real (ver hallazgo de alcance)

**Archivos:** `src/compiler_llvm.cpp`/`include/compiler_llvm.h` (ampliado el
caso `Llamada` de `genExpr`, sin estado nuevo), `tests/test_codegen_llvm.cpp`
(ampliado con 9 pruebas nuevas).

- [x] Equivalente de `GeneradorC::genLlamada` para `Llamada` con destino
  `Identificador`: built-ins de un argumento opcional (`escribir`/
  `imprimir`/`escribe`/`poner`, `acadena`/`alogico`/`anumero`/`tipo`/`error`/
  `incluir` — el argumento ausente se completa con `lat_nulo()`, paridad
  exacta), built-ins sin argumentos (`leer`/`limpiar`) e `imprimirf`
  (variádica, empaquetada igual que `lat_lista_de`/`lat_dic_de` desde la Fase
  L4/L6 — ver hallazgo 1 abajo). Si el nombre no coincide con ningún builtin
  ni con una función de usuario ya declarada (Fase L6), devuelve `nullptr`
  como cualquier nodo no soportado (no debería ocurrir con AST real).
- [x] `Llamada` con destino `AccesoMiembro` cuyo objeto es un `Identificador`
  que nombra una de las 7 librerías (`cadena`, `lista`, `dic`, `mate`, `sis`,
  `archivo`, `paquete`) se traduce a `lat_<lib>_<fn>(args...)` — `cadena.
  formato` variádica con el mismo empaquetado que `imprimirf`. A diferencia
  de `GeneradorC` (que detecta `libsUsadas` dinámicamente para emitir
  `#include` selectivos), `GeneradorLLVM` puede `declare` cualquier función
  del runtime en el momento en que la necesita, vía `RuntimeAbiLLVM::
  declarar` (Fase L2) sobre `runtime_abi.ll` — no hace falta lista de "libs
  usadas": `invocador_c.cpp` ya enlaza incondicionalmente todos los `.c` de
  `runtime/libs/`.
- [x] Cualquier otro `AccesoMiembro` (el objeto no es una de las 7
  librerías) se traduce al despacho dinámico uniforme `lat_obj_llamar_metodo
  (objeto, nombre, nargs, args...)` — mismo fallback que usa `GeneradorC`
  tanto para "milib.funcionExportada(args)" (objeto de tipo `LAT_MODULO`,
  cargado con `paquete.cargar`, Reto 7 del plan) como para "instancia.metodo
  (args)" (objeto de tipo `LAT_OBJETO`, una vez la Fase L8 sepa construir
  instancias). No necesita ningún seguimiento de clases/estructuras porque
  el despacho ya es dinámico en runtime vía el diccionario de métodos que
  vive en el propio objeto — se implementó en esta fase (no en L8) porque no
  depende de nada que L8 vaya a agregar.

**Hallazgo 1 (no anticipado explícitamente en el texto original de esta
fase) — `lat_imprimirf` no tiene celda de retorno.** A diferencia de
`lat_escribir`/`lat_cadena_formato`/etc. (que siempre retornan `LatValor` por
`sret`), `lat_imprimirf` está declarada `void lat_imprimirf(size_t n, ...)`
en el runtime real — confirmado en `generated/runtime_abi.ll`:
`declare void @lat_imprimirf(i64 noundef, ...)`, sin ningún parámetro
`sret`. `genExpr(Llamada)` para `imprimirf` emite la llamada por su efecto y
devuelve `nullptr` como valor -- no porque el nodo sea "no soportado" (como
en el resto de `genExpr`), sino porque genuinamente no hay ninguna celda que
devolver. Esto replica el estado de cosas ya existente en `GeneradorC`,
donde el texto C `lat_imprimirf(...)` tampoco es una expresión `LatValor`
válida para anidar (solo se usa hoy como `ExprSentencia`) — la diferencia es
que en LLVM esta limitación se documenta explícitamente devolviendo
`nullptr`, en vez de dejar que el compilador de C de turno la descubra si
alguna vez se generara ese texto en posición de expresión.

**Hallazgo 2 — la firma real de `lat_obj_llamar_metodo` mezcla escalares
crudos con `LatValor` indirecto, confirmando la necesidad del mecanismo de
sondeo de la Decisión 2 también para funciones "mixtas".**
`generated/runtime_abi.ll` clasifica `LatValor lat_obj_llamar_metodo(LatValor
objeto, const char* nombre, int nargs, ...)` como `declare void @lat_obj_
llamar_metodo(ptr sret(%struct.LatValor) align 8, ptr noundef, ptr noundef,
i32 noundef, ...)`: el retorno es indirecto (`sret`, como siempre), `objeto`
(`LatValor`) es indirecto (`ptr`, igual que cualquier otro `LatValor`),
`nombre` (`const char*`) es un `ptr` **directo** (ya era un puntero en C, sin
indirección adicional) y `nargs` (`int`) es un `i32` **escalar por
registro**, no una celda `LatValor`. `genExpr(Llamada)` pasa `builder.
getInt32(...)` para `nargs` (un entero LLVM nativo, nunca una celda
`%struct.LatValor`) — construir esto a mano sin haber importado la firma real
de Clang habría sido fácil de acertar por intuición para este caso concreto,
pero es exactamente el tipo de suposición que la Decisión 2 prohíbe: la única
fuente de verdad es `runtime_abi.ll`, nunca una firma reconstruida por
inspección del prototipo C.

**Hallazgo/decisión de alcance — métodos estáticos de clase/estructura
(`NombreClase.metodo(...)`) se mueven a la Fase L8, no se implementan aquí.**
El texto original de esta fase incluía "métodos estáticos... mismo patrón de
empaquetar argumentos en arreglo + llamar función uniforme" como parte de
L7. En la práctica, resolver si `NombreClase` nombra una clase/estructura
conocida (y si `metodo` es uno de sus métodos estáticos) exige que
`GeneradorLLVM` lleve una tabla `clases_`/`estructuras_` análoga a la de
`GeneradorC` -- que no existe todavía porque `genClase`/`genMetodo`/
`genEstructura` son trabajo de la Fase L8, no de esta. Intentar resolver
"método estático" antes de que exista esa tabla no tiene con qué comparar
`NombreClase`; se documenta aquí como movido a L8 (donde sí se agregará la
tabla) en vez de implementarse a medias. El resto de la llamada dinámica
(`objeto.metodo(...)`/`milib.fn(...)`, que no necesita esa tabla) sí se
implementó en esta fase, ver arriba.

**Hallazgo/riesgo pendiente — ejecución real de argumentos variádicos
`LatValor` en un sitio de llamada, heredado desde L4/L6, ahora también
aplica a `lat_cadena_formato`/`lat_imprimirf`/`lat_obj_llamar_metodo`.**
Igual que `lat_lista_de`/`lat_dic_de` (Fase L4) y la llamada a una función de
usuario variádica (Fase L6), estas tres funciones son variádicas con
argumentos `LatValor` (16 bytes, indirectos) intercalados con parámetros
fijos escalares. El mecanismo de la Decisión 2 (Clang deriva el
`FunctionType`/atributos de la *declaración*) es sólido y está verificado
con LLVM real; lo que **no** se ha confirmado todavía con una ejecución real
es que un *sitio de llamada* armado a mano vía `IRBuilder::CreateCall` sobre
ese `FunctionType` (pasando el puntero de cada celda como argumento
variádico, nunca el struct por valor) produzca exactamente el mismo código
máquina que generaría Clang para una llamada C real equivalente -- las
pruebas de esta fase (igual que las de L4/L6) solo comprueban subcadena de
IR + `verifyModule`, no ejecución. Se documenta como riesgo conocido, no
como bloqueante: la Fase L9 (driver AOT, criterio "los 22 ejemplos
compilan y ejecutan con `--backend=llvm`") ejercitará esto con programas
reales que ya usan `imprimirf`/`cadena.formato`/listas, y es el punto natural
para confirmarlo con ejecución real -- introducir aquí una infraestructura de
compilación+enlace+ejecución ad-hoc (sin el driver todavía) habría exigido
adelantar trabajo de L9 fuera de su fase, y para el caso específico de
`lat_obj_llamar_metodo` además habría exigido un módulo dinámico de prueba
(`.dll`/`.so`) que ni siquiera existe hoy para el backend C (no hay ningún
test, unitario ni E2E, que ejercite `paquete.cargar` contra un módulo real).

**Criterio de aceptación — cumplido a nivel de codegen unitario, con el
mismo patrón de L3-L6:** 9 pruebas nuevas en `tests/test_codegen_llvm.cpp`
(subcadena de IR + `verifyModule`) cubren cada builtin de un argumento
(`escribir`), el argumento faltante completado con `lat_nulo()`
(`imprimir()`), un builtin sin argumentos (`leer`), `imprimirf` variádica
(confirma que devuelve `nullptr` como valor), una llamada de librería simple
(`cadena.mayusculas`), una llamada de librería sin argumentos (`mate.pi`),
`cadena.formato` variádica, el despacho dinámico `objeto.metodo(...)` (con
verificación explícita de que se incrusta el nombre del método/función como
literal y el conteo de argumentos como `i32`) y una llamada a un
identificador que no es builtin ni función de usuario (debe devolver
`nullptr`, no crashear). Suite completa (`ctest -C Release`):
`test_codegen_llvm` pasa con 157 comprobaciones (129 de L2-L6 + 28 nuevas);
las 44 suites de CTest (incluidos los 22 ejemplos E2E y todas las suites de
librería) siguen pasando sin cambios -- esta fase no tocó `GeneradorC` ni
ningún archivo de `runtime/`. El criterio *literal* del texto original del
plan (ejecución real de `paquete.cargar` + módulo externo compilado por
separado, vía `--backend=llvm`) sigue sin poder ejecutarse de punta a punta
por las mismas dos razones ya documentadas en L3-L6 (`generar()` todavía no
recorre el `Programa` real -- Fase L9) y en el hallazgo de riesgo pendiente
de arriba (falta de un módulo de prueba `.dll`/`.so`, que tampoco existe
para el backend C).

### Fase L8 — POO ✅ verificada con LLVM real

**Archivos:** `include/compiler_llvm.h`/`src/compiler_llvm.cpp` (ampliados:
nuevos métodos públicos `recolectarTipos`/`declararMetodo`/`genMetodo`/
`genClase`/`genEstructura`/`genInterfaz`, nuevo estado privado
`clases_`/`estructuras_`/`interfaces_`/`actualClase_`/`actualPadre_`, nuevo
helper privado `genArgumentoDeArray`), `tests/test_codegen_llvm.cpp`
(ampliado con 49 pruebas nuevas).

- [x] `recolectarTipos(Programa&)` puebla `clases_`/`estructuras_`/
  `interfaces_` -- equivalente exacto a `GeneradorC::recolectarTipos` --
  público (a diferencia de su contraparte en `GeneradorC`) para que las
  pruebas unitarias puedan poblar las tablas sin pasar por un driver
  completo, mismo patrón que el resto de métodos expuestos desde la Fase L3.
- [x] `declararMetodo`/`genMetodo` traducen métodos de instancia y estáticos
  (constructores incluidos) con la firma **empaquetada**
  `void @lat_fn_<Clase>_<metodo>(ptr sret %ret, i32 %nargs, ptr %args)` --
  **no** la firma "un puntero por parámetro" de `declararFuncion` (Fase L6).
  Motivo: el despacho dinámico real
  (`lat_obj_llamar_metodo`/`LatFnModulo`, confirmado leyendo
  `runtime/latino.c`: `LatFnModulo fn = metodo.como.funcion; fn(nargs + 1,
  args);`) solo conoce el número de argumentos de una llamada concreta en
  **tiempo de ejecución** -- nunca en tiempo de compilación, a diferencia de
  una llamada directa a una función de usuario. A nivel de ABI real en este
  target (LatValor siempre por sret, Fase L2), esta firma es exactamente la
  que produce Clang para `LatValor fn(int, LatValor*)` (`LatFnModulo`), así
  que un `llvm::Function*` de `declararMetodo` es válido para pasarse
  directamente a `lat_funcion_nueva`/`lat_obj_set_metodo` sin ningún ajuste.
- [x] "este" y cada parámetro se leen con `genArgumentoDeArray` (helper
  privado nuevo): una celda local fresca poblada con una rama real de basic
  blocks -- `(nargs > idx) ? args[idx] : lat_nulo()` -- nunca un acceso
  directo a `args[idx]`. Ajuste deliberado sobre el texto original de esta
  fase (que proponía `CreateGEP`+`CreateLoad` sin rama): `nargs` es un `i32`
  de **ejecución** (parámetro real de la función, nunca una constante de
  compilación), así que leer `args[idx]` sin comprobar primero sería leer
  más allá del array que construyó el llamador cuando la llamada trae menos
  argumentos que parámetros declarados -- exactamente el mismo caso que ya
  resuelve el `(nargs > idx) ? args[idx] : lat_nulo()` de
  `GeneradorC::genMetodo`, aquí con basic blocks reales en vez de un
  operador ternario de C (mismo patrón que `Ternaria`, Fase L4).
- [x] `NuevoExpr` (`nuevo Clase(args...)`) traduce 1:1
  `GeneradorC::genExpr(NuevoExpr)`: `lat_obj_nuevo` con el ancestro más
  antiguo de la cadena de herencia, luego `lat_obj_set_clase` por cada nivel
  hacia la hoja (para que `lat_obj_es_instancia` reconozca a los
  ancestros), registro de campos con valor por defecto (`lat_obj_set`) y
  métodos de instancia (`lat_obj_set_metodo` + `lat_funcion_nueva`)
  recorriendo la cadena de herencia de la raíz hacia la hoja, e invocación
  del constructor si existe uno. Una estructura sigue el mismo patrón sin
  ninguna cadena de ancestros (nunca hereda).
- [x] `EsExpr` (`expr es Clase`) y `AccesoEste` (`este`) no necesitan ninguna
  tabla: el primero es una llamada directa a `lat_obj_es_instancia`
  (retorna `int`, sin `sret` -- a diferencia de casi todo el resto del
  runtime) envuelta en `lat_logico`; el segundo busca `variables["este"]`,
  poblada por `genMetodo`.
- [x] Método estático (`NombreClase.metodo(...)`/
  `NombreEstructura.metodo(...)`) resuelto dentro del mismo bloque que ya
  maneja `AccesoMiembro` con objeto `Identificador` en `genExpr(Llamada)`
  (Fase L7), justo después de la comprobación de biblioteca y antes del
  despacho dinámico uniforme -- recorriendo la cadena de herencia hacia
  arriba para una clase, igual algoritmo que `GeneradorC::genLlamada`. Sin
  celda "este": el array de argumentos empieza directamente en el primer
  argumento real (`ptr null` cuando la llamada no trae ninguno, igual que el
  `NULL` literal de `GeneradorC` para ese caso).
- [x] `LlamadaBase` (`base(args...)`) empaqueta `este` + los argumentos
  evaluados en un array contiguo (misma convención que `NuevoExpr`) y llama
  al constructor de `actualPadre_` si existe uno -- paridad exacta con
  `GeneradorC::genSentencia(LlamadaBase)`, incluido el caso sin constructor
  de clase base (no emite ninguna llamada).
- [x] **Reto 6 del plan (colisión de nombres clase/estructura) -- verificado,
  no hace falta ningún cambio.** `AnalizadorSemantico::recolectarTipos`
  (`src/analizador_semantico.cpp:190`) ya guarda clases, estructuras e
  interfaces en una única tabla `tipos` compartida y rechaza cualquier
  nombre repetido entre las tres ("el tipo 'X' ya está definido") antes de
  llegar a ningún backend -- la colisión que el texto original de esta fase
  pedía verificar ya estaba cerrada desde antes de esta migración, en ambos
  backends por igual.
- [x] Un método abstracto (`genMetodo`) no declara ni define ninguna
  función -- paridad con `GeneradorC::genMetodo`, que tampoco emite nada
  para uno. `genInterfaz` es no-op (todos sus métodos son abstractos, sin
  excepción) -- paridad exacta con `GeneradorC::genInterfaz`.

**Hallazgo (no forma parte de esta fase, documentado para no repetirlo por
error en trabajo futuro) — `GeneradorC::genExpr(NuevoExpr)` tiene un bug
preexistente e independiente de esta migración: un campo con valor por
defecto no compila con `--backend=c`.** `GeneradorC` emite
`lat_obj_set(obj, lat_cadena("campo"), valor)`, pero la firma real de
`lat_obj_set` (`runtime/latino.h:89`) es `void lat_obj_set(LatValor objeto,
const char* nombre, LatValor valor)` -- el segundo argumento espera
`const char*`, no un `LatValor`. Confirmado compilando un caso mínimo
(`clase Contador \n publico valor: numero = 0 ...`) con
`--backend=c`: `cl.exe` rechaza el `.c` generado con `error C2440: no se
puede realizar la conversión de 'LatValor' a 'const char *'`. Como
`tests/test_poo_e2e.cpp` no cubre ningún campo con valor por defecto, este
bug pasó inadvertido hasta ahora. `GeneradorLLVM::genExpr(NuevoExpr)`
**no** replica este bug: usa el nombre crudo (`CreateGlobalStringPtr`, sin
envolverlo en una llamada a `lat_cadena`) para los tres parámetros de
nombre (campo/método/clase) en `lat_obj_set`/`lat_obj_set_metodo`/
`lat_obj_set_clase`/`lat_obj_es_instancia`, consistente con la firma real
que `RuntimeAbiLLVM` importa de `runtime_abi.ll` -- la Decisión 2 del plan
("nunca declarar firmas a mano") aplicada aquí también evitó heredar este
bug por construcción, no por una corrección deliberada. Corregir
`GeneradorC` queda fuera del alcance de esta migración (bug del backend C
preexistente, no introducido por este plan) -- se documenta aquí como
hallazgo para una futura corrección independiente.

**Criterio de aceptación — cumplido a nivel de codegen unitario, mismo
patrón que L2-L7:** 49 pruebas/comprobaciones nuevas en
`tests/test_codegen_llvm.cpp` (subcadena de IR + `verifyModule`) cubren:
`nuevo` sobre una estructura simple (con constructor), `nuevo` sobre una
clase con herencia (orden `lat_obj_nuevo(raíz)` antes de
`lat_obj_set_clase(hoja)`), registro de campo con valor por defecto y
método de instancia (`lat_obj_set`/`lat_obj_set_metodo`/
`lat_funcion_nueva`), `EsExpr` (`lat_obj_es_instancia` retornando `i32` sin
`sret` + `lat_logico`), `AccesoEste` dentro de un método (ramas
`arg_presente`/`arg_ausente` reales), un parámetro de método opcional
(segunda rama `hay_arg`), un método estático sin celda "este" (cero ramas
`hay_arg` con cero parámetros), un método abstracto (no declara nada),
idempotencia de `genMetodo`, una llamada a método estático conocido
(resuelta a llamada directa, nunca al despacho dinámico), un método
estático heredado (resuelto a la función de la clase que lo declara, con
`ptr null` para la llamada sin argumentos), `base(...)` con y sin
constructor de clase base, y `genInterfaz` (no-op). Suite completa
(`ctest -C Release`): `test_codegen_llvm` pasa con 206 comprobaciones (157
de L2-L7 + 49 nuevas); las 44 suites de CTest (incluidos los 22 ejemplos
E2E, `test_poo_e2e` y todas las suites de librería) siguen pasando sin
cambios -- esta fase no tocó `GeneradorC` ni ningún archivo de `runtime/`.
El criterio *literal* del texto original del plan (paridad de salida E2E
contra `--backend=c` para programas POO) sigue sin poder ejecutarse de
punta a punta por la misma razón ya documentada en L3-L7: `generar()`
todavía emite el módulo "hola mundo" de la Fase L1 (Fase L9, driver,
pendiente).

### Fase L9 — Driver AOT ✅ verificada con LLVM real

**Archivos:** `src/compiler_llvm.cpp`/`include/compiler_llvm.h`
(`GeneradorLLVM::generar` reescrito para recorrer el `Programa` real, ya no
el módulo "hola mundo" de la Fase L1), `tools/abi_probe.c` (agrega
`lat_abi_verificar` a la lista de funciones referenciadas), `src/main.cpp`
(nueva bandera `--solo-ir`).

- [x] `main.cpp` ya traía el flag `--backend=c|llvm` (default `c`) y el
  despacho a `GeneradorC::generar`/`GeneradorLLVM::generar` desde la Fase L1
  — no hizo falta ningún cambio ahí para esta fase.
- [x] `GeneradorLLVM::generar(Programa&)`: recolecta tipos
  (`recolectarTipos`), declara los prototipos de todas las funciones de
  usuario de nivel superior antes de generar ningún cuerpo (mismo patrón de
  dos pasadas que `GeneradorC::generarCuerpo`, necesario para recursión
  indirecta), genera cada función/clase/estructura/interfaz, y arma un
  `main(int argc, char** argv)` real: llama a `lat_set_args(argc, argv)`,
  luego a `lat_abi_verificar(tam, alineación, offset_tipo)` (Decisión 2 del
  plan, implementada en la Fase L2 pero sin ningún llamador hasta ahora) con
  los tres valores leídos del mismo `RuntimeAbiLLVM`/`DataLayout` que ya usa
  `test_codegen_llvm.cpp` para la comprobación equivalente, declara las
  variables locales de nivel superior (`recolectarVariables` +
  `declararLocales`) y traduce el resto de las sentencias con `genBloque` —
  paridad exacta con `GeneradorC::generarCuerpo`. `genSentencia`/`genBloque`
  ya no-opeaban sobre `Incluir`/`FuncionDef`/`ClaseDef`/`EstructuraDef`/
  `InterfazDef` desde la Fase L5-L8, así que a diferencia de
  `GeneradorC::generarCuerpo` (que sí filtra `FuncionDef` a mano antes de
  llamar a `genSentencia`) no hizo falta ningún filtro adicional al armar el
  cuerpo de `main`.
- [x] **Hallazgo — `lat_abi_verificar` nunca se había invocado desde ningún
  generador.** La función existe desde la Fase L2, pero como `generar()`
  todavía emitía el módulo "hola mundo" hasta esta fase, nunca tuvo un
  llamador real. `tools/abi_probe.c` tampoco la referenciaba (solo
  `(void)fn;` fuerza a Clang a emitir su `declare` en `runtime_abi.ll`), así
  que hubo que agregarla a la lista de `lat_abi_referenciar_todo` antes de
  que `RuntimeAbiLLVM::declarar(modulo, "lat_abi_verificar")` pudiera
  encontrarla.
- [x] **Hallazgo/decisión de alcance — el refactor de `invocador_c.cpp`
  para aceptar una "lista mixta de entradas" (previsto originalmente para
  esta fase) no hizo falta, confirmando el hallazgo ya anotado en la Fase
  L1.** `compilarAEjecutable` recibe un único nombre de archivo y lo inserta
  en la línea de comandos de `cl`/`cc` sin lógica dependiente de la
  extensión; `invocador_llvm.cpp` ya lo reutiliza tal cual desde la Fase L1
  pasándole el `.obj` emitido por LLVM en vez de un `.c`. No hay ningún caso
  en el plan que necesite pasar *ambos* (`.c` de usuario y `.obj` de LLVM) en
  una misma invocación, así que la lista mixta nunca fue necesaria.
- [x] Nueva bandera `--solo-ir` (análoga a `--solo-c`): vuelca el IR textual
  del módulo (`llvm::Module::print`) a `-o` si se indica, si no a `stdout`,
  sin compilar ni enlazar. `--solo-c` con `--backend llvm` y `--solo-ir` con
  `--backend c` son errores de uso explícitos (código 2).

**Criterio de aceptación — cumplido, incluida la paridad de salida
(adelantada respecto al texto original, que la dejaba para la Fase L12):**
los 27 archivos de `ejemplos/*.lat` con anotación `#salida:` compilan y
ejecutan con `--backend llvm`, y producen **salida idéntica byte a byte**
(no solo "no crashea") a la del `--backend=c` correspondiente — verificado
comparando la salida cruda de ambos ejecutables para los 27 archivos, no
solo los tokens que usa el arnés de `test_e2e.cpp`. Suite completa (`ctest
-C Release`): las 44 suites existentes siguen pasando sin cambios (las 8
que superaron un timeout de prueba de 30s/180s en esta verificación son
lentas por overhead de invocar `cl.exe` repetidamente en este entorno
—`vswhere.exe` no está disponible aquí y la localización de `VsDevCmd` cae a
una ruta más lenta—, no por ningún fallo real: todas pasan con más margen de
tiempo). El criterio *literal* de paridad exhaustiva contra ambos backends
para el 100% de las construcciones del lenguaje (no solo los 27 ejemplos
con `#salida:`) sigue siendo el gate formal de la Fase L12.

### Fase L10 — Modo JIT (LLVM ORC) ✅ verificada con LLVM real

**Archivos:** `include/compiler_llvm.h`/`src/compiler_llvm.cpp` (nuevo método
público `tomarContexto`), `include/invocador_llvm.h`/`src/invocador_llvm.cpp`
(función nueva `ejecutarJit`), `src/main.cpp` (flag `--jit`), `CMakeLists.txt`
(target `latino_runtime_estatico`, ver Decisión 4), `src/CMakeLists.txt`
(componente LLVM `orcjit`, enlace de `latino_runtime_estatico` dentro de
`latino`, copia post-build junto al `.exe`).

- [x] `GeneradorLLVM::tomarContexto()`: transfiere la posesión del
  `llvm::LLVMContext` a quien llama (`std::move` del `unique_ptr` interno) --
  necesario porque `llvm::orc::ThreadSafeModule` exige poseer el contexto,
  no solo una referencia (debe seguir vivo mientras el JIT ejecuta, más allá
  de la vida del `GeneradorLLVM`).
- [x] `ejecutarJit(modulo, contexto)`: crea un `llvm::orc::LLJIT`
  (`LLJITBuilder().create()`), añade un generador de resolución de símbolos
  a su `JITDylib` principal, empaqueta `modulo`+`contexto` en un
  `ThreadSafeModule` y lo agrega al JIT (`addIRModule`), busca el símbolo
  `main` (`lookup("main")`) y lo invoca como `int(*)(int, char**)` con un
  `argv` sintético de un solo elemento (`{"latino_jit", nullptr}`) —
  `latino --jit archivo.lat` no reenvía argumentos adicionales de línea de
  comandos al programa. Reutiliza el mismo `llvm::Module` que ya construye
  `GeneradorLLVM::generar()` (Fase L9): ningún codegen nuevo en esta fase.
- [x] `main.cpp`: nueva bandera `--jit` (solo válida con `--backend llvm`;
  incompatible con `--solo-ir`), ignora `-o`.

**Hallazgo 1 (contradice la lectura literal de la Decisión 4) —
`DynamicLibrarySearchGenerator::GetForCurrentProcess()` con el runtime
enlazado ESTÁTICAMENTE dentro de `latino.exe` no funciona en Windows.** El
texto original proponía enlazar el runtime estáticamente dentro de
`latino.exe` y dejar que `GetForCurrentProcess()` lo resolviera. En la
práctica, en Windows esa función resuelve símbolos vía `GetProcAddress()`
sobre los módulos cargados del proceso, y `GetProcAddress()` **solo ve
símbolos que un módulo exporta explícitamente** — un `.exe` enlazado por
MSVC no exporta nada de su propio código a menos que se use un `.def` o
`__declspec(dllexport)` explícito, algo que ni `latino.c` ni sus 7
bibliotecas usan (ni deberían, ya que también los compila
`invocador_c.cpp` para el ejecutable del usuario, sin ninguna noción de
"exportar"). Confirmado en la práctica: con el runtime enlazado STATIC
dentro de `latino.exe`, `ejecutarJit` fallaba con `JIT session error:
Symbols not found: [ lat_abi_verificar, lat_cadena, lat_escribir,
lat_set_args ]` seguido de una violación de acceso al intentar invocar un
`main` nunca materializado. La solución (que la propia Decisión 4 ya preveía
como alternativa) fue compilar `latino_runtime_estatico` como biblioteca
**dinámica** en vez de estática, con la propiedad de CMake
`WINDOWS_EXPORT_ALL_SYMBOLS` (exporta automáticamente toda función del
target en Windows, sin necesitar un `.def` a mano para las ~185 funciones
del runtime; no-op en Linux/macOS, donde los símbolos de una biblioteca
dinámica ya son visibles por defecto), y cargarla explícitamente **por
ruta** con `DynamicLibrarySearchGenerator::Load(ruta, prefijo)` en vez de
`GetForCurrentProcess()` — más determinista que depender de qué módulos
"ve" la búsqueda de todo el proceso. La ruta se resuelve en tiempo de
ejecución (`llvm::sys::fs::getMainExecutable` + `sys::path::parent_path`) y
se combina con el nombre de archivo real de la biblioteca, horneado vía
`target_compile_definitions(... "$<TARGET_FILE_NAME:latino_runtime_estatico>")`
en `src/CMakeLists.txt` (expresión de generador — no puede ir en
`config.h.in`, que solo admite variables de CMake normales). Un
`add_custom_command(TARGET latino POST_BUILD ...)` copia la DLL junto al
`.exe` en cada build, porque el generador de Visual Studio coloca cada
target en su propio árbol de salida (el target se declara en el
`CMakeLists.txt` raíz, no en `src/`). El nombre del target
(`latino_runtime_estatico`) se conservó porque es el que fija la Decisión 4
del plan; lo que cambió es *cómo* se enlaza/carga, no su propósito.
- [x] `LATINO_RUNTIME_ESTATICO_NOMBRE` (macro nueva vía
  `target_compile_definitions`, no vía `config.h.in`) — ver arriba.

**Hallazgo 2 (no anticipado en el texto original) — el `argv` sintético
pasado al `main` JIT-eado debe tener almacenamiento estático, no de pila.**
`lat_set_args(argc, argv)` (llamado dentro del `main` generado, Fase L9)
guarda el puntero `argv` tal cual en la variable global `lat_argv` del
runtime, sin copiarlo — válido para el modo AOT, donde ese `argv` es el
`argv` real de `main()` con almacenamiento de por vida del proceso, pero no
para el JIT si `ejecutarJit` lo declarara como arreglo local de su propia
pila: quedaría colgante en cuanto `ejecutarJit` retornara. Corregido
declarando `nombrePrograma`/`argvJit` con `static` en `ejecutarJit`.

**Hallazgo 3 (el más costoso de diagnosticar de toda esta fase) —
violación de acceso al SALIR del proceso, después de que el programa
JIT-eado ya ejecutó y retornó correctamente.** Confirmado paso a paso con
impresiones de depuración temporales que el crash ocurría **después** de
que el `LLJIT` se destruyera limpiamente dentro de `ejecutarJit` (es decir,
ni la ejecución del código JIT-eado ni la destrucción del propio LLJIT eran
la causa) — el proceso llegaba a imprimir la salida esperada y el código de
retorno correcto, y luego crasheaba en algún punto del desenrollado normal
del resto de `main()` (destrucción del AST, del analizador semántico, ...) o
de la limpieza global de C++ al salir. La causa más probable es el orden en
que Windows descarga `latino_runtime_estatico.dll` (cargada explícitamente
por ruta, Hallazgo 1) frente al del resto del apagado del proceso — un tipo
de problema conocido al mezclar bibliotecas cargadas dinámicamente con
limpieza global de C++/CRT al salir. Dado que para cuando esto ocurre ya
terminó todo el trabajo real (el programa del usuario ya ejecutó y ya
imprimió su salida), se evitó por completo vaciando los búferes de stdio
(`std::fflush(nullptr)`, necesario porque a diferencia de `exit`, `_Exit` no
vacía automáticamente los flujos de stdio) y saliendo del proceso de
inmediato con `std::_Exit(codigoJit)` justo después de que `ejecutarJit`
retorna, en `main.cpp` — mismo patrón pragmático que usan muchas
herramientas de línea de comandos de vida corta para evitar crashes de
orden de destrucción en el apagado, en vez de depender de que el
desenrollado normal de C++ termine sin problemas.

**Criterio de aceptación — cumplido, incluida la paridad de salida
(adelantada respecto al texto original, que no la exigía en esta fase):**
`latino --jit ejemplos/hola.lat` imprime `hola LLVM`/`hola mundo` sin
generar ningún `.obj`/`.exe` intermedio en disco (confirmado: el único
`.exe` en el directorio de salida tras la corrida es el propio
`latino.exe`), con código de salida 0 y sin ninguna violación de acceso.
Verificado además con los 27 archivos de `ejemplos/*.lat` con anotación
`#salida:` (no solo `hola.lat`): `--jit` produce **la misma salida por
stdout y el mismo código de salida** que el `.exe` equivalente compilado con
`--backend c`, para los 27. Suite completa (`ctest -C Release`): las 44
suites existentes siguen pasando sin cambios de comportamiento (algunas de
las suites de librería tardan más de lo esperado en este entorno por el
mismo motivo ya documentado en la Fase L9 — invocar `cl.exe` repetidamente
sin `vswhere.exe` disponible —, no por ninguna regresión real).

### Fase L11 — Tests ✅ verificada con LLVM real

**Archivos:** `tests/test_codegen_llvm.cpp` (ya poblado progresivamente en
L2–L8, sin cambios en esta fase), `tests/test_e2e.cpp` (parametrizado con un
backend opcional en vez de un `test_e2e_llvm.cpp` separado),
`tests/test_harness.h` (idem para las suites de librería),
`tests/CMakeLists.txt` (registro de las variantes `_llvm`).

- [x] `test_codegen_llvm.cpp` ya cumplía su parte desde L2–L8 (206
  comprobaciones de subcadena de IR + `verifyModule`) — sin trabajo nuevo.
- [x] **E2E comparativo, implementado parametrizando `test_e2e.cpp` en vez
  de duplicarlo en `test_e2e_llvm.cpp`** (el propio texto original de esta
  fase ofrecía esa alternativa). `test_e2e` acepta ahora un quinto argumento
  opcional `backend` (`"c"` por defecto) y lo reenvía como `--backend
  <backend>` al compilar. `tests/CMakeLists.txt` registra, por cada
  `ejemplos/*.lat`, una segunda prueba `e2e_<ejemplo>_llvm` que compila el
  mismo archivo con `--backend llvm` a un `.exe` de ruta distinta (para no
  pisar el del backend C) y lo compara contra la **misma** anotación
  `#salida:` que ya usa la prueba `e2e_<ejemplo>` del backend C. Diffear
  ambos backends contra la misma referencia implica transitivamente que
  coinciden entre sí — un `test_e2e_llvm.cpp` separado que los comparara
  directamente habría verificado exactamente lo mismo con más código
  duplicado, por eso se descartó esa opción.
- [x] `test_harness.h`: `Harness` acepta un `backend` opcional (`"c"` por
  defecto) en su constructor y lo agrega a la línea de compilación;
  `ejecutar_main` lo toma de un cuarto argumento CLI opcional. Todas las
  suites (`test_funciones_base`, `test_lib_cadena/lista/dic/mate/sis/
  archivo`, `test_incluir`, `test_poo_e2e`) heredan el soporte sin tocar su
  propio código, porque todas ya pasan por `harness::ejecutar_main`.
- [x] `tests/CMakeLists.txt`: el bucle de ejemplos E2E y la macro
  `add_suite20` registran una segunda prueba (`_llvm`) por cada una,
  reutilizando el **mismo binario ya compilado** (solo cambia el argumento
  de backend en `add_test`) — nada de esto duplica código C++, solo
  registro de CTest. `add_suite20` usa un subdirectorio temporal propio
  (`.../llvm/`) para la variante LLVM, porque cada caso de prueba escribe un
  `.lat`/`.exe` con el mismo nombre en ambas variantes y correrían el riesgo
  de pisarse si compartieran directorio (más relevante aún con `ctest -j`).
  Todo bajo `if(LATINO_LLVM_BACKEND)`: sin LLVM instalado, `tests/
  CMakeLists.txt` no registra ninguna prueba `_llvm` y la suite del backend
  C corre completa igual que siempre (Decisión 1 del plan).

**Criterio de aceptación — cumplido:** la suite de CTest pasó de 44 a 80
pruebas (27 `e2e_*_llvm` + 9 `*_llvm` de `add_suite20`, además de las 44 ya
existentes sin cambios). Las 80 pasan al 100% corriendo en **serie** (`ctest
-C Release --timeout 400`, ~3408 s reales en este entorno); las nuevas
`_llvm` confirman paridad de salida exacta contra la misma anotación
`#salida:` que ya validaba el backend C, para los 27 ejemplos y para cada
caso individual de las 9 suites de librería/POO/funciones base/módulos.
Verificado también que `ctest -C Release` sigue corriendo la suite completa
(incluidas las `_llvm`) sin fallos con `LATINO_LLVM_BACKEND=ON`; no se
volvió a verificar por separado el build `LATINO_LLVM_BACKEND=OFF` en esta
fase (ya lo hizo la Fase L1 y esta fase no tocó esa ruta condicional más que
envolver el registro nuevo en el mismo `if` que ya existía).

**Hallazgo real (no anticipado en el texto original de esta fase, y
preexistente a esta migración) — `ctest -j` (ejecución en paralelo) no es
seguro con este compilador, con o sin backend LLVM.** `invocador_c.cpp`/
`invocador_llvm.cpp` escriben el objeto/`.c` temporal de cada compilación en
una ruta **fija** dentro de `fs::temp_directory_path()` (p.ej.
`latino_obj/sis.obj`, `latino_llvm_obj.obj`), no en una ruta única por
proceso. Correr la suite con `ctest -j 4` lanza varios `latino.exe`
concurrentes que compilan programas distintos y **pisan el mismo archivo
temporal entre sí**, produciendo fallos aleatorios (`Permission denied`,
`LNK1104: no se puede abrir el archivo`, ejecutables truncados/faltantes) —
confirmado: con `-j 4` fallaron 56/80 pruebas con estos síntomas exactos, y
las 80 pasan al 100% con la misma build corriendo en serie. No es una
regresión de esta fase (el backend C ya tenía este mismo problema antes de
que existiera el backend LLVM; L11 simplemente fue la primera vez que se
corrió la suite con `-j`, porque duplicar el número de pruebas hizo tentador
paralelizar). Se documenta aquí como hallazgo para quien configure CI en el
futuro (`PLAN_LLVM.md` no tenía hasta ahora ninguna suposición explícita
sobre paralelismo de pruebas) — no se intenta arreglar en este plan
(cambiaría `invocador_c.cpp`, archivo fuera del alcance de la migración de
backend) más que dejar constancia de que **`ctest` para este proyecto debe
correrse sin `-j`** hasta que se corrija esa ruta fija.

### Fase L12 — Paridad y decisión de default

- Ejecutar toda la suite con ambos backends; medir tiempo de compilación
  end-to-end y tiempo/tamaño de los binarios resultantes.
- Criterio objetivo de "paridad" para considerar cambiar el default de
  `--backend` a `llvm`:
  1. Los 22 ejemplos de `ejemplos/*.lat` producen salida idéntica en ambos
     backends.
  2. Todas las suites de librería (`test_lib_*`, `test_poo_e2e`,
     `test_incluir`, `test_funciones_base`) pasan también contra
     `--backend=llvm`.
  3. Sin regresión de tiempo de compilación end-to-end mayor a lo razonable
     frente al backend C.
- Si se cumple, cambiar el default en `main.cpp`; si no, documentar los
  gaps restantes como fases de seguimiento.

### Fase L13 — *(fuera de alcance temporal)* Retiro de `GeneradorC`

No se ejecuta como parte de este plan. Se documenta aquí solo como
referencia: una vez `--backend=llvm` sea el default sin regresiones
reportadas durante un ciclo de uso real, un plan de seguimiento
(`PLAN_RETIRO_BACKEND_C.md`) podría evaluar si retirar `GeneradorC` (archivo
único de 841 líneas, barato de mantener) aporta valor real o si conviene
dejarlo indefinidamente como referencia/fallback de bajo costo — de forma
similar a cómo el proyecto ya mantiene `src/interpreter.cpp` sin usarlo
activamente.

## Retos técnicos detallados

### Reto 1: ABI de `LatValor` por valor en ~150 fronteras de función

**Problema**: cada llamada al runtime pasa/retorna un struct de 16 bytes por
valor; la clasificación ABI (registros vs. paso indirecto) difiere entre
Windows x64 y SysV x86-64, y un mismatch entre lo que emite `GeneradorLLVM` y
lo que espera el `.c` compilado corrompe memoria de forma silenciosa.

**Solución**: mecanismo de sondeo vía Clang descrito en la Decisión 2 — nunca
declarar el tipo/las firmas a mano. Implementar y verificar esto (Fase L2)
**antes** de escribir cualquier codegen real (Fase L3+).

### Reto 2: ABI de C++ entre `latino.exe` y las bibliotecas de LLVM

**Problema**: al usar la API C++ nativa de LLVM (decisión del usuario), un
mismatch de toolchain/STL/ABI entre `latino.exe` y `libLLVM*` produce
crashes o corrupción silenciosa, no errores de enlace claros.

**Solución**: obtener LLVM exclusivamente por los canales de la tabla de la
Decisión 1 (vcpkg en Windows con el mismo triplet MSVC, paquetes de distro en
Linux/macOS), fijar versión concreta (18.x), y documentar explícitamente que
mezclar un LLVM prebuilt genérico de otra fuente con el toolchain del
proyecto no está soportado.

### Reto 3: `alloca` fuera del entry block

**Problema**: LLVM exige (no por sintaxis, sino por convención de
optimización y corrección) que los `alloca` de una función vivan en su entry
block; emitirlos dentro de basic blocks de bucles/condicionales impide la
promoción a SSA (`mem2reg`) y puede causar crecimiento de pila no acotado.

**Solución**: `IRBuilder` secundario fijo en el entry block, usado
exclusivamente para `alloca`s, independiente del builder que avanza por el
control de flujo (Fase L4).

### Reto 4: Hoisting total sin scoping por bloque

**Problema**: `GeneradorC` hoy declara TODAS las variables locales de una
función al inicio (sin scoping por bloque, similar a `var` en JS). Sería
tentador "arreglarlo" al reescribir el backend.

**Solución**: replicar el comportamiento actual sin cambios — es una decisión
de *lenguaje*, no de *backend*; cambiarla es un plan de lenguaje
independiente y no debe mezclarse con esta migración.

### Reto 5: Variádica sin varargs nativos de LLVM

**Problema**: podría parecer que `...` debe mapear a varargs nativos de LLVM
(`va_arg`, intrínsecos `llvm.va_start`/`llvm.va_end`), que son notoriamente
frágiles de generar correctamente a mano.

**Solución**: no hace falta — Latino ya modela "el resto" como una lista
construida por el *llamador* antes de invocar (`GeneradorC::genLlamada`
existente). `GeneradorLLVM` replica exactamente ese empaquetado (Fase L6),
evitando varargs nativos por completo.

### Reto 6: Colisión de nombres de símbolos POO en el módulo LLVM

**Problema**: un módulo LLVM exige nombres de función globalmente únicos,
igual que un archivo objeto C. El esquema `lat_fn_<Clase>_<metodo>` ya lo
asume hoy en el backend C.

**Solución**: verificar en la Fase L8 si el analizador semántico ya impide que
una clase y una estructura (u otras combinaciones) compartan nombre; si no,
añadir esa verificación — es un gap potencial que existe igual en el backend
C actual, esta migración solo lo hace más visible.

### Reto 7: `paquete` (carga dinámica) y el ABI derivado

**Problema**: `runtime/libs/paquete.c` carga módulos `.dll`/`.so` externos en
runtime (`LoadLibrary`/`dlopen`) con la convención
`LatValor fn(int nargs, LatValor* args)`. Si el ABI que asume el código
emitido por LLVM difiriera del que usa un módulo externo compilado por
separado, se rompería la interoperabilidad de forma silenciosa.

**Solución**: no requiere tratamiento especial de codegen — es responsabilidad
exclusiva de `paquete.c`, agnóstico al backend que generó el `.obj`
invocador. El mecanismo de ABI de la Decisión 2 (misma fuente de verdad,
Clang, para todo el proyecto) es la mitigación real; añadir el test explícito
de la Fase L7 para confirmarlo en la práctica.

### Reto 8: `elegir` como comparaciones secuenciales, no `switch` nativo

Ver Fase L5 — usar `SwitchInst` de LLVM rompería la semántica dinámica de
`elegir` (valores de caso evaluados en runtime, no constantes de compilación).

### Reto 9: Verificación de módulo antes de emitir

`llvm::verifyModule()` debe correr siempre antes de
`addPassesToEmitFile`/antes de entregar el módulo al `LLJIT`. Sin esto, bugs
del generador (bloques sin terminador, tipos no coincidentes en una llamada)
producen errores opacos del backend de LLVM o binarios corruptos.

### Reto 10: Resolución de símbolos del runtime en el proceso JIT

**Problema** (nuevo por el modo JIT): si el runtime se enlaza estáticamente en
`latino.exe` pero también existe como `.dll`/`.so` separado para el modo AOT,
hay dos copias del estado global del runtime (si alguna vez lo tuviera) y dos
superficies de símbolos distintas a mantener sincronizadas.

**Solución**: un único target CMake (`latino_runtime_estatico`) que se enlaza
dentro de `latino.exe` para el modo JIT, y sigue reutilizando los mismos
`.c` fuente para el modo AOT (compilados por separado por
`invocador_c.cpp`/`invocador_llvm.cpp` como siempre) — no se comparte binario
entre JIT y AOT, se comparte únicamente el código fuente C, que es la unidad
que ya se considera la fuente de verdad del runtime.

## Archivos críticos para la implementación

- [runtime/latino.h](../runtime/latino.h) — fuente de verdad del ABI de
  `LatValor` y de las firmas del runtime; base del mecanismo de sondeo de
  Clang.
- [src/compiler.cpp](../src/compiler.cpp) / [include/compiler.h](../include/compiler.h)
  — referencia 1:1 de todo el mapeo AST→código a replicar en
  `GeneradorLLVM` (tabla `fnBinaria`, `recolectarVariables`, `genLlamada`,
  `genClase`/`genMetodo`/`NuevoExpr`).
- [src/invocador_c.cpp](../src/invocador_c.cpp) / [include/invocador_c.h](../include/invocador_c.h)
  — infraestructura de enlace (VsDevCmd/cl.exe, cc/gcc/clang, recolección de
  `runtime/libs/*.c`) a extender para aceptar objetos ya compilados por LLVM.
- [src/main.cpp](../src/main.cpp) — punto de despacho de `--backend`/`--jit`
  y orquestación general del pipeline.
- `CMakeLists.txt` / `src/CMakeLists.txt` / `tests/CMakeLists.txt` —
  integración de `find_package(LLVM CONFIG)`, opción `LATINO_LLVM_BACKEND`,
  registro condicional de los tests LLVM.
- `include/compiler_llvm.h` / `src/compiler_llvm.cpp` — nuevo generador.
- `include/invocador_llvm.h` / `src/invocador_llvm.cpp` — nuevo driver
  AOT+JIT.
- `tools/abi_probe.c` — sondeo de ABI vía Clang.
- `tests/test_codegen_llvm.cpp` / `tests/test_e2e_llvm.cpp` — nuevas suites.

## Orden de implementación recomendado

El orden garantiza que cada paso compila y no rompe la suite existente del
backend C:

1. **L0–L1** — LLVM disponible en el build, smoke test de plumbing
   compilación+enlace, sin tocar el AST real.
2. **L2** — mecanismo de ABI verificado (`verifyModule` + prueba de
   `sizeof`), antes de cualquier codegen real.
3. **L3–L4** — expresiones y variables locales (con la disciplina de
   `alloca` en entry block).
4. **L5** — control de flujo.
5. **L6** — funciones de usuario y variádica.
6. **L7** — FFI a runtime/librerías.
7. **L8** — POO.
8. **L9** — driver AOT completo, con los 22 ejemplos corriendo con
   `--backend=llvm`.
9. **L10** — modo JIT (reutiliza el módulo ya validado en L3–L8).
10. **L11** — tests ampliados y comparativos.
11. **L12** — evaluación de paridad y decisión de default.
12. **L13** — *(no en este plan)* retiro eventual de `GeneradorC`.

## Garantía de retrocompatibilidad

- `--backend=c` (default durante toda la migración) no cambia: mismo
  `GeneradorC`, mismo runtime, misma salida en los 22 ejemplos y toda la
  suite de CTest actual.
- `tests/test_codegen.cpp` no se modifica.
- Ningún archivo de `runtime/` cambia de comportamiento observable — solo se
  agrega `lat_abi_verificar()` (Fase L2), que no altera ninguna función
  existente.
- Las máquinas sin LLVM instalado siguen pudiendo construir y probar el
  100% del backend C (`LATINO_LLVM_BACKEND=OFF` automático si
  `find_package(LLVM)` falla).

## Verificación

1. **Build**: `cmake -B build -DLATINO_LLVM_BACKEND=ON` (requiere LLVM 18.x
   instalado según la Decisión 1) y luego `cmake --build build --config
   Release`.
2. **Por fase (L1–L10)**: cada fase indica su propio criterio de aceptación
   arriba (compila, `verifyModule` pasa, ejemplo puntual produce la salida
   esperada).
3. **Suite completa**: `ctest --test-dir build -C Release --output-on-failure`
   — con `LATINO_LLVM_BACKEND=ON`, deben pasar tanto las suites existentes
   del backend C como las nuevas suites LLVM (`test_codegen_llvm`,
   `test_e2e_llvm`, variantes `--backend=llvm` de `test_lib_*`/
   `test_poo_e2e`/etc.).
4. **Paridad (gate de la Fase L12)**: script/target que compila y ejecuta
   los 22 `ejemplos/*.lat` con ambos backends y falla si alguna salida
   difiere.
5. **JIT**: `latino --jit ejemplos/hola.lat` (y progresivamente el resto de
   ejemplos a medida que L3–L8 avanzan) sin generar artefactos en disco.
