/* dic.h — librería estándar de diccionarios para programas Latino transpilados.
 *
 * Uso en Latino: dic.longitud(d), dic.llaves(d), etc.
 * El compilador mapea  dic.foo(args)  →  lat_dic_foo(args)  en C.
 *
 * Las funciones que modifican el diccionario lo hacen in-place a través del
 * puntero interno; las que crean colecciones nuevas devuelven un LatValor nuevo.
 */

#ifndef LATINO_DIC_H
#define LATINO_DIC_H

#include "latino.h"

/* Consulta */
LatValor lat_dic_contiene(LatValor d, LatValor clave);
LatValor lat_dic_longitud(LatValor d);
LatValor lat_dic_llaves(LatValor d);
LatValor lat_dic_valores(LatValor d);
LatValor lat_dic_vals(LatValor d);   /* alias de valores */

/* Modificación in-place */
LatValor lat_dic_eliminar(LatValor d, LatValor clave);
LatValor lat_dic_actualizar(LatValor d, LatValor d2);

/* Fase 22 — Creación / consulta adicional */
LatValor lat_dic_combinar(LatValor d1, LatValor d2);
LatValor lat_dic_items(LatValor d);
LatValor lat_dic_copiar(LatValor d);

#endif /* LATINO_DIC_H */
