# PLAN_MACROS.md — Macros al estilo de Julia para Latino

## Objetivo

Agregar al lenguaje un sistema de macros completo, inspirado en Julia: la
programa puede definir funciones que se ejecutan **en tiempo de compilación**
y que reciben **expresiones sin evaluar**, las inspeccionan, las transforman y
devuelven nuevo código Latino que se inserta en el AST antes del análisis
semántico y de cualquiera de los dos backends (C y LLVM).

Se descartó explícitamente el subconjunto "solo plantillas `quote`/`$` +
macros integradas": el objetivo pedido fue el sistema completo, con
manipulación programática del AST, higiene y `esc`.

## Decisiones de diseño

1. **Las macros se escriben en Latino y se interpretan en C++.** No se agrega
   un tipo `LAT_EXPR` al runtime ni se compilan las macros a código nativo:
   eso exigiría una ABI nueva para pasar AST entre el ejecutable y la macro y
   haría que el compilador dependiera de compilar/ejecutar código en cada
   invocación. En su lugar, `ExpandidorMacros` contiene un intérprete pequeño
   sobre valores `ValorMacro` (nulo, número, cadena, lógico, símbolo,
   expresión, lista, diccionario y función).
2. **Punto de integración:** después de `elevarFuncionesAnidadas`, de la
   resolución de módulos y de la expansión de `incluir "archivo.lat"`, y antes
   de `AnalizadorSemantico`. Los nodos nuevos (`MacroDef`,
   `MacroInvocacion`, `CitaExpr`, ...) se eliminan del AST en esa fase; por eso
   `Visitante` les da implementación no-op y ni semántico ni backends tuvieron
   que aprender a visitarlos.
3. **Los parámetros se enlazan a AST, no a valores.** `@m 1 + 2` entrega el
   subárbol `Binaria("+", 1, 2)`. Si la macro quiere el valor, debe insertar
   la expresión en el código generado (normalmente con `$x` dentro de un
   `quote`).
4. **Higiene por renombrado de ligaduras introducidas.** Las variables que el
   `quote` introduce (destinos de asignación, variables de `para`, nombres de
   funciones anónimas/definidas) se renombran con un sufijo único. Las
   expresiones interpoladas con `$` y las marcadas con `esc()` se respetan.
5. **`@expandir_macro` necesita un impresor de fuente nuevo.** `ImpresorAST` es
   un volcado de depuración que no se puede volver a parsear;
   `ImpresorFuente` genera código Latino legible desde el AST.
6. **`@tiempo` usa un reloj monotónico nuevo en el runtime core**,
   `lat_tiempo_ns()` (`clock_gettime(CLOCK_MONOTONIC)` en POSIX,
   `QueryPerformanceCounter` en Windows), registrado también en
   `tools/abi_probe.c` para que el backend LLVM pueda declararlo con la ABI
   real del runtime.
7. **`@afirmar` se baja a `error(...)`**, builtin ya existente en los dos
   backends, para no exigir `incluir "sis"` en el programa de usuario.

## Sintaxis

```python
macro nombre(p0, p1, ...)
    ...cuerpo ejecutado en tiempo de compilación...
fin

@nombre argumento
@nombre(arg0, arg1, ...)

quote
    ...código sin evaluar...
fin

$expresion        # interpolación (dentro de quote o del cuerpo de macro)
:nombre           # símbolo
esc(expresion)    # excluye un fragmento de la higiene
Expr(:llamada, :escribir, 1)
```

Palabras reservadas nuevas: `macro`, `quote`. Operadores nuevos: `@`, `$`.

## Fases

### M1 — Lexer y AST (completada)

- `@` y `$` lexean como operadores.
- `macro` y `quote` son palabras reservadas.
- Nodos: `MacroDef`, `MacroInvocacion`, `CitaExpr`, `InterpolacionExpr`,
  `SimboloExpr`, `BloqueMacroExpr` y `FragmentoExpr`.
- `Nodo::macroEscapado` marca los fragmentos inyectados con `esc()`/`$`.

### M2 — Parser (completada)

