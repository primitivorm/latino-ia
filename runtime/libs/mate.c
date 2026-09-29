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

/* =========================================================================
 * Fase 25 — Combinatoria y teoría de números
 * ====================================================================== */

/* factorial(n) → n!  (0→1, n>170→inf por desbordamiento de double) */
LatValor lat_mate_factorial(LatValor nv) {
    long long n = (long long)a_numero(nv);
    if (n < 0) return lat_numero(0.0);
    double r = 1.0;
    for (long long i = 2; i <= n; i++) r *= (double)i;
    return lat_numero(r);
}

/* mcd(a, b) → máximo común divisor (algoritmo de Euclides) */
LatValor lat_mate_mcd(LatValor av, LatValor bv) {
    long long a = (long long)fabs(a_numero(av));
    long long b = (long long)fabs(a_numero(bv));
    while (b) { long long t = b; b = a % b; a = t; }
    return lat_numero((double)a);
}

/* mcm(a, b) → mínimo común múltiplo */
LatValor lat_mate_mcm(LatValor av, LatValor bv) {
    long long a = (long long)fabs(a_numero(av));
    long long b = (long long)fabs(a_numero(bv));
    if (a == 0 || b == 0) return lat_numero(0.0);
    long long g = a, h = b;
    while (h) { long long t = h; h = g % h; g = t; }
    return lat_numero((double)(a / g * b));
}

/* es_primo(n) → cierto si n es primo (prueba hasta √n) */
LatValor lat_mate_es_primo(LatValor nv) {
    long long n = (long long)a_numero(nv);
    if (n < 2) return lat_logico(0);
    if (n == 2) return lat_logico(1);
    if (n % 2 == 0) return lat_logico(0);
    for (long long i = 3; i * i <= n; i += 2)
        if (n % i == 0) return lat_logico(0);
    return lat_logico(1);
}

/* fibonacci(n) → n-ésimo número de Fibonacci (0→0, 1→1, iterativo) */
LatValor lat_mate_fibonacci(LatValor nv) {
    long long n = (long long)a_numero(nv);
    if (n <= 0) return lat_numero(0.0);
    if (n == 1) return lat_numero(1.0);
    double a = 0.0, b = 1.0;
    for (long long i = 2; i <= n; i++) { double t = a + b; a = b; b = t; }
    return lat_numero(b);
}

/* =========================================================================
 * Paridad con math de Python
 * ====================================================================== */

/* --- Constantes adicionales --- */
LatValor lat_mate_infinito(void) { return lat_numero(HUGE_VAL); }
LatValor lat_mate_nan(void)       { return lat_numero(NAN); }

/* --- Aritmética y comparación de flotantes --- */

LatValor lat_mate_copiar_signo(LatValor av, LatValor bv) {
    return lat_numero(copysign(a_numero(av), a_numero(bv)));
}

LatValor lat_mate_residuo(LatValor av, LatValor bv) {
    return lat_numero(fmod(a_numero(av), a_numero(bv)));
}

/* suma_precisa(lista) → suma de flotantes con compensación de error
 * (algoritmo de Neumaier, igual estrategia que math.fsum de Python). */
LatValor lat_mate_suma_precisa(LatValor listav) {
    if (listav.tipo != LAT_LISTA) return lat_numero(0.0);
    LatLista *l = listav.como.lista;
    double suma = 0.0, comp = 0.0;
    for (size_t i = 0; i < l->longitud; i++) {
        double x = a_numero(l->datos[i]);
        double t = suma + x;
        if (fabs(suma) >= fabs(x)) comp += (suma - t) + x;
        else                       comp += (x - t) + suma;
        suma = t;
    }
    return lat_numero(suma + comp);
}

/* son_cercanos(a, b) → igual a math.isclose(a, b) con los valores por
 * defecto de Python: rel_tol=1e-9, abs_tol=0.0 */
LatValor lat_mate_son_cercanos(LatValor av, LatValor bv) {
    double a = a_numero(av), b = a_numero(bv);
    double rel_tol = 1e-9, abs_tol = 0.0;
    double diff = fabs(a - b);
    return lat_logico(diff <= fabs(rel_tol * (fabs(a) > fabs(b) ? fabs(a) : fabs(b)))
                       || diff <= abs_tol);
}

