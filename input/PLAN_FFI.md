# Plan de trabajo — Foreign Function Interface (FFI) con C al estilo de Rust

## Contexto

Latino ya tiene un mecanismo para consumir código nativo en runtime:
`incluir "paquete"` (Fase 16 de [PLAN_LIBS.md](PLAN_LIBS.md),
`runtime/libs/paquete.c`/`.h`). Funciona así:

```latino
incluir "paquete"
milib = paquete.cargar("mi_extension.dll")
r = paquete.llamar(milib, "mi_funcion", 2, a, b)
```

`paquete.cargar` hace `LoadLibrary`/`dlopen` **en tiempo de ejecución** y
`paquete.llamar` busca el símbolo por nombre y lo invoca — pero exige que la
función nativa tenga, textualmente, la firma empaquetada que define
`LatFnModulo` (`runtime/latino.h`):

```c
typedef LatValor (*LatFnModulo)(int nargs, LatValor* args);
```

Es decir: **no se puede llamar a una función C arbitraria** (`abs`,
`strlen`, una función de `user32.dll`, una biblioteca científica en C/C++
con ABI-C) — primero hay que escribir, a mano, un archivo `.c` que envuelva
cada función real detrás de esa firma fija y la compile como biblioteca
dinámica. Es el mismo costo que tendría exponer una API a Python sin usar
`ctypes`/`cffi`: funciona, pero exige una capa de C intermedia por cada
función que se quiera exponer.

