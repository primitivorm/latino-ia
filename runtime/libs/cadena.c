/* cadena.c — implementación de la librería de cadenas de Latino. */

#define _CRT_SECURE_NO_WARNINGS

#include "cadena.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Utilidades internas
 * ---------------------------------------------------------------------- */

static const char *valor_cadena(LatValor v) {
    return (v.tipo == LAT_CADENA && v.como.cadena) ? v.como.cadena : "";
}

static char *dup_str(const char *s) {
    size_t len = strlen(s);
    char *r = (char *)malloc(len + 1);
    if (r) memcpy(r, s, len + 1);
    return r;
}

/* -------------------------------------------------------------------------
 * Motor de expresiones regulares básico (sin POSIX, funciona en MSVC)
 *
 * Soporta: . * + ? ^ $ y clases de caracteres \d \w \s \D \W \S
 * (implementación de descenso recursivo estilo Rob Pike)
 * ---------------------------------------------------------------------- */

/* Devuelve cuántos caracteres de `text` consume la repetición de `pat`
 * seguida del resto `re`.  Retorna -1 si no hay coincidencia. */
static int regex_match_len(const char *re, const char *text);

static int regex_is_meta_char(char c, unsigned char t) {
    switch (c) {
    case 'd': return isdigit(t);
    case 'D': return !isdigit(t);
    case 'w': return (isalnum(t) || t == '_');
    case 'W': return !(isalnum(t) || t == '_');
    case 's': return isspace(t);
    case 'S': return !isspace(t);
    default:  return (t == (unsigned char)c);
    }
}

/* Coincide una clase de caracteres [...]  — re apunta al primer char después de '[' */
static int regex_match_class(const char *re, unsigned char c) {
    int negate = 0;
    if (*re == '^') { negate = 1; re++; }
    int found = 0;
    while (*re && *re != ']') {
        if (*re == '\\') {
            re++;
            if (*re) { found |= regex_is_meta_char(*re, c); re++; }
        } else if (re[1] == '-' && re[2] != ']' && re[2] != '\0') {
            /* rango: a-z */
            if (c >= (unsigned char)re[0] && c <= (unsigned char)re[2]) found = 1;
            re += 3;
        } else {
            if ((unsigned char)*re == c) found = 1;
            re++;
        }
    }
    return negate ? !found : found;
}

/* Coincide 1 carácter contra el átomo en *re: devuelve 1 si ok, 0 si no */
static int regex_match_one(const char *re, unsigned char c) {
    if (*re == '[') return regex_match_class(re + 1, c);
    if (*re == '\\') return regex_is_meta_char(re[1], c);
    if (*re == '.')  return (c != '\0');
    return ((unsigned char)*re == c);
}

/* Avance del puntero en el patrón por un "átomo" (puede ser \X, [clase] o un char) */
static const char *regex_next_atom(const char *re) {
    if (*re == '\\' && re[1] != '\0') return re + 2;
    if (*re == '[') {
        re++;  /* saltar '[' */
        if (*re == '^') re++;
        while (*re && *re != ']') {
            if (*re == '\\' && re[1] != '\0') re++;  /* escape dentro de clase */
            re++;
        }
        if (*re == ']') re++;
        return re;
    }
    return re + 1;
}

static int regex_match_star(const char *atom_re, const char *after_re,
                            const char *text, int plus) {
    /* Greedy: encontrar la extensión máxima */
    int max_rep = 0;
    while (text[max_rep] != '\0' &&
           regex_match_one(atom_re, (unsigned char)text[max_rep]))
        max_rep++;
    /* Si es '+' necesitamos al menos 1 */
    if (plus && max_rep == 0) return -1;
    for (int i = max_rep; i >= (plus ? 1 : 0); i--) {
        int r = regex_match_len(after_re, text + i);
        if (r >= 0) return i + r;
    }
    return -1;
}