LatValor lat_mate_es_finito(LatValor xv)   { return lat_logico(isfinite(a_numero(xv))); }
LatValor lat_mate_es_infinito(LatValor xv) { return lat_logico(isinf(a_numero(xv))); }
LatValor lat_mate_es_nan(LatValor xv)      { return lat_logico(isnan(a_numero(xv))); }

/* raiz_entera(x) → floor(sqrt(x)) exacto para enteros no negativos */
LatValor lat_mate_raiz_entera(LatValor xv) {
    long long x = (long long)a_numero(xv);
    if (x < 0) return lat_numero(0.0);
    long long r = (long long)sqrt((double)x);
    while (r * r > x) r--;
    while ((r + 1) * (r + 1) <= x) r++;
    return lat_numero((double)r);
}

/* permutaciones(n, k) → n! / (n - k)! */
LatValor lat_mate_permutaciones(LatValor nv, LatValor kv) {
    long long n = (long long)a_numero(nv);
    long long k = (long long)a_numero(kv);
    if (k < 0 || k > n) return lat_numero(0.0);
    double r = 1.0;
    for (long long i = 0; i < k; i++) r *= (double)(n - i);
    return lat_numero(r);
}

/* combinaciones(n, k) → n! / (k! * (n - k)!) */
LatValor lat_mate_combinaciones(LatValor nv, LatValor kv) {
    long long n = (long long)a_numero(nv);
    long long k = (long long)a_numero(kv);
    if (k < 0 || k > n) return lat_numero(0.0);
    if (k > n - k) k = n - k;
    double r = 1.0;
    for (long long i = 0; i < k; i++) r = r * (double)(n - i) / (double)(i + 1);
    return lat_numero(round(r));
}

LatValor lat_mate_siguiente(LatValor xv, LatValor yv) {
    return lat_numero(nextafter(a_numero(xv), a_numero(yv)));
}

/* ulp(x) → tamaño del último bit de precisión de x (Unit in the Last Place) */
LatValor lat_mate_ulp(LatValor xv) {
    double x = fabs(a_numero(xv));
    if (isnan(x)) return lat_numero(x);
    if (isinf(x)) return lat_numero(x);
    return lat_numero(nextafter(x, HUGE_VAL) - x);
}

/* --- Exponencial y logaritmo --- */

LatValor lat_mate_expm1(LatValor xv) { return lat_numero(expm1(a_numero(xv))); }

/* log_base(x, base) → logaritmo de x en la base indicada */
LatValor lat_mate_log_base(LatValor xv, LatValor basev) {
    return lat_numero(log(a_numero(xv)) / log(a_numero(basev)));
}

LatValor lat_mate_log2(LatValor xv)  { return lat_numero(log2(a_numero(xv))); }
LatValor lat_mate_log1p(LatValor xv) { return lat_numero(log1p(a_numero(xv))); }

/* --- Trigonometría y conversión de ángulos --- */

LatValor lat_mate_radianes(LatValor gradosv) {
    return lat_numero(a_numero(gradosv) * M_PI / 180.0);
}

LatValor lat_mate_grados(LatValor radianesv) {
    return lat_numero(a_numero(radianesv) * 180.0 / M_PI);
}

LatValor lat_mate_hipotenusa(LatValor av, LatValor bv) {
    return lat_numero(hypot(a_numero(av), a_numero(bv)));
}

/* distancia(p, q) → distancia euclidiana entre dos puntos (listas de igual
 * longitud), igual a math.dist de Python. */
LatValor lat_mate_distancia(LatValor pv, LatValor qv) {
    if (pv.tipo != LAT_LISTA || qv.tipo != LAT_LISTA) return lat_numero(0.0);
    LatLista *p = pv.como.lista, *q = qv.como.lista;
    size_t n = (p->longitud < q->longitud) ? p->longitud : q->longitud;
    double suma = 0.0;
    for (size_t i = 0; i < n; i++) {
        double d = a_numero(p->datos[i]) - a_numero(q->datos[i]);
        suma += d * d;
    }
    return lat_numero(sqrt(suma));
}

/* --- Funciones especiales --- */

LatValor lat_mate_erf(LatValor xv)    { return lat_numero(erf(a_numero(xv))); }
LatValor lat_mate_erfc(LatValor xv)   { return lat_numero(erfc(a_numero(xv))); }
LatValor lat_mate_gamma(LatValor xv)  { return lat_numero(tgamma(a_numero(xv))); }
LatValor lat_mate_lgamma(LatValor xv) { return lat_numero(lgamma(a_numero(xv))); }
