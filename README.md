# latino-ia

Compilador para el lenguaje de programación **Latino**, escrito en C++17.

La especificación completa del lenguaje está en [WORK.md](WORK.md): variables, constantes,
operadores, estructuras de control (`si/sino/osi`, `elegir`, `desde`, `mientras`,
`repetir-hasta`, `romper`), funciones (con argumentos variables), listas y diccionarios.

## Estrategia

El compilador **transpila Latino a C** y luego delega la generación del binario al compilador
de C del sistema. Esto se alinea con la tabla de tipos de [WORK.md](WORK.md), que ya mapea cada
tipo de Latino a un tipo de C (lógico → `bool`, numérico → `double`, cadena → `char*`,
lista/diccionario → `struct`).

El flujo del compilador es el clásico de un lenguaje de programación:

```
archivo .lat → Lexer → Parser (AST) → Análisis semántico → Generación de código C → compilador C → ejecutable
```

## Estructura del proyecto

| Carpeta / archivo | Contenido |
| ----------------- | --------- |
| `src/`            | Código fuente del compilador (lexer, parser, compiler, driver). |
| `include/`        | Cabeceras (interfaces) del compilador. |
| `runtime/`        | Biblioteca de soporte en C que usa el código generado (tipos, listas, diccionarios, `escribir`, etc.). |
| `ejemplos/`       | Programas `.lat` de ejemplo derivados de [WORK.md](WORK.md); sirven como casos de prueba de extremo a extremo. |
| `WORK.md`         | Especificación del lenguaje (fuente de verdad). |

La extensión de archivo de los programas Latino es **`.lat`**.

## Estado

Proyecto en etapa temprana. Avance por fases (ver el plan de trabajo):

- [x] Fase 0 — Cimientos del proyecto (este README, `ejemplos/`, `runtime/`).
- [x] Fase 1 — Lexer completo (operadores, delimitadores, comentarios, cadenas, conteo de líneas).
- [ ] Fase 2 — Diseño del AST.
- [ ] Fase 3 — Parser por descenso recursivo que produce el AST.
- [ ] Fase 4 — Análisis semántico.
- [ ] Fase 5 — Generación de código C.
- [ ] Fase 6 — Biblioteca de runtime en C.
- [ ] Fase 7 — Driver / CLI.
- [ ] Fase 8 — Pruebas y ejemplos.

## Construir

Requisitos: CMake 3.12+ y un compilador de C++17 (en Windows, Visual Studio 2022).

```powershell
# Genera la solución de Visual Studio 2022 en build/
.\generar-salida.ps1

# Compila
cmake --build build
```

El ejecutable resultante se llama `latino`.

## Uso (objetivo, en construcción)

```powershell
latino ejemplos/hola.lat -o hola.exe   # compila a ejecutable
latino ejemplos/hola.lat --solo-c      # emite solo el .c generado
latino ejemplos/hola.lat --ast         # vuelca el AST (depuración)
```