static int regex_match_len(const char *re, const char *text) {
    if (*re == '\0') return 0;
    if (*re == '$')  return (*text == '\0') ? 0 : -1;

    /* Calcular tamaño del átomo actual */
    const char *atom_end = regex_next_atom(re);
    char quant = *atom_end; /* *, +, ? o ninguno */

    if (quant == '*' || quant == '+') {
        return regex_match_star(re, atom_end + 1, text, quant == '+');
    }
    if (quant == '?') {
        if (*text != '\0' && regex_match_one(re, (unsigned char)*text)) {
            int r = regex_match_len(atom_end + 1, text + 1);
            if (r >= 0) return r + 1;
        }
        return regex_match_len(atom_end + 1, text);
    }

    /* Sin cuantificador */
    if (*text != '\0' && regex_match_one(re, (unsigned char)*text)) {
        int r = regex_match_len(atom_end, text + 1);
        return (r >= 0) ? r + 1 : -1;
    }
    return -1;
}

/* Encuentra la primera ocurrencia del patrón `re` en `text`.
 * Rellena *start con la posición y *len con la longitud de la coincidencia.
 * Retorna 1 si encontró, 0 si no. */
static int regex_find(const char *re, const char *text, int *start, int *mlen) {
    const char *p = text;
    int anchored = (*re == '^');
    if (anchored) re++;
    do {
        int l = regex_match_len(re, p);
        if (l >= 0) {
            *start = (int)(p - text);
            *mlen  = l;
            return 1;
        }
        if (anchored) break;
    } while (*p++ != '\0');
    return 0;
}

/* -------------------------------------------------------------------------
 * Conversión / inspección
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_bytes(LatValor s) {
    const char *str = valor_cadena(s);
    size_t n = strlen(str);
    /* Construir lista en C dinámico: primero array temporal */
    LatValor *arr = (LatValor *)malloc(n * sizeof(LatValor));
    if (!arr) return lat_nulo();
    for (size_t i = 0; i < n; i++)
        arr[i] = lat_numero((double)(unsigned char)str[i]);
    LatLista *lista = (LatLista *)malloc(sizeof(LatLista));
    if (!lista) { free(arr); return lat_nulo(); }
    lista->refs     = 1;
    lista->datos    = arr;
    lista->longitud = n;
    lista->capacidad = n;
    LatValor r;
    r.tipo = LAT_LISTA;
    r.como.lista = lista;
    return r;
}

LatValor lat_cadena_char(LatValor n) {
    int code = (int)(n.tipo == LAT_NUMERO ? n.como.numero : 0);
    char buf[2] = { (char)code, '\0' };
    return lat_cadena(buf);
}

LatValor lat_cadena_longitud(LatValor s) {
    return lat_numero((double)strlen(valor_cadena(s)));
}

LatValor lat_cadena_esta_vacia(LatValor s) {
    return lat_logico(strlen(valor_cadena(s)) == 0);
}

LatValor lat_cadena_es_alfa(LatValor s) {
    const char *str = valor_cadena(s);
    if (*str == '\0') return lat_logico(0);
    for (; *str; str++)
        if (!isalpha((unsigned char)*str)) return lat_logico(0);
    return lat_logico(1);
}

LatValor lat_cadena_es_numerico(LatValor s) {
    const char *str = valor_cadena(s);
    if (*str == '\0') return lat_logico(0);
    char *end;
    strtod(str, &end);
    /* es numérico si se consumió toda la cadena (ignorando espacios al final) */
    while (*end && isspace((unsigned char)*end)) end++;
    return lat_logico(*end == '\0');
}

