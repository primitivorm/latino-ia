/* lista.c — implementación de la librería de listas de Latino. */

#define _CRT_SECURE_NO_WARNINGS

#include "lista.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Utilidades internas
 * ---------------------------------------------------------------------- */

static LatLista *lista_de_valor(LatValor v) {
    return (v.tipo == LAT_LISTA && v.como.lista) ? v.como.lista : NULL;
}

static int valores_iguales(LatValor a, LatValor b) {
    return lat_es_verdadero(lat_igual(a, b));
}

/* Garantiza que la lista tiene al menos cap_extra espacios libres. */
static void lista_crecer(LatLista *l, size_t extra) {
    if (l->longitud + extra <= l->capacidad) return;
    size_t nueva_cap = l->capacidad ? l->capacidad * 2 : 4;
    while (nueva_cap < l->longitud + extra) nueva_cap *= 2;
    l->datos = (LatValor *)realloc(l->datos, sizeof(LatValor) * nueva_cap);
    l->capacidad = nueva_cap;
}

/* -------------------------------------------------------------------------
 * 1. lista.agregar(l, v) — agrega v al final
 * ---------------------------------------------------------------------- */
LatValor lat_lista_agregar(LatValor lv, LatValor v) {
    LatLista *l = lista_de_valor(lv);
    if (!l) return lat_nulo();
    lista_crecer(l, 1);
    l->datos[l->longitud++] = v;
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 2. lista.comparar(l1, l2) — compara elemento a elemento
 *    devuelve -1, 0 ó 1
 * ---------------------------------------------------------------------- */
LatValor lat_lista_comparar(LatValor l1v, LatValor l2v) {
    LatLista *l1 = lista_de_valor(l1v);
    LatLista *l2 = lista_de_valor(l2v);
    if (!l1 || !l2) return lat_numero(0.0);
    size_t min_len = l1->longitud < l2->longitud ? l1->longitud : l2->longitud;
    for (size_t i = 0; i < min_len; i++) {
        if (!valores_iguales(l1->datos[i], l2->datos[i]))
            return lat_numero(lat_es_verdadero(lat_menor(l1->datos[i], l2->datos[i]))
                              ? -1.0 : 1.0);
    }
    if (l1->longitud < l2->longitud) return lat_numero(-1.0);
    if (l1->longitud > l2->longitud) return lat_numero(1.0);
    return lat_numero(0.0);
}

/* -------------------------------------------------------------------------
 * 3. lista.concatenar(l1, l2) — nueva lista con todos los elementos de ambas
 * ---------------------------------------------------------------------- */
LatValor lat_lista_concatenar(LatValor l1v, LatValor l2v) {
    LatLista *l1 = lista_de_valor(l1v);
    LatLista *l2 = lista_de_valor(l2v);
    size_t n1 = l1 ? l1->longitud : 0;
    size_t n2 = l2 ? l2->longitud : 0;
    size_t total = n1 + n2;

    LatLista *r = (LatLista *)malloc(sizeof(LatLista));
    if (!r) return lat_nulo();
    r->refs      = 1;
    r->capacidad = total ? total : 4;
    r->longitud  = total;
    r->datos     = (LatValor *)malloc(sizeof(LatValor) * r->capacidad);
    if (!r->datos) { free(r); return lat_nulo(); }

    if (l1) memcpy(r->datos,      l1->datos, sizeof(LatValor) * n1);
    if (l2) memcpy(r->datos + n1, l2->datos, sizeof(LatValor) * n2);

    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = r;
    return v;
}

/* -------------------------------------------------------------------------
 * 4. lista.contiene(l, v) — cierto si v está en la lista
 * ---------------------------------------------------------------------- */
LatValor lat_lista_contiene(LatValor lv, LatValor v) {
    LatLista *l = lista_de_valor(lv);
    if (!l) return lat_logico(0);
    for (size_t i = 0; i < l->longitud; i++)
        if (valores_iguales(l->datos[i], v))
            return lat_logico(1);
    return lat_logico(0);
}

/* -------------------------------------------------------------------------
 * 5. lista.crear(n, v) — crea lista de n elementos todos con valor v
 * ---------------------------------------------------------------------- */
LatValor lat_lista_crear(LatValor nv, LatValor v) {
    int n = (int)(nv.tipo == LAT_NUMERO ? nv.como.numero : 0);
    if (n < 0) n = 0;

    LatLista *l = (LatLista *)malloc(sizeof(LatLista));
    if (!l) return lat_nulo();
    l->refs      = 1;
    l->capacidad = n ? (size_t)n : 4;
    l->longitud  = (size_t)n;
    l->datos     = (LatValor *)malloc(sizeof(LatValor) * l->capacidad);
    if (!l->datos) { free(l); return lat_nulo(); }
    for (int i = 0; i < n; i++) l->datos[i] = v;

    LatValor r;
    r.tipo = LAT_LISTA;
    r.como.lista = l;
    return r;
}

/* -------------------------------------------------------------------------
 * 6. lista.eliminar(l, v) — elimina primera ocurrencia de v
 * ---------------------------------------------------------------------- */
LatValor lat_lista_eliminar(LatValor lv, LatValor v) {
    LatLista *l = lista_de_valor(lv);
    if (!l) return lat_nulo();
    for (size_t i = 0; i < l->longitud; i++) {
        if (valores_iguales(l->datos[i], v)) {
            memmove(&l->datos[i], &l->datos[i + 1],
                    sizeof(LatValor) * (l->longitud - i - 1));
            l->longitud--;
            return lat_nulo();
        }
    }
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 7. lista.eliminar_indice(l, i) — elimina el elemento en índice i
 * ---------------------------------------------------------------------- */
LatValor lat_lista_eliminar_indice(LatValor lv, LatValor iv) {
    LatLista *l = lista_de_valor(lv);
    if (!l) return lat_nulo();
    int idx = (int)(iv.tipo == LAT_NUMERO ? iv.como.numero : 0);
    if (idx < 0) idx = (int)l->longitud + idx;
    if (idx < 0 || (size_t)idx >= l->longitud) return lat_nulo();
    memmove(&l->datos[idx], &l->datos[idx + 1],
            sizeof(LatValor) * (l->longitud - (size_t)idx - 1));
    l->longitud--;
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 8. lista.encontrar(l, v) — índice de primera ocurrencia (−1 si no)
 *    lista.indice es un alias
 * ---------------------------------------------------------------------- */
LatValor lat_lista_encontrar(LatValor lv, LatValor v) {
    LatLista *l = lista_de_valor(lv);
    if (!l) return lat_numero(-1.0);
    for (size_t i = 0; i < l->longitud; i++)
        if (valores_iguales(l->datos[i], v))
            return lat_numero((double)i);
    return lat_numero(-1.0);
}

LatValor lat_lista_indice(LatValor lv, LatValor v) {
    return lat_lista_encontrar(lv, v);
}

/* -------------------------------------------------------------------------
 * 9. lista.extender(l1, l2) — agrega todos los elementos de l2 a l1
 * ---------------------------------------------------------------------- */
LatValor lat_lista_extender(LatValor l1v, LatValor l2v) {
    LatLista *l1 = lista_de_valor(l1v);
    LatLista *l2 = lista_de_valor(l2v);
    if (!l1 || !l2) return lat_nulo();
    lista_crecer(l1, l2->longitud);
    memcpy(&l1->datos[l1->longitud], l2->datos, sizeof(LatValor) * l2->longitud);
    l1->longitud += l2->longitud;
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 10. lista.insertar(l, i, v) — inserta v en posición i
 * ---------------------------------------------------------------------- */
LatValor lat_lista_insertar(LatValor lv, LatValor iv, LatValor v) {
    LatLista *l = lista_de_valor(lv);
    if (!l) return lat_nulo();
    int idx = (int)(iv.tipo == LAT_NUMERO ? iv.como.numero : 0);
    if (idx < 0) idx = (int)l->longitud + idx;
    if (idx < 0) idx = 0;
    if ((size_t)idx > l->longitud) idx = (int)l->longitud;
    lista_crecer(l, 1);
    memmove(&l->datos[idx + 1], &l->datos[idx],
            sizeof(LatValor) * (l->longitud - (size_t)idx));
    l->datos[idx] = v;
    l->longitud++;
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 11. lista.invertir(l) — invierte el orden in-place
 * ---------------------------------------------------------------------- */
LatValor lat_lista_invertir(LatValor lv) {
    LatLista *l = lista_de_valor(lv);
    if (!l || l->longitud < 2) return lat_nulo();
    size_t left = 0, right = l->longitud - 1;
    while (left < right) {
        LatValor tmp    = l->datos[left];
        l->datos[left]  = l->datos[right];
        l->datos[right] = tmp;
        left++;
        right--;
    }
    return lat_nulo();
}

/* -------------------------------------------------------------------------
 * 12. lista.longitud(l) — número de elementos
 * ---------------------------------------------------------------------- */
LatValor lat_lista_longitud(LatValor lv) {
    LatLista *l = lista_de_valor(lv);
    return lat_numero(l ? (double)l->longitud : 0.0);
}

/* -------------------------------------------------------------------------
 * 13. lista.separador(l, sep) — une elementos como cadena con separador
 * ---------------------------------------------------------------------- */
LatValor lat_lista_separador(LatValor lv, LatValor sepv) {
    LatLista *l = lista_de_valor(lv);
    if (!l || l->longitud == 0) return lat_cadena("");

    char *sep_str = lat_a_cadena(sepv);
    size_t sep_len = strlen(sep_str);

    /* Primera pasada: calcular la longitud total. */
    size_t total = 0;
    char **partes = (char **)malloc(sizeof(char *) * l->longitud);
    if (!partes) { free(sep_str); return lat_nulo(); }
    for (size_t i = 0; i < l->longitud; i++) {
        partes[i] = lat_a_cadena(l->datos[i]);
        total += strlen(partes[i]);
        if (i + 1 < l->longitud) total += sep_len;
    }

    /* Segunda pasada: construir el resultado. */
    char *r = (char *)malloc(total + 1);
    if (!r) {
        for (size_t i = 0; i < l->longitud; i++) free(partes[i]);
        free(partes);
        free(sep_str);
        return lat_nulo();
    }
    char *p = r;
    for (size_t i = 0; i < l->longitud; i++) {
        size_t n = strlen(partes[i]);
        memcpy(p, partes[i], n);
        p += n;
        free(partes[i]);
        if (i + 1 < l->longitud) { memcpy(p, sep_str, sep_len); p += sep_len; }
    }
    *p = '\0';
    free(partes);
    free(sep_str);

    LatValor v = lat_cadena(r);
    free(r);
    return v;
}
