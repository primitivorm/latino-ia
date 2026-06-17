/* dic.c — implementación de la librería de diccionarios de Latino. */

#define _CRT_SECURE_NO_WARNINGS

#include "dic.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Utilidades internas
 * ---------------------------------------------------------------------- */

static LatDic *dic_de_valor(LatValor v) {
    return (v.tipo == LAT_DICCIONARIO && v.como.dic) ? v.como.dic : NULL;
}

/* Crea una LatLista vacía con capacidad inicial. */
static LatLista *lista_nueva(size_t cap) {
    LatLista *l = (LatLista *)malloc(sizeof(LatLista));
    if (!l) return NULL;
    l->capacidad = cap ? cap : 4;
    l->longitud  = 0;
    l->datos = (LatValor *)malloc(sizeof(LatValor) * l->capacidad);
    if (!l->datos) { free(l); return NULL; }
    return l;
}

/* -------------------------------------------------------------------------
 * 1. dic.contiene(d, clave) — cierto si clave existe en el diccionario
 * ---------------------------------------------------------------------- */
LatValor lat_dic_contiene(LatValor dv, LatValor clave_v) {
    LatDic *d = dic_de_valor(dv);
    if (!d) return lat_logico(0);
    char *clave = lat_a_cadena(clave_v);
    for (size_t i = 0; i < d->longitud; i++) {
        if (strcmp(d->claves[i], clave) == 0) {
            free(clave);
            return lat_logico(1);
        }
    }
    free(clave);
    return lat_logico(0);
}

/* -------------------------------------------------------------------------
 * 2. dic.eliminar(d, clave) — elimina la entrada con esa clave (in-place)
 * ---------------------------------------------------------------------- */
LatValor lat_dic_eliminar(LatValor dv, LatValor clave_v) {
    LatDic *d = dic_de_valor(dv);
    if (!d) return lat_nulo();
    char *clave = lat_a_cadena(clave_v);
    for (size_t i = 0; i < d->longitud; i++) {
        if (strcmp(d->claves[i], clave) == 0) {
            free(d->claves[i]);
            memmove(&d->claves[i],  &d->claves[i + 1],
                    sizeof(char *)   * (d->longitud - i - 1));
            memmove(&d->valores[i], &d->valores[i + 1],
                    sizeof(LatValor) * (d->longitud - i - 1));
            d->longitud--;
            free(clave);
            return lat_nulo();
        }
    }
    free(clave);
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 3. dic.llaves(d) — lista nueva con todas las claves como cadenas
 * ---------------------------------------------------------------------- */
LatValor lat_dic_llaves(LatValor dv) {
    LatDic *d = dic_de_valor(dv);
    size_t n = d ? d->longitud : 0;
    LatLista *l = lista_nueva(n ? n : 1);
    if (!l) return lat_nulo();
    for (size_t i = 0; i < n; i++)
        l->datos[l->longitud++] = lat_cadena(d->claves[i]);
    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = l;
    return v;
}

/* -------------------------------------------------------------------------
 * 4. dic.longitud(d) — número de entradas
 * ---------------------------------------------------------------------- */
LatValor lat_dic_longitud(LatValor dv) {
    LatDic *d = dic_de_valor(dv);
    return lat_numero(d ? (double)d->longitud : 0.0);
}

/* -------------------------------------------------------------------------
 * 5. dic.valores(d) — lista nueva con todos los valores
 *    dic.vals es un alias
 * ---------------------------------------------------------------------- */
LatValor lat_dic_valores(LatValor dv) {
    LatDic *d = dic_de_valor(dv);
    size_t n = d ? d->longitud : 0;
    LatLista *l = lista_nueva(n ? n : 1);
    if (!l) return lat_nulo();
    for (size_t i = 0; i < n; i++)
        l->datos[l->longitud++] = d->valores[i];
    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = l;
    return v;
}

LatValor lat_dic_vals(LatValor dv) {
    return lat_dic_valores(dv);
}