- `parseMacro()` para `macro nombre(params) ... fin`.
- `parseQuote()` para `quote ... fin` como expresión.
- `@nombre args` en `parsePrimario()`: forma Julia (un argumento que absorbe
  los operadores de la línea, o varios separados por coma) y forma con
  paréntesis.
- `$expr` y `:simbolo` en `parseUnario()`. El `:` solo se lee como símbolo si
  el token siguiente es un identificador, para no interferir con anotaciones
  de tipo ni con diccionarios.

### M3 — Impresor de fuente (completada)

`include/impresor_fuente.h` / `src/impresor_fuente.cpp`: AST → Latino
reparseable, usado por `@expandir_macro` y por `string(expr)` del intérprete.

### M4 — Expandidor / intérprete compile-time (completada)

`include/expandidor_macros.h` / `src/expandidor_macros.cpp`:

- Recolección de todas las definiciones antes de expander (permite que una
  macro use a otra definida más abajo).
- Entorno con ámbitos, asignación, `si`/`osi`/`sino`, `mientras`, `desde`,
  `para`, `repetir`, `romper`, `retornar`, funciones definidas y anónimas.
- Operadores aritméticos, de comparación, lógicos, concatenación `..`,
  ternario, indexación de listas/diccionarios y acceso a `.tipo`/`.args` de
  un `Expr`.
- Builtins de macro: `esc`, `string`/`acadena`, `simbolo`/`symbol`,
  `generar_simbolo` (alias `gensimb`), `Expr`/`expr`, `longitud`, `tipo`, `escribir`/`imprimir`/`poner`,
  `rango`, `error`.
- Copia del AST en tres modos: copiar, interpolar `$`, expander `@`.
- Expansión recursiva de invocaciones dentro de expresiones, con límite de
  profundidad para evitar recursión infinita.

### M5 — Integración en el pipeline (completada)

`src/main.cpp` invoca `ExpandidorMacros::expandir` justo antes del análisis
semántico; el texto producido por `@expandir_macro` se escribe en stdout.
`src/CMakeLists.txt` compila `impresor_fuente.cpp` y `expandidor_macros.cpp`.

### M6 — Macros integradas (completada)

- `@mostrar`: escribe `expresión = valor` usando `acadena` y `escribir`.
- `@afirmar` (alias `@asegurar`): baja a
  `si (cond == falso) { error("afirmación falló: ...") }`.
- `@tiempo`: mide con `tiempo_ns()` e imprime milisegundos.
- `@expandir_macro`: escribe el resultado de la expansión en stdout.
- Los nombres en inglés (`@show`, `@assert`, `@time`, `@macroexpand`, `gensym`)
  se aceptan como alias por compatibilidad; las formas canónicas son las
  españolas (`@mostrar`, `@afirmar`/`@asegurar`, `@tiempo`,
  `@expandir_macro`, `generar_simbolo`/`gensimb`).

### M7 — Pruebas y documentación (completada y verificada en C y LLVM)

- `tests/test_macros.cpp`: parser, impresor, higiene, `esc()` y expansión unitaria.
- `tests/test_macros_e2e.cpp`: 11 programas completos (ambos backends).
- `ejemplos/macros.lat` con `#salida:` (registrado en `test_e2e`).
- `SINTAXIS.md`, `README.md`, `CLAUDE.md`.

Corrección relevante durante M7: `construirExpr` recibe los argumentos **sin**
el símbolo de tipo (que `Expr()` separa), y `@afirmar` baja la negación a
`cond == falso` porque `lat_negar` del runtime es negación numérica, no lógica.

## Fuera de alcance de v1

- Macros exportadas/importadas entre módulos (`exportar macro` se parsea pero
  no se resuelve en `ResolutorModulos`).
- Macros variádicas y patrones de argumentos.
- Bloques multi-sentencia usados como valor de una expresión (se permiten como
  sentencia; ver limitación documentada en SINTAXIS.md).
- Sustitución de tipos en macros genéricas y unificación con el plan de
  genéricos más allá del uso de `Expr`/`:simbolo`.
- Higiene completa con resolución léxica de variables libres al ámbito de
  definición de la macro (v1 renombra las ligaduras introducidas y respeta
  `$`/`esc`).
