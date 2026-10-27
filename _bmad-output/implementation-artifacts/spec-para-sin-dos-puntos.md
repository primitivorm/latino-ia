---
title: 'Rechazar dos puntos en la cabecera de para rango'
type: 'bugfix'
created: '2026-10-27'
status: 'done'
route: 'one-shot'
---

# Rechazar dos puntos en la cabecera de para rango

## Intent

**Problem:** El parser aceptaba `:` al final de `para ... en rango(...)`, aunque esa puntuación no pertenece a la sintaxis de Latino.

**Approach:** Rechazar el `:` mediante el parser existente, cubrir explícitamente el error sintáctico y alinear ejemplos y documentación a la forma sin dos puntos.

## Suggested Review Order

**Sintaxis y diagnóstico**

- La cabecera parsea el iterable y exige que el bloque empiece en la siguiente línea.
  [`parser.cpp:462`](../../src/parser.cpp#L462)

- La prueba asegura que la forma con `:` produce error de sintaxis.
  [`test_parser.cpp:141`](../../tests/test_parser.cpp#L141)

**Compatibilidad de uso**

- El ejemplo E2E usa cabeceras válidas sin dos puntos.
  [`rango.lat:4`](../../ejemplos/rango.lat#L4)

- La referencia del lenguaje deja claro que la puntuación final está prohibida.
  [`SINTAXIS.md:273`](../../SINTAXIS.md#L273)
