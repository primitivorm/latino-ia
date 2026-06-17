/* mate.c — implementación de la librería matemática de Latino. */

#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES   /* MSVC: expone M_PI, M_E en <math.h> */

#include "mate.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

/* Fallback por si el compilador no define las constantes. */
#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif
#ifndef M_E
#define M_E   2.71828182845904523536
#endif

/* -------------------------------------------------------------------------
 * Utilidades internas
 * ---------------------------------------------------------------------- */

static double a_numero(LatValor v) {
    return (v.tipo == LAT_NUMERO) ? v.como.numero : 0.0;
}

/* Crea una LatLista con exactamente dos elementos. */
static LatValor lista2(LatValor a, LatValor b) {
    LatLista *l = (LatLista *)malloc(sizeof(LatLista));
    if (!l) return lat_nulo();
    l->refs      = 1;
    l->capacidad = 2;
    l->longitud  = 2;
    l->datos = (LatValor *)malloc(sizeof(LatValor) * 2);
    if (!l->datos) { free(l); return lat_nulo(); }
    l->datos[0] = a;
    l->datos[1] = b;
    LatValor v;
    v.tipo = LAT_LISTA;
    v.como.lista = l;
    return v;
}

/* -------------------------------------------------------------------------
 * Constantes
 * ---------------------------------------------------------------------- */
LatValor lat_mate_pi(void)  { return lat_numero(M_PI); }
LatValor lat_mate_tau(void) { return lat_numero(2.0 * M_PI); }
LatValor lat_mate_e(void)   { return lat_numero(M_E); }

/* -------------------------------------------------------------------------
 * Trigonometría
 * ---------------------------------------------------------------------- */
LatValor lat_mate_sen(LatValor x)              { return lat_numero(sin(a_numero(x))); }
LatValor lat_mate_cos(LatValor x)              { return lat_numero(cos(a_numero(x))); }
LatValor lat_mate_tan(LatValor x)              { return lat_numero(tan(a_numero(x))); }
LatValor lat_mate_asen(LatValor x)             { return lat_numero(asin(a_numero(x))); }
LatValor lat_mate_acos(LatValor x)             { return lat_numero(acos(a_numero(x))); }
LatValor lat_mate_atan(LatValor x)             { return lat_numero(atan(a_numero(x))); }
LatValor lat_mate_atan2(LatValor y, LatValor x){ return lat_numero(atan2(a_numero(y), a_numero(x))); }

/* -------------------------------------------------------------------------
 * Hiperbólicas
 * ---------------------------------------------------------------------- */
LatValor lat_mate_senh(LatValor x)  { return lat_numero(sinh(a_numero(x))); }
LatValor lat_mate_cosh(LatValor x)  { return lat_numero(cosh(a_numero(x))); }
LatValor lat_mate_tanh(LatValor x)  { return lat_numero(tanh(a_numero(x))); }
LatValor lat_mate_asenh(LatValor x) { return lat_numero(asinh(a_numero(x))); }
LatValor lat_mate_acosh(LatValor x) { return lat_numero(acosh(a_numero(x))); }
LatValor lat_mate_atanh(LatValor x) { return lat_numero(atanh(a_numero(x))); }

/* -------------------------------------------------------------------------
 * Exponencial y logaritmo
 * ---------------------------------------------------------------------- */
LatValor lat_mate_exp(LatValor x)   { return lat_numero(exp(a_numero(x))); }
LatValor lat_mate_log(LatValor x)   { return lat_numero(log(a_numero(x))); }
LatValor lat_mate_log10(LatValor x) { return lat_numero(log10(a_numero(x))); }

/* -------------------------------------------------------------------------
 * Potencia y raíz
 * ---------------------------------------------------------------------- */
LatValor lat_mate_pot(LatValor base, LatValor expo) {
    return lat_numero(pow(a_numero(base), a_numero(expo)));
}
LatValor lat_mate_raiz(LatValor x)  { return lat_numero(sqrt(a_numero(x))); }
LatValor lat_mate_raizc(LatValor x) { return lat_numero(cbrt(a_numero(x))); }

/* -------------------------------------------------------------------------
 * Redondeo
 * ---------------------------------------------------------------------- */
LatValor lat_mate_piso(LatValor x)     { return lat_numero(floor(a_numero(x))); }
LatValor lat_mate_techo(LatValor x)    { return lat_numero(ceil(a_numero(x))); }
LatValor lat_mate_redondear(LatValor x){ return lat_numero(round(a_numero(x))); }
LatValor lat_mate_truncar(LatValor x)  { return lat_numero(trunc(a_numero(x))); }

/* -------------------------------------------------------------------------
 * Utilidades
 * ---------------------------------------------------------------------- */

LatValor lat_mate_abs(LatValor x) {
    return lat_numero(fabs(a_numero(x)));
}

LatValor lat_mate_max(LatValor av, LatValor bv) {
    double a = a_numero(av), b = a_numero(bv);
    return lat_numero(a > b ? a : b);
}

LatValor lat_mate_min(LatValor av, LatValor bv) {
    double a = a_numero(av), b = a_numero(bv);
    return lat_numero(a < b ? a : b);
}

LatValor lat_mate_aleatorio(void) {
    static int sembrado = 0;
    if (!sembrado) { srand((unsigned int)time(NULL)); sembrado = 1; }
    return lat_numero((double)rand() / ((double)RAND_MAX + 1.0));
}

LatValor lat_mate_alt(void) { return lat_mate_aleatorio(); }

/* frexp(x) → [mantisa, exponente]  */
LatValor lat_mate_frexp(LatValor xv) {
    int exp_val = 0;
    double m = frexp(a_numero(xv), &exp_val);
    return lista2(lat_numero(m), lat_numero((double)exp_val));
}

/* ldexp(mantisa, exponente) → mantisa * 2^exponente */
LatValor lat_mate_ldexp(LatValor mv, LatValor ev) {
    return lat_numero(ldexp(a_numero(mv), (int)a_numero(ev)));
}

/* base(x, b) → representación de x en base b (cadena) */
LatValor lat_mate_base(LatValor xv, LatValor bv) {
    long x = (long)a_numero(xv);
    int  b = (int) a_numero(bv);
    if (b < 2 || b > 36) return lat_cadena("0");
    if (x == 0) return lat_cadena("0");

    char buf[66];
    int neg = (x < 0);
    unsigned long ux = neg ? (unsigned long)(-x) : (unsigned long)x;
    int i = 65;
    buf[i] = '\0';
    while (ux > 0) {
        buf[--i] = "0123456789abcdefghijklmnopqrstuvwxyz"[ux % (unsigned long)b];
        ux /= (unsigned long)b;
    }
    if (neg) buf[--i] = '-';
    return lat_cadena(&buf[i]);
}

/* parte(x) → [parte_entera, parte_fraccionaria] */
LatValor lat_mate_parte(LatValor xv) {
    double entero = 0.0;
    double frac = modf(a_numero(xv), &entero);
    return lista2(lat_numero(entero), lat_numero(frac));
}

/* porc(v, total) → (v / total) * 100 */
LatValor lat_mate_porc(LatValor vv, LatValor totalv) {
    double total = a_numero(totalv);
    if (total == 0.0) return lat_numero(0.0);
    return lat_numero((a_numero(vv) / total) * 100.0);
}
