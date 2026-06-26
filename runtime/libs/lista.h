/* lista.h — librería estándar de listas para programas Latino transpilados.
 *
 * Uso en Latino: lista.longitud(l), lista.agregar(l, v), etc.
 * El compilador mapea  lista.foo(args)  →  lat_lista_foo(args)  en C.
 *
 * Las funciones que modifican la lista lo hacen in-place a través del
 * puntero interno; las que crean una nueva lista devuelven un LatValor nuevo.
 */

#ifndef LATINO_LISTA_H
#define LATINO_LISTA_H

#include "latino.h"

/* Modificación in-place */
LatValor lat_lista_agregar(LatValor l, LatValor v);
LatValor lat_lista_eliminar(LatValor l, LatValor v);
LatValor lat_lista_eliminar_indice(LatValor l, LatValor i);
LatValor lat_lista_extender(LatValor l1, LatValor l2);
LatValor lat_lista_insertar(LatValor l, LatValor i, LatValor v);
LatValor lat_lista_invertir(LatValor l);

/* Creación de nueva lista */
LatValor lat_lista_concatenar(LatValor l1, LatValor l2);
LatValor lat_lista_crear(LatValor n, LatValor v);

/* Consulta */
LatValor lat_lista_comparar(LatValor l1, LatValor l2);
LatValor lat_lista_contiene(LatValor l, LatValor v);
LatValor lat_lista_encontrar(LatValor l, LatValor v);
LatValor lat_lista_indice(LatValor l, LatValor v);   /* alias de encontrar */
LatValor lat_lista_longitud(LatValor l);
LatValor lat_lista_separador(LatValor l, LatValor sep);

/* Fase 21 — Modificación in-place */
LatValor lat_lista_ordenar(LatValor l);

/* Fase 21 — Creación de nueva lista */
LatValor lat_lista_unico(LatValor l);
LatValor lat_lista_rebanada(LatValor l, LatValor ini, LatValor fin);

/* Fase 21 — Acceso rápido */
LatValor lat_lista_primero(LatValor l);
LatValor lat_lista_ultimo(LatValor l);

/* Fase 21 — Consulta adicional */
LatValor lat_lista_contar(LatValor l, LatValor v);

#endif /* LATINO_LISTA_H */
