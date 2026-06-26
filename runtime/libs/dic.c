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
    l->refs      = 1;
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

/* -------------------------------------------------------------------------
 * Utilidad interna: crea un LatDic vacío con capacidad dada.
 * ---------------------------------------------------------------------- */
static LatDic *dic_nuevo_local(size_t cap) {
    LatDic *d = (LatDic *)malloc(sizeof(LatDic));
    if (!d) return NULL;
    d->refs      = 1;
    d->longitud  = 0;
    d->capacidad = cap ? cap : 4;
    d->claves  = (char **)malloc(sizeof(char *) * d->capacidad);
    d->valores = (LatValor *)malloc(sizeof(LatValor) * d->capacidad);
    if (!d->claves || !d->valores) { free(d->claves); free(d->valores); free(d); return NULL; }
    return d;
}

/* Inserta (o sobreescribe) una entrada en un LatDic. */
static void dic_insertar(LatDic *d, const char *clave, LatValor valor) {
    /* Busca si ya existe */
    for (size_t i = 0; i < d->longitud; i++) {
        if (strcmp(d->claves[i], clave) == 0) { d->valores[i] = valor; return; }
    }
    /* Crece si es necesario */
    if (d->longitud == d->capacidad) {
        d->capacidad *= 2;
        d->claves  = (char **)realloc(d->claves,  sizeof(char *) * d->capacidad);
        d->valores = (LatValor *)realloc(d->valores, sizeof(LatValor) * d->capacidad);
    }
    d->claves[d->longitud]  = (char *)malloc(strlen(clave) + 1);
    if (d->claves[d->longitud]) strcpy(d->claves[d->longitud], clave);
    d->valores[d->longitud] = valor;
    d->longitud++;
}

/* -------------------------------------------------------------------------
 * Fase 22 — 6. dic.combinar(d1, d2)
 * Devuelve un nuevo diccionario con todas las entradas de d1 y d2.
 * Si una clave existe en ambos, d2 tiene precedencia.
 * ---------------------------------------------------------------------- */
LatValor lat_dic_combinar(LatValor d1v, LatValor d2v) {
    LatDic *d1 = dic_de_valor(d1v);
    LatDic *d2 = dic_de_valor(d2v);
    size_t n1 = d1 ? d1->longitud : 0;
    size_t n2 = d2 ? d2->longitud : 0;

    LatDic *r = dic_nuevo_local(n1 + n2 ? n1 + n2 : 4);
    if (!r) return lat_nulo();

    for (size_t i = 0; i < n1; i++) dic_insertar(r, d1->claves[i], d1->valores[i]);
    for (size_t i = 0; i < n2; i++) dic_insertar(r, d2->claves[i], d2->valores[i]);

    LatValor v;
    v.tipo = LAT_DICCIONARIO;
    v.como.dic = r;
    return v;
}

/* -------------------------------------------------------------------------
 * Fase 22 — 7. dic.items(d)
 * Devuelve una lista de listas [[clave, valor], ...].
 * ---------------------------------------------------------------------- */
LatValor lat_dic_items(LatValor dv) {
    LatDic *d = dic_de_valor(dv);
    size_t n = d ? d->longitud : 0;

    LatLista *result = lista_nueva(n ? n : 1);
    if (!result) return lat_nulo();

    for (size_t i = 0; i < n; i++) {
        LatLista *par = lista_nueva(2);
        if (!par) break;
        par->datos[0] = lat_cadena(d->claves[i]);
        par->datos[1] = d->valores[i];
        par->longitud = 2;
        LatValor vpar;
        vpar.tipo = LAT_LISTA;
        vpar.como.lista = par;
        result->datos[result->longitud++] = vpar;
    }

    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = result;
    return v;
}

/* -------------------------------------------------------------------------
 * Fase 22 — 8. dic.copiar(d)
 * Devuelve una copia superficial del diccionario.
 * ---------------------------------------------------------------------- */
LatValor lat_dic_copiar(LatValor dv) {
    LatDic *src = dic_de_valor(dv);
    size_t n = src ? src->longitud : 0;

    LatDic *r = dic_nuevo_local(n ? n : 4);
    if (!r) return lat_nulo();

    for (size_t i = 0; i < n; i++)
        dic_insertar(r, src->claves[i], src->valores[i]);

    LatValor v;
    v.tipo = LAT_DICCIONARIO;
    v.como.dic = r;
    return v;
}

/* -------------------------------------------------------------------------
 * Fase 22 — 9. dic.actualizar(d, d2)
 * Inserta/sobreescribe in-place las entradas de d2 en d.
 * ---------------------------------------------------------------------- */
LatValor lat_dic_actualizar(LatValor dv, LatValor d2v) {
    LatDic *d  = dic_de_valor(dv);
    LatDic *d2 = dic_de_valor(d2v);
    if (!d || !d2) return lat_nulo();
    for (size_t i = 0; i < d2->longitud; i++)
        dic_insertar(d, d2->claves[i], d2->valores[i]);
    return lat_nulo();
}