/* -------------------------------------------------------------------------
 * Búsqueda
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_contiene(LatValor s, LatValor sub) {
    return lat_logico(strstr(valor_cadena(s), valor_cadena(sub)) != NULL);
}

LatValor lat_cadena_encontrar(LatValor s, LatValor sub) {
    const char *haystack = valor_cadena(s);
    const char *needle   = valor_cadena(sub);
    const char *pos = strstr(haystack, needle);
    return lat_numero(pos ? (double)(pos - haystack) : -1.0);
}

LatValor lat_cadena_indice(LatValor s, LatValor sub) {
    return lat_cadena_encontrar(s, sub);
}

LatValor lat_cadena_ultimo_indice(LatValor s, LatValor sub) {
    const char *haystack = valor_cadena(s);
    const char *needle   = valor_cadena(sub);
    size_t nlen = strlen(needle);
    if (nlen == 0) return lat_numero((double)strlen(haystack));
    const char *last = NULL;
    const char *p    = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        last = p;
        p += nlen;
    }
    return lat_numero(last ? (double)(last - haystack) : -1.0);
}

LatValor lat_cadena_inicia_con(LatValor s, LatValor prefijo) {
    const char *str = valor_cadena(s);
    const char *pre = valor_cadena(prefijo);
    size_t plen = strlen(pre);
    return lat_logico(strncmp(str, pre, plen) == 0);
}

LatValor lat_cadena_termina_con(LatValor s, LatValor sufijo) {
    const char *str = valor_cadena(s);
    const char *suf = valor_cadena(sufijo);
    size_t slen = strlen(str);
    size_t suflen = strlen(suf);
    if (suflen > slen) return lat_logico(0);
    return lat_logico(strcmp(str + slen - suflen, suf) == 0);
}

/* -------------------------------------------------------------------------
 * Comparación
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_comparar(LatValor s1, LatValor s2) {
    int r = strcmp(valor_cadena(s1), valor_cadena(s2));
    return lat_numero(r < 0 ? -1.0 : r > 0 ? 1.0 : 0.0);
}

LatValor lat_cadena_es_igual(LatValor s1, LatValor s2) {
    const char *a = valor_cadena(s1);
    const char *b = valor_cadena(s2);
#ifdef _WIN32
    return lat_logico(_stricmp(a, b) == 0);
#else
    return lat_logico(strcasecmp(a, b) == 0);
#endif
}

/* -------------------------------------------------------------------------
 * Transformación
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_mayusculas(LatValor s) {
    const char *str = valor_cadena(s);
    size_t n = strlen(str);
    char *r = (char *)malloc(n + 1);
    if (!r) return lat_nulo();
    for (size_t i = 0; i <= n; i++)
        r[i] = (char)toupper((unsigned char)str[i]);
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_minusculas(LatValor s) {
    const char *str = valor_cadena(s);
    size_t n = strlen(str);
    char *r = (char *)malloc(n + 1);
    if (!r) return lat_nulo();
    for (size_t i = 0; i <= n; i++)
        r[i] = (char)tolower((unsigned char)str[i]);
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_invertir(LatValor s) {
    const char *str = valor_cadena(s);
    size_t n = strlen(str);
    char *r = (char *)malloc(n + 1);
    if (!r) return lat_nulo();
    for (size_t i = 0; i < n; i++) r[i] = str[n - 1 - i];
    r[n] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_recortar(LatValor s) {
    const char *str = valor_cadena(s);
    while (isspace((unsigned char)*str)) str++;
    size_t n = strlen(str);
    while (n > 0 && isspace((unsigned char)str[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (!r) return lat_nulo();
    memcpy(r, str, n);
    r[n] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_concatenar(LatValor s1, LatValor s2) {
    return lat_concatenar(s1, s2);
}

LatValor lat_cadena_reemplazar(LatValor sv, LatValor viejov, LatValor nuevov) {
    const char *str   = valor_cadena(sv);
    const char *viejo = valor_cadena(viejov);
    const char *nuevo = valor_cadena(nuevov);
    size_t vlen = strlen(viejo);
    if (vlen == 0) return lat_cadena(str);

    /* Contar ocurrencias para calcular el tamaño del buffer */
    size_t count = 0;
    const char *p = str;
    while ((p = strstr(p, viejo)) != NULL) { count++; p += vlen; }

    size_t slen  = strlen(str);
    size_t nlen  = strlen(nuevo);
    size_t rsize = slen + count * (nlen - vlen) + 1;
    if (count == 0) return lat_cadena(str);

    char *r = (char *)malloc(rsize);
    if (!r) return lat_nulo();
    char *out = r;
    p = str;
    while (*p) {
        if (strncmp(p, viejo, vlen) == 0) {
            memcpy(out, nuevo, nlen);
            out += nlen;
            p   += vlen;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_insertar(LatValor sv, LatValor posv, LatValor subv) {
    const char *str = valor_cadena(sv);
    const char *sub = valor_cadena(subv);
    int pos = (int)(posv.tipo == LAT_NUMERO ? posv.como.numero : 0);
    size_t slen = strlen(str);
    size_t sublen = strlen(sub);
    if (pos < 0) pos = 0;
    if ((size_t)pos > slen) pos = (int)slen;
    char *r = (char *)malloc(slen + sublen + 1);
    if (!r) return lat_nulo();
    memcpy(r, str, (size_t)pos);
    memcpy(r + pos, sub, sublen);
    memcpy(r + pos + sublen, str + pos, slen - (size_t)pos + 1);
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_subcadena(LatValor sv, LatValor iniciov, LatValor finv) {
    const char *str = valor_cadena(sv);
    size_t slen = strlen(str);
    int inicio = (int)(iniciov.tipo == LAT_NUMERO ? iniciov.como.numero : 0);
    int fin    = (int)(finv.tipo   == LAT_NUMERO ? finv.como.numero   : (double)slen);
    if (inicio < 0) inicio = 0;
    if (fin < 0 || (size_t)fin > slen) fin = (int)slen;
    if (inicio > fin) inicio = fin;
    size_t n = (size_t)(fin - inicio);
    char *r = (char *)malloc(n + 1);
    if (!r) return lat_nulo();
    memcpy(r, str + inicio, n);
    r[n] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_rellenar_derecha(LatValor sv, LatValor nv, LatValor cv) {
    const char *str = valor_cadena(sv);
    int n    = (int)(nv.tipo == LAT_NUMERO ? nv.como.numero : 0);
    const char *pad_str = valor_cadena(cv);
    char pad = (pad_str[0] != '\0') ? pad_str[0] : ' ';
    size_t slen = strlen(str);
    if (n < 0 || (size_t)n <= slen) return lat_cadena(str);
    char *r = (char *)malloc((size_t)n + 1);
    if (!r) return lat_nulo();
    memcpy(r, str, slen);
    for (size_t i = slen; i < (size_t)n; i++) r[i] = pad;
    r[n] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_rellenar_izquierda(LatValor sv, LatValor nv, LatValor cv) {
    const char *str = valor_cadena(sv);
    int n    = (int)(nv.tipo == LAT_NUMERO ? nv.como.numero : 0);
    const char *pad_str = valor_cadena(cv);
    char pad = (pad_str[0] != '\0') ? pad_str[0] : ' ';
    size_t slen = strlen(str);
    if (n < 0 || (size_t)n <= slen) return lat_cadena(str);
    size_t pad_count = (size_t)n - slen;
    char *r = (char *)malloc((size_t)n + 1);
    if (!r) return lat_nulo();
    for (size_t i = 0; i < pad_count; i++) r[i] = pad;
    memcpy(r + pad_count, str, slen + 1);
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

/* -------------------------------------------------------------------------
 * División
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_separar(LatValor sv, LatValor delimv) {
    const char *str   = valor_cadena(sv);
    const char *delim = valor_cadena(delimv);
    size_t dlen = strlen(delim);

    /* Contar segmentos */
    size_t count = 1;
    const char *p = str;
    if (dlen > 0) {
        while ((p = strstr(p, delim)) != NULL) { count++; p += dlen; }
    }

    /* Construir la lista */
    LatValor *arr = (LatValor *)malloc(count * sizeof(LatValor));
    if (!arr) return lat_nulo();
    size_t idx = 0;
    p = str;
    if (dlen == 0) {
        /* sin delimitador: cada carácter es un elemento */
        free(arr);
        size_t n = strlen(str);
        arr = (LatValor *)malloc(n * sizeof(LatValor));
        if (!arr) return lat_nulo();
        for (size_t i = 0; i < n; i++) {
            char buf[2] = { str[i], '\0' };
            arr[i] = lat_cadena(buf);
        }
        count = n;
    } else {
        const char *next;
        while ((next = strstr(p, delim)) != NULL) {
            size_t seg_len = (size_t)(next - p);
            char *seg = (char *)malloc(seg_len + 1);
            if (!seg) { free(arr); return lat_nulo(); }
            memcpy(seg, p, seg_len);
            seg[seg_len] = '\0';
            arr[idx++] = lat_cadena(seg);
            free(seg);
            p = next + dlen;
        }
        /* último segmento */
        arr[idx] = lat_cadena(p);
    }

    LatLista *lista = (LatLista *)malloc(sizeof(LatLista));
    if (!lista) { free(arr); return lat_nulo(); }
    lista->refs     = 1;
    lista->datos    = arr;
    lista->longitud = count;
    lista->capacidad = count;
    LatValor r;
    r.tipo = LAT_LISTA;
    r.como.lista = lista;
    return r;
}

/* -------------------------------------------------------------------------
 * Formato
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_formato(size_t n, ...) {
    if (n == 0) return lat_cadena("");
    va_list ap;
    va_start(ap, n);
    LatValor fmt_v = va_arg(ap, LatValor);
    const char *fmt = valor_cadena(fmt_v);

    /* Construir la cadena de formato estándar usando conversión de LatValor */
    char buf[4096];
    int pos = 0;
    size_t arg_idx = 1;

    const char *f = fmt;
    while (*f && pos < (int)sizeof(buf) - 1) {
        if (*f != '%') { buf[pos++] = *f++; continue; }
        f++; /* saltar '%' */
        if (*f == '%') { buf[pos++] = '%'; f++; continue; }

        /* Leer el especificador de formato */
        char spec[32] = "%";
        int si = 1;
        while (*f && !strchr("diouxXeEfFgGscp", *f) && si < 30)
            spec[si++] = *f++;
        if (*f) spec[si++] = *f++;
        spec[si] = '\0';

        if (arg_idx >= n) {
            /* sin más argumentos: emitir el especificador tal cual */
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s", spec);
            continue;
        }
        LatValor arg = va_arg(ap, LatValor);
        arg_idx++;

        char tmp[256];
        char *conv_spec = spec + si - 1; /* último carácter del spec */
        char last = *conv_spec;
        if (last == 's') {
            char *as = lat_a_cadena(arg);
            snprintf(tmp, sizeof(tmp), spec, as);
            free(as);
        } else if (last == 'd' || last == 'i' || last == 'o' ||
                   last == 'u' || last == 'x' || last == 'X') {
            *conv_spec = 'd'; /* tratar como entero */
            snprintf(tmp, sizeof(tmp), spec,
                     (int)(arg.tipo == LAT_NUMERO ? arg.como.numero : 0.0));
            *conv_spec = last;
        } else {
            /* f, g, e, etc. → double */
            snprintf(tmp, sizeof(tmp), spec,
                     arg.tipo == LAT_NUMERO ? arg.como.numero : 0.0);
        }
        int written = snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s", tmp);
        if (written > 0) pos += written;
    }
    buf[pos] = '\0';
    va_end(ap);
    return lat_cadena(buf);
}

/* -------------------------------------------------------------------------
 * Expresiones regulares
 * ---------------------------------------------------------------------- */

LatValor lat_cadena_regex(LatValor sv, LatValor patronv) {
    const char *str = valor_cadena(sv);
    const char *pat = valor_cadena(patronv);
    int start, mlen;
    if (!regex_find(pat, str, &start, &mlen)) return lat_nulo();
    char *r = (char *)malloc((size_t)mlen + 1);
    if (!r) return lat_nulo();
    memcpy(r, str + start, (size_t)mlen);
    r[mlen] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

LatValor lat_cadena_regexl(LatValor sv, LatValor patronv) {
    const char *str = valor_cadena(sv);
    const char *pat = valor_cadena(patronv);

    /* Acumular resultados en un array dinámico */
    size_t cap = 8, count = 0;
    LatValor *arr = (LatValor *)malloc(cap * sizeof(LatValor));
    if (!arr) return lat_nulo();

    const char *p = str;
    int start, mlen;
    while (regex_find(pat, p, &start, &mlen)) {
        if (count >= cap) {
            cap *= 2;
            LatValor *tmp = (LatValor *)realloc(arr, cap * sizeof(LatValor));
            if (!tmp) { free(arr); return lat_nulo(); }
            arr = tmp;
        }
        char *seg = (char *)malloc((size_t)mlen + 1);
        if (!seg) { free(arr); return lat_nulo(); }
        memcpy(seg, p + start, (size_t)mlen);
        seg[mlen] = '\0';
        arr[count++] = lat_cadena(seg);
        free(seg);
        p += start + (mlen > 0 ? mlen : 1); /* avanzar al menos 1 */
        if (*p == '\0') break;
    }

    LatLista *lista = (LatLista *)malloc(sizeof(LatLista));
    if (!lista) { free(arr); return lat_nulo(); }
    lista->refs     = 1;
    lista->datos    = arr;
    lista->longitud = count;
    lista->capacidad = cap;
    LatValor r;
    r.tipo = LAT_LISTA;
    r.como.lista = lista;
    return r;
}

/* =========================================================================
 * Fase 23 — Funciones adicionales de cadena
 * ====================================================================== */

/* -------------------------------------------------------------------------
 * cadena.contar(s, sub) — ocurrencias no solapadas de sub en s
 * ---------------------------------------------------------------------- */
LatValor lat_cadena_contar(LatValor sv, LatValor subv) {
    const char *s   = valor_cadena(sv);
    const char *sub = valor_cadena(subv);
    size_t sub_len  = strlen(sub);
    if (sub_len == 0) return lat_numero(0.0);

    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, sub)) != NULL) {
        count++;
        p += sub_len;
    }
    return lat_numero((double)count);
}

/* -------------------------------------------------------------------------
 * cadena.titulo(s) — primera letra de cada palabra en mayúscula
 * ---------------------------------------------------------------------- */
LatValor lat_cadena_titulo(LatValor sv) {
    const char *s = valor_cadena(sv);
    size_t len = strlen(s);
    char *r = (char *)malloc(len + 1);
    if (!r) return lat_nulo();
    int nueva_palabra = 1;
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)s[i] == ' ' || (unsigned char)s[i] == '\t' ||
            (unsigned char)s[i] == '\n' || (unsigned char)s[i] == '\r') {
            r[i] = s[i];
            nueva_palabra = 1;
        } else if (nueva_palabra) {
            r[i] = (char)toupper((unsigned char)s[i]);
            nueva_palabra = 0;
        } else {
            r[i] = (char)tolower((unsigned char)s[i]);
        }
    }
    r[len] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