Este plan agrega un segundo mecanismo, **`externo`**, que permite declarar
la firma **real** de una función C —tipos con ancho fijo, punteros
opacos— directamente en un programa Latino y llamarla sin escribir ningún
código C intermedio. Es el mismo modelo mental que
[`extern "C"` de Rust](https://doc.rust-lang.org/nomicon/ffi.html):

```rust
#[link(name = "user32")]
extern "C" {
    fn MessageBoxA(hwnd: *mut c_void, text: *const c_char,
                    caption: *const c_char, boxtype: u32) -> i32;
}

unsafe {
    MessageBoxA(std::ptr::null_mut(), c"Hola".as_ptr(), c"Latino".as_ptr(), 0);
}
```

`incluir "paquete"` y `externo` no se reemplazan entre sí: resuelven
problemas distintos y conviven (ver Decisión de diseño 8) — el mismo
principio de convivencia que ya usó `PLAN_MODULOS.md` entre `incluir` y
`exportar`/`importar`.

## Motivación

- Permite usar bibliotecas C/C++ (ABI-C) ya existentes — `libc`/`msvcrt`,
  el API de Win32, SQLite, zlib, etc. — sin escribir una sola línea de C de
  por medio, algo que hoy es literalmente imposible con `paquete` salvo
  reescribiendo la biblioteca.
- Es la vía natural para que la propia librería estándar de Latino
  (`runtime/libs/*`) deje de depender de que **todo** viva reescrito en C
  dentro del repo: un futuro `mate.raiz_rapida` podría enlazar contra una
  biblioteca matemática real en vez de reimplementarla.
- Buen valor educativo/demostrativo para el lenguaje: "FFI al estilo de
  Rust" es un modelo mental conocido y ya se usó como referencia de diseño
  en otras partes del proyecto (POO "al estilo C#", módulos "al estilo
  TypeScript/ES Modules", genéricos "al estilo de Rust" —
  [PLAN_GENERICOS.md](PLAN_GENERICOS.md)).
- El compilador ya resuelve, para ambos backends, el problema más difícil
  de interoperar con C real: el ABI exacto de un valor agregado (`LatValor`
  pasado por `sret`/puntero en MSVC x64, ver Fase L2 de
  [PLAN_LLVM.md](PLAN_LLVM.md)). Ese conocimiento se reutiliza aquí para
  *desempaquetar* un `LatValor` a un tipo C primitivo, no para inventar un
  mecanismo nuevo desde cero.

### Decisiones de diseño

1. **Enlace en tiempo de compilación, no carga dinámica en runtime.** Un
   bloque `externo` declara una firma que el backend elegido (C o LLVM)
   resuelve como cualquier símbolo externo de C: el linker del sistema
   (`cl.exe`/`link.exe`, o `ld`/`lld` vía Clang) lo busca al enlazar el
   ejecutable final. Esto es lo opuesto a `paquete.cargar`, que resuelve el
   símbolo en runtime con `GetProcAddress`/`dlsym` — la diferencia central
   entre ambos mecanismos (ver Contexto).
2. **Vocabulario de tipos FFI propio, no `TipoAnotado`.** Los tipos que ya
   usa el tipado gradual (`numero`, `cadena`, `logico`, `nulo` — Fase 27,
   [PLAN_TIPADO.md](PLAN_TIPADO.md)) siguen significando "un `LatValor`
   dinámico verificado en ese punto" y **se reutilizan tal cual** en una
   firma `externo` cuando el mapeo a C es exacto y sin pérdida:
   - `numero` ↔ `double` (el propio storage de `LAT_NUMERO` ya es
     `double`, cero conversión).
   - `logico` ↔ `int` de 0/1 (mismo storage que `LAT_LOGICO`).
   - `cadena` ↔ `const char*` (el storage de `LAT_CADENA` ya es un buffer
     `char*` terminado en nulo — ver Riesgos técnicos).
   - `nulo` ↔ `void`, solo válido como tipo de retorno.

   Se agregan tipos **nuevos**, exclusivos de `externo`, solo donde no hay
   equivalente sin pérdida: anchos enteros fijos (`entero8/16/32/64`,
   `natural8/16/32/64` sin signo) y un puntero opaco (`puntero`). Nunca se
   exponen los tipos C ambiguos por plataforma (`int`, `long`, `size_t`) —
   ver Riesgos técnicos.
3. **Marshalling automático en el borde, no manual.** El compilador genera,
   en cada llamada a una función `externo`, la conversión `LatValor →
   tipo C` de cada argumento y `tipo C → LatValor` del retorno, según la
   firma declarada. El programador nunca escribe la conversión a mano (a
   diferencia de `paquete.llamar`, donde hoy el usuario arma el arreglo de
   `LatValor` y la función C envoltorio decide cómo interpretarlo).
4. **Puntero opaco nuevo en el runtime.** `puntero` se representa con un
   `LatTipo` nuevo, `LAT_PUNTERO` (`void*` crudo en la unión de
   `LatValor`), análogo a `*mut c_void`/`*const c_void` en Rust: no se
   puede indexar ni leer desde Latino, solo pasar de una llamada `externo`
   a otra o comparar contra `nulo`. Sin esto, una función C que devuelve un
   handle opaco (un `FILE*`, un HWND) no tendría dónde vivir en el sistema
   de valores de Latino.
5. **`inseguro` marca el borde, igual que `unsafe` en Rust.** Toda llamada
   a una función `externo` debe ocurrir dentro de un bloque `inseguro ...
   fin`, o dentro de una función declarada `funcion inseguro nombre(...)`.
   Es un chequeo puramente estático (análisis semántico) sin efecto en
   runtime — el objetivo es el mismo que en Rust: que el uso de FFI sea
   *visualmente* imposible de pasar por alto en una revisión de código,
   nunca "seguridad" real (Latino no puede verificar en compilación que un
   puntero nativo siga siendo válido).
6. **`enlazar "nombre"` es opcional.** Sin `enlazar`, se asume que el
   símbolo ya es resoluble contra el runtime C que todo ejecutable Latino
   enlaza siempre (`msvcrt`/`ucrt` en Windows, `libc` en Linux/macOS) — el
   caso común de llamar `abs`, `strlen`, `toupper`, etc. sin configuración
   adicional. `externo enlazar "user32"` asocia el bloque con una
   biblioteca de importación adicional, que cada backend agrega a su línea
   de compilación/enlace (ver "Codegen").
7. **Convención de llamada: la nativa del ABI de destino, sin variantes
   explícitas en v1.** Alcanza para x64 Windows/SysV (única plataforma real
   hoy — ver Riesgos técnicos de `PLAN_LLVM.md`); no se expone
   `stdcall`/`__cdecl` como en Rust (`extern "system"` vs `extern "C"`).
8. **Sin relación con `incluir "paquete"`.** Mecanismos complementarios que
   conviven indefinidamente, igual que `incluir` y `exportar`/`importar`
   conviven tras `PLAN_MODULOS.md`: `paquete` sigue siendo la vía para
   cargar un plugin en runtime con una firma ya adaptada a Latino;
   `externo` es la vía para consumir una firma C real, resuelta en
   compilación.

## Sintaxis propuesta

### Declaración de funciones externas

```latino
# Sin "enlazar": el símbolo ya está en el runtime C que todo ejecutable
# Latino enlaza siempre (msvcrt/ucrt en Windows, libc en Linux/macOS).
externo
    funcion abs(n: entero32): entero32
    funcion toupper(c: entero32): entero32
    funcion strlen(s: cadena): entero64
fin

# Con "enlazar": biblioteca adicional que el backend debe agregar a su
# línea de enlace.
externo enlazar "user32"
    funcion MessageBoxA(hwnd: puntero, texto: cadena, titulo: cadena,
                         tipo: entero32): entero32
fin
```

### Llamar una función externa — siempre dentro de `inseguro`

```latino
inseguro
    r = abs(-5)
    escribir(r)                                   # 5
    escribir(strlen("hola"))                       # 4
    MessageBoxA(nulo, "Hola desde Latino", "FFI", 0)
fin
```

Una función completa puede marcarse `inseguro` en vez de envolver cada
llamada en un bloque:

```latino
funcion inseguro saludar_nativo()
    MessageBoxA(nulo, "Hola desde Latino", "FFI", 0)
fin
```

Llamar una función `externo` fuera de un bloque/función `inseguro` es un
error de compilación (ver "Mensajes de error").

### Punteros opacos

```latino
externo
    funcion malloc(tam: entero64): puntero
    funcion free(p: puntero)
fin

inseguro
    p = malloc(16)
    si p es nulo
        escribir("sin memoria")
    fin
    free(p)
fin
```

### Tabla de tipos FFI

| Tipo en `externo` | Tipo C | Storage de `LatValor` | Nota |
|---|---|---|---|
| `numero` | `double` | `LAT_NUMERO` (ya es `double`) | reutilizado, sin tipo nuevo |
| `logico` | `int` (0/1) | `LAT_LOGICO` (ya es `int`) | reutilizado, sin tipo nuevo |
| `cadena` | `const char*` | `LAT_CADENA` (ya es `char*` con `\0`) | reutilizado, sin tipo nuevo |
| `nulo` | `void` | — | solo como tipo de retorno |
| `entero8`/`16`/`32`/`64` | `int8_t`/`int16_t`/`int32_t`/`int64_t` | nuevo, empaqueta/desempaqueta en `LAT_NUMERO` | ver pérdida de precisión en Riesgos técnicos para 64 bits |
| `natural8`/`16`/`32`/`64` | `uint8_t`/`uint16_t`/`uint32_t`/`uint64_t` | ídem, sin signo | ídem |
| `puntero` | `void*` | nuevo `LAT_PUNTERO` | opaco, solo `nulo`-comprobable |

### Tabla de palabras reservadas nuevas

| Palabra | Uso |
|---|---|
| `externo` | abre un bloque de declaraciones de funciones nativas |
| `enlazar` | (dentro de `externo`) nombra la biblioteca de importación a enlazar |
| `inseguro` | marca un bloque de sentencias, o una función completa, habilitado para llamar funciones `externo` |

(`entero8`/`16`/`32`/`64`, `natural8`/`16`/`32`/`64` y `puntero` se resuelven
por lexema en contexto de tipo, igual que `numero`/`cadena`/`logico` hoy —
no son palabras reservadas nuevas, ver nota de `PLAN_TIPADO.md` sobre no
romper variables que ya se llamen así.)

## Semántica

### Parser

`externo [enlazar "lib"] ( funcion nombre(param: TipoFFI, ...) [: TipoFFI] )+ fin`
— cada `funcion` dentro del bloque es solo una firma (sin cuerpo, sin
`retornar`). `inseguro ... fin` se parsea como un bloque de sentencias más
(mismo shape que el cuerpo de `si`/`mientras`); `funcion inseguro
nombre(...)` agrega el modificador `inseguro` a la declaración de función
ya existente.

### Análisis semántico

- Tabla nueva `funcionesExternas` (nombre → firma: parámetros tipados +
  retorno), separada de la tabla de funciones normales de
  `AnalizadorSemantico` — un nombre no puede estar en ambas tablas a la vez
  (error si colisiona con una función Latino normal).
- Pila `inseguroActivo` (booleano anidado, mismo patrón que
  `genericosActivos` de `PLAN_GENERICOS.md`): se empuja al entrar a un
  bloque `inseguro` o a una función marcada `inseguro`; toda `Llamada` cuyo
  destino resuelva a `funcionesExternas` se valida contra el tope de esta
  pila.
- Aridad: número de argumentos debe coincidir exactamente con la firma
  (no hay variádicas en `externo`, a diferencia de las funciones Latino
  normales).
- Chequeo de tipo: cuando el argumento en el sitio de llamada es un
  literal o una variable con anotación de tipo ya conocida (misma
  infraestructura que `PLAN_TIPADO.md`), se verifica en compilación contra
  el `TipoFFI` esperado; si no se puede determinar estáticamente, la
  verificación queda diferida a runtime (ver Codegen).

### Codegen — backend C (`GeneradorC`, `src/compiler.cpp`)

1. Por cada `ExternoBloque`, al principio del `.c` generado:
   - `#pragma comment(lib, "<lib>.lib")` bajo `#ifdef _MSC_VER` (no-op en
     otros compiladores — ver punto 3).
   - `extern <tipo_C_retorno> <simbolo>(<tipos_C_parametros>);` — una
     declaración real, no una firma `LatValor`.
2. Cada `Llamada` a una función externa se traduce a: extraer el campo
   `.como.X` de cada `LatValor` argumento (con un chequeo dinámico de tipo
   si no se resolvió en compilación — nueva función auxiliar de runtime,
   ver más abajo), invocar el símbolo real, y envolver el resultado con el
   constructor de `LatValor` correspondiente (`lat_numero`, `lat_logico`,
   `lat_cadena`, o el nuevo `lat_puntero`).
3. `invocador_c.cpp` necesita poder agregar `-l<lib>` a la línea de
   `gcc`/`clang` en plataformas no-MSVC (el `#pragma comment(lib,...)` del
   punto 1 solo lo entiende MSVC) — requiere que `GeneradorC` exponga la
   lista de bibliotecas declaradas con `enlazar` para que `main.cpp` se la
   pase a `invocador_c.cpp`.

### Codegen — backend LLVM (`GeneradorLLVM`, `src/compiler_llvm.cpp`)

1. Por cada `ExternoFn`, `M.getOrInsertFunction(simbolo, tipo_LLVM_real)`
   con la firma nativa (`i8`/`i16`/`i32`/`i64`/`double`/`ptr`) — a
   diferencia de las funciones de usuario (L6) y los builtins (L7), **no**
   se usa la firma empaquetada `(sret, nargs, args)`, ni hace falta pasar
   por `RuntimeAbiLLVM`: son tipos LLVM primitivos que `IRBuilder` maneja
   de forma nativa.
2. Marshalling: `extractvalue`/GEP + `load` sobre la `%struct.LatValor` ya
   conocida (mismo layout que expone `RuntimeAbiLLVM` desde L2 de
   `PLAN_LLVM.md`), conversión con `trunc`/`fptrunc`/`bitcast` según el
   `TipoFFI`, `call` directo al símbolo externo, y empaquetado del
   resultado con las funciones constructoras del runtime ya declaradas.
3. `invocador_llvm.cpp` necesita agregar la biblioteca de `enlazar` a la
   invocación del linker (AOT) — mismo requisito que el punto 3 del backend
   C, forma de código distinta.

### Runtime (`runtime/latino.h`/`.c`)

- `LAT_PUNTERO` nuevo en `LatTipo`; campo `void* puntero;` nuevo en la
  unión de `LatValor`.
- `LatValor lat_puntero(void* p);` — constructor.
- `int lat_puntero_es_nulo(LatValor v);` — o reutilizar el `es nulo` ya
  existente si `LAT_PUNTERO` con `puntero == NULL` se decide tratar como
  equivalente a `LAT_NULO` en el operador `es` (definir en F3, ver Fases).
- `LatValor lat_ffi_verificar_tipo(LatValor v, /* enum TipoFFI en runtime */ int esperado, const char* nombreFn, int indiceArg);`
  — chequeo dinámico usado cuando el compilador no pudo verificar el tipo
  del argumento en compilación; aborta con mensaje claro si no coincide
  (mismo espíritu que `lat_verificar_tipo`, reutilizado por POO/genéricos).

## Mensajes de error

Compilación (`stderr`, mismo estilo que `AnalizadorSemantico`):

- `llamada a función externa 'X' fuera de un bloque 'inseguro'`
- `tipo FFI desconocido: 'Y' en la firma de 'X'`
- `número de argumentos incorrecto para la función externa 'X': se esperaban N, se recibieron M`
- `'X' ya está declarada como función externa` (colisión entre dos bloques `externo`)
- `'X' ya está declarada como función Latino` (colisión `externo` vs. función normal)

Runtime (vía `lat_ffi_verificar_tipo`):

- `ffi: se esperaba 'cadena' para el argumento 2 de 'MessageBoxA', se recibió lista`

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `src/lexer.cpp` | 3 palabras reservadas nuevas: `externo`, `enlazar`, `inseguro` |
| `include/ast.h` | `ExternoBloque`/`ExternoFn`/`InseguroBloque`; enum `TipoFFI`; campo `inseguro` en `FuncionDef` |
| `include/parser.h`, `src/parser.cpp` | `parseExterno()`, `parseInseguro()`, modificador `inseguro` en `parseFuncion()` |
| `include/ast_impresor.h`, `src/ast_impresor.cpp` | `visitar(ExternoBloque&)`/`visitar(InseguroBloque&)` (debug) |
| `include/analizador_semantico.h`, `src/analizador_semantico.cpp` | tabla `funcionesExternas`, pila `inseguroActivo`, chequeos de aridad/tipo |
| `src/compiler.cpp` | declaraciones `extern`/`#pragma comment(lib,...)`, marshalling de llamadas, lista de bibliotecas a enlazar |
| `src/compiler_llvm.cpp` | `declare` con firma nativa, marshalling con IR, lista de bibliotecas a enlazar |
| `src/invocador_c.cpp` | agregar `-l<lib>` en plataformas no-MSVC |
| `src/invocador_llvm.cpp` | agregar biblioteca de enlace al paso de linking AOT |
| `runtime/latino.h`, `runtime/latino.c` | `LAT_PUNTERO`, `lat_puntero`, `lat_ffi_verificar_tipo` |
| `tests/` | `test_ffi` (parser/semántico/codegen, backends `c` y `llvm`) y `test_ffi_e2e` (programas reales llamando símbolos de `libc`/`msvcrt` ya enlazados, sin depender de una `.dll` externa al repo) |
| `SINTAXIS.md` | sección nueva documentando `externo`/`enlazar`/`inseguro` |
| `CLAUDE.md`, `README.md` | entrada de estado al completar el plan, igual que se hizo con `PLAN_LLVM.md`/`PLAN_MODULOS.md`/`PLAN_GENERICOS.md` |

## Fases de implementación

- **F1 — Lexer y AST.** Palabras reservadas `externo`/`enlazar`/`inseguro`;
  nodos `ExternoBloque`/`ExternoFn`/`InseguroBloque`; enum `TipoFFI`; campo
  `inseguro` en `FuncionDef`. Sin lógica de resolución. Pruebas:
  `test_lexer`, `test_ast`.

- **F2 — Parser.** `parseExterno()` (con `enlazar` opcional, N firmas de
  función sin cuerpo), `parseInseguro()`, modificador `inseguro` en
  `parseFuncion()`. Pruebas: `test_parser` con las variantes de "Sintaxis
  propuesta".

- **F3 — Runtime: puntero opaco.** `LAT_PUNTERO` en `LatTipo`/`LatValor`,
  `lat_puntero`, decisión sobre `es nulo` para punteros, `lat_ffi_verificar_tipo`.
  Sin integración con el compilador todavía.

- **F4 — Análisis semántico.** Tabla `funcionesExternas`, pila
  `inseguroActivo`, chequeo de aridad y de colisión de nombres, chequeo
  estático de tipo cuando es posible (reusa infraestructura de
  `PLAN_TIPADO.md`). Pruebas: `test_semantico` (llamada fuera de
  `inseguro` → error, aridad incorrecta → error, tipo estático incorrecto
  → error).

- **F5 — Codegen backend C.** Declaraciones `extern`/`#pragma comment`,
  marshalling de argumentos/retorno, cambios en `invocador_c.cpp` para
  `-l<lib>` en no-MSVC. Verificar con un caso real (`abs`/`strlen` de
  `msvcrt`, sin `enlazar`).

- **F6 — Codegen backend LLVM.** `declare` con firma nativa, marshalling
  con IR (`extractvalue`/`trunc`/`call`), cambios en `invocador_llvm.cpp`
  para agregar bibliotecas de enlace. Verificar paridad de salida con el
  backend C sobre los mismos casos de F5 (mismo criterio de
  `PLAN_LLVM.md`/L12).

- **F7 — Pruebas de cobertura.** `tests/test_ffi.cpp` (parser/semántico/
  codegen, con subcadena de IR + `verifyModule` para LLVM, mismo patrón de
  `test_codegen_llvm.cpp`) y `tests/test_ffi_e2e.cpp` (programas `.lat`
  reales en ambos backends, sin depender de ninguna `.dll`/`.so` externa al
  repo — solo símbolos ya enlazados por defecto, ver Decisión de diseño 6).

- **F8 — Documentación y cierre.** Sección nueva en `SINTAXIS.md`, entrada
  de estado en `CLAUDE.md`/`README.md`, pase completo de `ctest` en serie.

## Riesgos técnicos

- **Pérdida de precisión en enteros de 64 bits.** `LAT_NUMERO` es un
  `double` (53 bits de mantisa); un `entero64`/`natural64` devuelto por una
  función C con un valor mayor a 2^53 pierde precisión al volver a Latino —
  limitación estructural del valor dinámico de Latino, igual que los
  `Number` de JavaScript. v1 documenta la limitación en `SINTAXIS.md` y no
  la resuelve (un `LAT_ENTERO64` dedicado, sin pérdida, queda para un plan
  futuro si aparece un caso de uso real).
- **Anchos de tipo C ambiguos por plataforma.** `int`/`long`/`size_t` no
  tienen el mismo ancho en todas las plataformas (p. ej. `long` es 32 bits
  en Windows x64 pero 64 bits en Linux x64). Mitigación ya incorporada en
  la Decisión de diseño 2: `externo` solo expone anchos fijos explícitos
  (`entero32`, `entero64`, etc.), nunca los nombres C ambiguos — el usuario
  debe saber el ancho real de la función que está declarando (igual que en
  Rust con `std::os::raw::c_long` vs. `i64` explícito).
- **Invariante de `cadena` sin bytes nulos embebidos.** El marshalling
  `cadena → const char*` asume terminador nulo simple (confirmado: el
  storage de `LAT_CADENA` en `runtime/latino.c` es un buffer
  `malloc`-eado de `len + 1` bytes, null-terminated). Si una cadena Latino
  contuviera un byte nulo interno, se truncaría silenciosamente al cruzar
  el borde FFI — riesgo ya inherente al valor `cadena` de Latino, no nuevo
  de este plan, pero más visible aquí porque C la trata como C-string.
- **Ciclo de vida de `puntero` es responsabilidad del programador.**
  `LAT_PUNTERO` es completamente opaco: Latino no tiene un destructor
  determinístico como el `Drop` de Rust, así que un `malloc` vía FFI que
  nunca recibe su `free` correspondiente simplemente fuga memoria — la
  misma responsabilidad que el `unsafe` de Rust deja en manos del
  programador, documentada como tal y no resuelta por el compilador.
- **Enlace multiplataforma con dos caminos de código.** `#pragma
  comment(lib,...)` solo lo entiende MSVC; GCC/Clang necesitan `-l<lib>`
  pasado por línea de comandos — mismo patrón de bifurcación por
  plataforma que ya usa `runtime/libs/paquete.c` (`LoadLibrary` vs.
  `dlopen`), pero ahora también en `invocador_c.cpp`/`invocador_llvm.cpp`,
  no solo en el runtime.
- **Convención de llamada asumida, no verificada.** v1 asume siempre la
  convención nativa del ABI de destino (correcta para x64 Windows/Linux,
  la única plataforma real hoy — ver `PLAN_LLVM.md`); declarar una función
  Win32 clásica de 32 bits con `stdcall` explícito quedaría mal enlazada
  sin ningún error de compilación, solo un crash o corrupción de pila en
  runtime. Documentado como asunción explícita, no un caso a detectar.

## Fuera de alcance

- **Bindgen automático** (generar firmas `externo` a partir de un header
  `.h` real, como el `bindgen` de Rust). v1 requiere escribir la firma a
  mano.
- **Structs/uniones C pasadas por valor.** v1 solo cubre tipos primitivos y
  punteros opacos; pasar o retornar un struct C arbitrario por valor
  requiere conocer su layout y reglas de ABI de agregados — el mismo
  problema que ya costó resolver para `LatValor` en la Fase L2 de
  `PLAN_LLVM.md`, pero generalizado a cualquier struct definido por el
  usuario. Se revisa en un plan futuro si aparece un caso de uso real.
- **Callbacks nativos** (pasar una función Latino como puntero a función C,
  p. ej. para `qsort`). Requiere generar una trampolina nativa con la
  firma C correcta por cada callback — fuera de v1.
- **Convenciones de llamada no nativas** (`stdcall` explícito en x86 de 32
  bits). v1 apunta solo a x64 (ver Decisión de diseño 7).
- **Unificar `externo` y `paquete` en una sola API.** Quedan como dos
  mecanismos independientes y complementarios (ver Decisión de diseño 8).
- **Un `LAT_ENTERO64` dedicado sin pérdida de precisión.** Ver Riesgos
  técnicos.
- **`link_name`/alias de símbolo** (declarar un nombre Latino distinto del
  símbolo C real, como `#[link_name = "..."]` en Rust) — se puede agregar
  sin romper compatibilidad en una fase posterior si hace falta (p. ej.
  para exponer un símbolo cuyo nombre colisione con una palabra reservada
  de Latino), pero v1 exige que el nombre de la función Latino coincida
  exactamente con el símbolo C.

## Estado

F1 completa (lexer y AST): tres palabras reservadas nuevas (`externo`,
`enlazar`, `inseguro`) en `src/lexer.cpp`; `enum class TipoFFI` y
`struct ParamFFI`/`FuncionExterna` en `include/ast.h` (vocabulario de tipos
FFI de la tabla de "Sintaxis propuesta"); nodos `ExternoBloque`/
`InseguroBloque` con `visitar()` no-op por defecto en `Visitante` (mismo
patrón que `ClaseDef`/`EstructuraDef`/`InterfazDef` en su fase inicial —
ningún visitante existente se rompe hasta que F4/F5/F6 les den lógica real);
campo `bool inseguro = false;` en `FuncionDef`. Sin lógica de parser ni de
resolución todavía. Pruebas: `test_lexer` (tokeniza las 3 palabras nuevas),
`test_ast` (construcción directa de `ExternoBloque` con/sin `enlazar`,
`FuncionExterna`/`ParamFFI` con los distintos `TipoFFI`, `InseguroBloque`, y
el modificador `FuncionDef::inseguro`).

F2 completa (parser): `Parser::parseExterno()`/`parseFuncionExterna()`/
`parseInseguro()` en `src/parser.cpp`, con despacho desde `parseSentencia()`
para las palabras reservadas `externo`/`inseguro`. `externo [enlazar
"lib"]` seguido de N firmas `funcion nombre(param: TipoFFI, ...): TipoFFI`
sin cuerpo, terminado en `fin`; el tipo de retorno es opcional y por
defecto es `TipoFFI::Nulo` (equivalente a omitir `: void` en C), pero el
tipo de cada parámetro es obligatorio (`esperarOperador(":")`, a diferencia
de los parámetros sin anotar de una función Latino normal) — no tiene
sentido un parámetro FFI sin tipo C explícito. `Parser::mapearNombreTipoFFI`
(nuevo, junto a `mapearNombreTipo`) traduce el lexema al enum `TipoFFI` y
devuelve `false` si no lo reconoce (a diferencia de `mapearNombreTipo`, que
nunca falla porque cualquier lexema no reconocido se interpreta como nombre
de clase de usuario — en FFI no existe ese fallback, así que un tipo
desconocido es directamente `tipo FFI desconocido: 'Y'`, error de
compilación). El modificador `inseguro` de `funcion inseguro nombre(...)`
se parsea dentro de `parseFuncion()`, entre `funcion`/`fun` y el nombre —
sin ambigüedad con el `inseguro` que abre un bloque, porque son posiciones
gramaticales distintas (`parseSentencia()` solo llega a la segunda forma
cuando `inseguro` es la primera palabra de la sentencia). `ImpresorAST`
gana `visitar(ExternoBloque&)`/`visitar(InseguroBloque&)` (volcado de una
línea por firma / del cuerpo del bloque, respectivamente, ya no no-op) y
marca `[inseguro]` en la firma de una `FuncionDef` que lleva el modificador
— mismo criterio que M2 de `PLAN_MODULOS.md` (el impresor gana soporte real
recién cuando el parser puede producir el nodo).

Pruebas: `tests/test_parser.cpp` (bloque `externo` con y sin `enlazar`,
tipo de retorno por defecto `nulo` cuando se omite `:`, bloque `inseguro`,
`funcion inseguro nombre(...)`, y dos casos negativos que confirman que el
parser devuelve `nullptr` en un tipo FFI desconocido y en un parámetro
`externo` sin `:` de tipo). Suite completa de CTest verificada en verde
tras el cambio (ver nota de `ctest -j` en `CLAUDE.md`).

Alcance de F1/F2, tal como lo define este plan: solo sintaxis y estructura
del AST. Una llamada a una función `externo` fuera de un bloque `inseguro`
**no** se rechaza todavía (llega en F4); una llamada a una función
`externo` dentro de `inseguro` tampoco genera código real todavía (F5/F6)
— hoy el compilador la trataría como una llamada a una función Latino
normal no declarada, y fallaría más adelante en el análisis semántico con
un mensaje genérico ("función no declarada"), no con los mensajes
específicos de FFI de la sección "Mensajes de error" — comportamiento
esperado en esta fase, no un bug.
