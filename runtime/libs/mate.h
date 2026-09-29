/* mate.h — librería matemática estándar para programas Latino transpilados.
 *
 * Uso en Latino: mate.sen(x), mate.raiz(x), etc.
 * El compilador mapea  mate.foo(args)  →  lat_mate_foo(args)  en C.
 *
 * Todas las funciones de un argumento numérico reciben y devuelven LatValor.
 * Las funciones que devuelven pares (frexp, parte) retornan una LatLista de 2.
 */

#ifndef LATINO_MATE_H
#define LATINO_MATE_H

#include "latino.h"

/* --- Constantes (sin argumentos) --- */
LatValor lat_mate_pi(void);
LatValor lat_mate_tau(void);
LatValor lat_mate_e(void);

/* --- Trigonometría --- */
LatValor lat_mate_sen(LatValor x);
LatValor lat_mate_cos(LatValor x);
LatValor lat_mate_tan(LatValor x);
LatValor lat_mate_asen(LatValor x);
LatValor lat_mate_acos(LatValor x);
LatValor lat_mate_atan(LatValor x);
LatValor lat_mate_atan2(LatValor y, LatValor x);

/* --- Hiperbólicas --- */
LatValor lat_mate_senh(LatValor x);
LatValor lat_mate_cosh(LatValor x);
LatValor lat_mate_tanh(LatValor x);
LatValor lat_mate_asenh(LatValor x);
LatValor lat_mate_acosh(LatValor x);
LatValor lat_mate_atanh(LatValor x);

/* --- Exponencial y logaritmo --- */
LatValor lat_mate_exp(LatValor x);
LatValor lat_mate_log(LatValor x);
LatValor lat_mate_log10(LatValor x);

/* --- Potencia y raíz --- */
LatValor lat_mate_pot(LatValor base, LatValor exp);
LatValor lat_mate_raiz(LatValor x);
LatValor lat_mate_raizc(LatValor x);

/* --- Redondeo --- */
LatValor lat_mate_piso(LatValor x);
LatValor lat_mate_techo(LatValor x);
LatValor lat_mate_redondear(LatValor x);
LatValor lat_mate_truncar(LatValor x);

/* --- Utilidades --- */
LatValor lat_mate_abs(LatValor x);
LatValor lat_mate_max(LatValor a, LatValor b);
LatValor lat_mate_min(LatValor a, LatValor b);
LatValor lat_mate_aleatorio(void);
LatValor lat_mate_alt(void);           /* alias de aleatorio */
LatValor lat_mate_frexp(LatValor x);   /* [mantisa, exponente] */
LatValor lat_mate_ldexp(LatValor m, LatValor e);
LatValor lat_mate_base(LatValor x, LatValor b);
LatValor lat_mate_parte(LatValor x);   /* [parte_entera, parte_fraccionaria] */
LatValor lat_mate_porc(LatValor v, LatValor total);

/* --- Fase 25 — Combinatoria y teoría de números --- */
LatValor lat_mate_factorial(LatValor n);
LatValor lat_mate_mcd(LatValor a, LatValor b);
LatValor lat_mate_mcm(LatValor a, LatValor b);
LatValor lat_mate_es_primo(LatValor n);
LatValor lat_mate_fibonacci(LatValor n);

/* --- Paridad con math de Python --- */

/* Constantes adicionales */
LatValor lat_mate_infinito(void);
LatValor lat_mate_nan(void);

/* Aritmética y comparación de flotantes */
LatValor lat_mate_copiar_signo(LatValor a, LatValor b);   /* copysign */
LatValor lat_mate_residuo(LatValor a, LatValor b);        /* fmod */
LatValor lat_mate_suma_precisa(LatValor lista);           /* fsum */
LatValor lat_mate_son_cercanos(LatValor a, LatValor b);   /* isclose */
LatValor lat_mate_es_finito(LatValor x);                  /* isfinite */
LatValor lat_mate_es_infinito(LatValor x);                /* isinf */
LatValor lat_mate_es_nan(LatValor x);                     /* isnan */
LatValor lat_mate_raiz_entera(LatValor x);                /* isqrt */
LatValor lat_mate_permutaciones(LatValor n, LatValor k);  /* perm */
LatValor lat_mate_combinaciones(LatValor n, LatValor k);  /* comb */
LatValor lat_mate_siguiente(LatValor x, LatValor y);      /* nextafter */
LatValor lat_mate_ulp(LatValor x);                        /* ulp */

/* Exponencial y logaritmo */
LatValor lat_mate_expm1(LatValor x);
LatValor lat_mate_log_base(LatValor x, LatValor base);    /* log(x, base) */
LatValor lat_mate_log2(LatValor x);
LatValor lat_mate_log1p(LatValor x);

/* Trigonometría y conversión de ángulos */
LatValor lat_mate_radianes(LatValor grados);
LatValor lat_mate_grados(LatValor radianes);
LatValor lat_mate_hipotenusa(LatValor a, LatValor b);     /* hypot */
LatValor lat_mate_distancia(LatValor p, LatValor q);      /* dist */

/* Funciones especiales */
LatValor lat_mate_erf(LatValor x);
LatValor lat_mate_erfc(LatValor x);
LatValor lat_mate_gamma(LatValor x);
LatValor lat_mate_lgamma(LatValor x);

#endif /* LATINO_MATE_H */