/* -------------------------------------------------------------------------
 * cadena.capitalizar(s) — primera letra mayúscula, el resto minúsculas
 * ---------------------------------------------------------------------- */
LatValor lat_cadena_capitalizar(LatValor sv) {
    const char *s = valor_cadena(sv);
    size_t len = strlen(s);
    char *r = (char *)malloc(len + 1);
    if (!r) return lat_nulo();
    for (size_t i = 0; i < len; i++)
        r[i] = (i == 0) ? (char)toupper((unsigned char)s[i])
                        : (char)tolower((unsigned char)s[i]);
    r[len] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

/* -------------------------------------------------------------------------
 * cadena.recortar_izq(s) — elimina espacios solo por la izquierda
 * ---------------------------------------------------------------------- */
LatValor lat_cadena_recortar_izq(LatValor sv) {
    const char *s = valor_cadena(sv);
    while (*s && isspace((unsigned char)*s)) s++;
    return lat_cadena(s);
}

/* -------------------------------------------------------------------------
 * cadena.recortar_der(s) — elimina espacios solo por la derecha
 * ---------------------------------------------------------------------- */
LatValor lat_cadena_recortar_der(LatValor sv) {
    const char *s = valor_cadena(sv);
    size_t len = strlen(s);
    char *r = (char *)malloc(len + 1);
    if (!r) return lat_nulo();
    memcpy(r, s, len + 1);
    while (len > 0 && isspace((unsigned char)r[len - 1])) len--;
    r[len] = '\0';
    LatValor v = lat_cadena(r);
    free(r);
    return v;
}

/* -------------------------------------------------------------------------
 * cadena.es_espacio(s) — cierto si la cadena es toda espacios en blanco
 * ---------------------------------------------------------------------- */
LatValor lat_cadena_es_espacio(LatValor sv) {
    const char *s = valor_cadena(sv);
    if (*s == '\0') return lat_logico(0);
    while (*s) {
        if (!isspace((unsigned char)*s)) return lat_logico(0);
        s++;
    }
    return lat_logico(1);
}
