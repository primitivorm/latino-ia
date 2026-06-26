/* cadena.h — librería estándar de cadenas para programas Latino transpilados.
 *
 * Uso en Latino: cadena.longitud(s), cadena.mayusculas(s), etc.
 * El compilador mapea  cadena.foo(args)  →  lat_cadena_foo(args)  en C.
 *
 * Todas las funciones que reciben o devuelven cadenas usan memoria asignada
 * en el heap (malloc); siguiendo la política del runtime base, no se libera
 * (suficiente para programas cortos).
 */

#ifndef LATINO_CADENA_H
#define LATINO_CADENA_H

#include "latino.h"
#include <stddef.h>

/* Conversión / inspección */
LatValor lat_cadena_bytes(LatValor s);               /* lista de valores numéricos de cada byte */
LatValor lat_cadena_char(LatValor n);                /* carácter ASCII del código n             */
LatValor lat_cadena_longitud(LatValor s);            /* número de bytes (strlen)                */
LatValor lat_cadena_esta_vacia(LatValor s);          /* cierto si longitud == 0                 */
LatValor lat_cadena_es_alfa(LatValor s);             /* cierto si todos los chars son letras    */
LatValor lat_cadena_es_numerico(LatValor s);         /* cierto si representa un número          */

/* Búsqueda */
LatValor lat_cadena_contiene(LatValor s, LatValor sub);           /* cierto / falso            */
LatValor lat_cadena_encontrar(LatValor s, LatValor sub);          /* índice o -1               */
LatValor lat_cadena_indice(LatValor s, LatValor sub);             /* alias de encontrar        */
LatValor lat_cadena_ultimo_indice(LatValor s, LatValor sub);      /* última ocurrencia o -1    */
LatValor lat_cadena_inicia_con(LatValor s, LatValor prefijo);     /* cierto / falso            */
LatValor lat_cadena_termina_con(LatValor s, LatValor sufijo);     /* cierto / falso            */

/* Comparación */
LatValor lat_cadena_comparar(LatValor s1, LatValor s2);           /* -1, 0 ó 1 (strcmp)       */
LatValor lat_cadena_es_igual(LatValor s1, LatValor s2);           /* ignora mayúsculas/minus. */

/* Transformación */
LatValor lat_cadena_mayusculas(LatValor s);
LatValor lat_cadena_minusculas(LatValor s);
LatValor lat_cadena_invertir(LatValor s);
LatValor lat_cadena_recortar(LatValor s);                         /* elimina espacios extremos */
LatValor lat_cadena_concatenar(LatValor s1, LatValor s2);
LatValor lat_cadena_reemplazar(LatValor s, LatValor viejo, LatValor nuevo);
LatValor lat_cadena_insertar(LatValor s, LatValor pos, LatValor sub);
LatValor lat_cadena_subcadena(LatValor s, LatValor inicio, LatValor fin);
LatValor lat_cadena_rellenar_derecha(LatValor s, LatValor n, LatValor c);
LatValor lat_cadena_rellenar_izquierda(LatValor s, LatValor n, LatValor c);

/* División */
LatValor lat_cadena_separar(LatValor s, LatValor delim);          /* → lista de subcadenas     */

/* Formato */
LatValor lat_cadena_formato(size_t n, ...);                       /* sprintf de LatValores     */

/* Expresiones regulares (implementación básica sin POSIX) */
LatValor lat_cadena_regex(LatValor s, LatValor patron);           /* primera coincidencia o nulo */
LatValor lat_cadena_regexl(LatValor s, LatValor patron);          /* lista de coincidencias    */

/* Fase 23 — Búsqueda adicional */
LatValor lat_cadena_contar(LatValor s, LatValor sub);             /* ocurrencias no solapadas  */

/* Fase 23 — Transformación adicional */
LatValor lat_cadena_titulo(LatValor s);                           /* primera letra de cada palabra en mayúscula */
LatValor lat_cadena_capitalizar(LatValor s);                      /* primera letra mayúscula, resto minúscula   */
LatValor lat_cadena_recortar_izq(LatValor s);                     /* elimina espacios a la izquierda            */
LatValor lat_cadena_recortar_der(LatValor s);                     /* elimina espacios a la derecha              */

/* Fase 23 — Inspección adicional */
LatValor lat_cadena_es_espacio(LatValor s);                       /* cierto si todos son espacios en blanco     */

#endif /* LATINO_CADENA_H */
