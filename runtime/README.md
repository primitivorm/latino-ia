# runtime

Biblioteca de soporte en **C** que utiliza el código generado por el compilador de Latino.

Se implementará en la **Fase 6** del plan de trabajo. Contendrá:

- `latino.h` / `latino.c`: el tipo `Valor` (unión etiquetada para lógico, numérico `double`,
  cadena `char*`, lista, diccionario y `nulo`), acorde a la tabla de tipos de
  [WORK.md](../WORK.md) sección IV.
- Operaciones aritméticas, lógicas, relacionales, concatenación (`..`), indexación de listas
  (incluida la indexación negativa) y acceso a diccionarios por clave.
- Funciones integradas usadas por los programas Latino: `escribir`, `imprimir`, etc.

El código C generado por el compilador hará `#include "latino.h"` y enlazará con esta
biblioteca.
