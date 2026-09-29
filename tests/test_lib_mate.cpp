// test_lib_mate.cpp — Suite de prueba para la librería mate (Fase 20).
// Cubre las 35 funciones documentadas en Manual-Latino/docs/librerias/mate.rst
// Las funciones no determinísticas (aleatorio/alt) sólo se ejercitan sin verificar valor.

#include "test_harness.h"

#define INC "incluir \"mate\"\n"

static const harness::CasoTest CASOS[] = {

    // Constantes
    { "mat_pi",
      INC "escribir(mate.pi())",
      "3.14159265358979" },

    { "mat_tau",
      INC "escribir(mate.tau())",
      "6.28318530717959" },

    { "mat_e",
      INC "escribir(mate.e())",
      "2.71828182845905" },

    // Trigonometría
    { "mat_sen_cero",
      INC "escribir(mate.sen(0))",
      "0" },

    { "mat_cos_cero",
      INC "escribir(mate.cos(0))",
      "1" },

    { "mat_tan_cero",
      INC "escribir(mate.tan(0))",
      "0" },

    { "mat_asen",
      INC "escribir(mate.asen(0))",
      "0" },

    { "mat_acos",
      INC "escribir(mate.acos(1))",
      "0" },

    { "mat_atan",
      INC "escribir(mate.atan(0))",
      "0" },

    { "mat_atan2",
      INC "escribir(mate.atan2(0, 1))",
      "0" },

    // Hiperbólicas
    { "mat_senh_cero",
      INC "escribir(mate.senh(0))",
      "0" },

    { "mat_cosh_cero",
      INC "escribir(mate.cosh(0))",
      "1" },

    { "mat_tanh_cero",
      INC "escribir(mate.tanh(0))",
      "0" },

    { "mat_asenh_cero",
      INC "escribir(mate.asenh(0))",
      "0" },

    { "mat_acosh_uno",
      INC "escribir(mate.acosh(1))",
      "0" },

    { "mat_atanh_cero",
      INC "escribir(mate.atanh(0))",
      "0" },

    // Exponencial y logaritmo
    { "mat_exp_cero",
      INC "escribir(mate.exp(0))",
      "1" },

    { "mat_log_e",
      INC "escribir(mate.log(mate.e()))",
      "1" },

    { "mat_log10_cien",
      INC "escribir(mate.log10(100))",
      "2" },

    // Potencia y raíz
    { "mat_pot_cuadrado",
      INC "escribir(mate.pot(2, 10))",
      "1024" },

    { "mat_pot_identidad",
      INC "escribir(mate.pot(5, 0))",
      "1" },

    { "mat_raiz",
      INC "escribir(mate.raiz(16))",
      "4" },

    { "mat_raizc",
      INC "escribir(mate.raizc(27))",
      "3" },

    // Redondeo
    { "mat_piso",
      INC "escribir(mate.piso(3.7))",
      "3" },

    { "mat_piso_negativo",
      INC "escribir(mate.piso(-3.2))",
      "-4" },

    { "mat_techo",
      INC "escribir(mate.techo(3.2))",
      "4" },

    { "mat_techo_entero",
      INC "escribir(mate.techo(3.0))",
      "3" },

    { "mat_redondear_arriba",
      INC "escribir(mate.redondear(3.5))",
      "4" },

    { "mat_redondear_abajo",
      INC "escribir(mate.redondear(3.4))",
      "3" },

    { "mat_truncar",
      INC "escribir(mate.truncar(3.9))",
      "3" },

    { "mat_truncar_negativo",
      INC "escribir(mate.truncar(-3.9))",
      "-3" },

    // Utilidades
    { "mat_abs_positivo",
      INC "escribir(mate.abs(-5))",
      "5" },

    { "mat_abs_cero",
      INC "escribir(mate.abs(0))",
      "0" },

    { "mat_max",
      INC "escribir(mate.max(3, 7))",
      "7" },

    { "mat_min",
      INC "escribir(mate.min(3, 7))",
      "3" },

    { "mat_porc",
      INC "escribir(mate.porc(25, 200))",
      "12.5" },

    { "mat_base_hex",
      INC "escribir(mate.base(255, 16))",
      "ff" },

    { "mat_base_binario",
      INC "escribir(mate.base(10, 2))",
      "1010" },

    { "mat_frexp_mantisa",
      INC "incluir \"lista\"\n"
          "r = mate.frexp(8)\n"
          "escribir(r[0])",
      "0.5" },

    { "mat_frexp_exponente",
      INC "incluir \"lista\"\n"
          "r = mate.frexp(8)\n"
          "escribir(r[1])",
      "4" },

    { "mat_ldexp",
      INC "escribir(mate.ldexp(0.5, 4))",
      "8" },

    { "mat_parte_entera",
      INC "incluir \"lista\"\n"
          "r = mate.parte(3.75)\n"
          "escribir(r[0])",
      "3" },

    { "mat_parte_fraccionaria",
      INC "incluir \"lista\"\n"
          "r = mate.parte(3.75)\n"
          "escribir(r[1])",
      "0.75" },

    // aleatorio / alt: ejercitar sin verificar valor (no determinístico)
    { "mat_aleatorio_rango",
      INC "r = mate.aleatorio()\n"
          "escribir(r >= 0 && r < 1)",
      "cierto" },

    { "mat_alt_rango",
      INC "r = mate.alt()\n"
          "escribir(r >= 0 && r < 1)",
      "cierto" },

    // ---- Fase 25 ----

    // mate.factorial
    { "mat_factorial_cero",
      INC "escribir(mate.factorial(0))",
      "1" },

    { "mat_factorial_cinco",
      INC "escribir(mate.factorial(5))",
      "120" },

    // mate.mcd
    { "mat_mcd",
      INC "escribir(mate.mcd(12, 8))",
      "4" },

    { "mat_mcd_primos_coprimos",
      INC "escribir(mate.mcd(7, 11))",
      "1" },

    // mate.mcm
    { "mat_mcm",
      INC "escribir(mate.mcm(4, 6))",
      "12" },

    // mate.es_primo
    { "mat_es_primo_si",
      INC "escribir(mate.es_primo(17))",
      "cierto" },

    { "mat_es_primo_no",
      INC "escribir(mate.es_primo(4))",
      "falso" },

    // mate.fibonacci
    { "mat_fibonacci_cero",
      INC "escribir(mate.fibonacci(0))",
      "0" },

    { "mat_fibonacci_diez",
      INC "escribir(mate.fibonacci(10))",
      "55" },

    // =========================================================================
    // Paridad con math de Python
    // =========================================================================

    // mate.infinito / mate.nan
    { "mat_infinito",
      INC "escribir(mate.infinito())",
      "inf" },

    { "mat_nan",
      INC "escribir(mate.nan())",
      "nan" },

    // mate.copiar_signo
    { "mat_copiar_signo",
      INC "escribir(mate.copiar_signo(5, -2))",
      "-5" },

    // mate.residuo
    { "mat_residuo",
      INC "escribir(mate.residuo(20, 3))",
      "2" },

    // mate.suma_precisa
    { "mat_suma_precisa",
      INC "escribir(mate.suma_precisa([1, 2, 3, 4]))",
      "10" },

    // mate.son_cercanos
    { "mat_son_cercanos_si",
      INC "escribir(mate.son_cercanos(1.0, 1.0))",
      "cierto" },

    { "mat_son_cercanos_no",
      INC "escribir(mate.son_cercanos(1.000001, 1.0))",
      "falso" },

    // mate.es_finito / mate.es_infinito / mate.es_nan
    { "mat_es_finito",
      INC "escribir(mate.es_finito(100))",
      "cierto" },

    { "mat_es_infinito",
      INC "escribir(mate.es_infinito(mate.infinito()))",
      "cierto" },

    { "mat_es_nan",
      INC "escribir(mate.es_nan(mate.nan()))",
      "cierto" },

    // mate.raiz_entera
    { "mat_raiz_entera",
      INC "escribir(mate.raiz_entera(15))",
      "3" },

    // mate.permutaciones / mate.combinaciones
    { "mat_permutaciones",
      INC "escribir(mate.permutaciones(5, 3))",
      "60" },

    { "mat_combinaciones",
      INC "escribir(mate.combinaciones(5, 3))",
      "10" },

    // mate.siguiente
    { "mat_siguiente",
      INC "escribir(mate.siguiente(1.0, 1.0))",
      "1" },

    // mate.ulp (solo se ejercita: el valor exacto depende del formato de
    // %.15g del runtime, ver CLAUDE.md)
    { "mat_ulp_positivo",
      INC "escribir(mate.ulp(1.0) > 0)",
      "cierto" },

    // mate.expm1
    { "mat_expm1",
      INC "escribir(mate.expm1(0))",
      "0" },

    // mate.log_base
    { "mat_log_base",
      INC "escribir(mate.log_base(100, 10))",
      "2" },

    // mate.log2
    { "mat_log2",
      INC "escribir(mate.log2(8))",
      "3" },

    // mate.log1p
    { "mat_log1p",
      INC "escribir(mate.log1p(0))",
      "0" },

    // mate.radianes / mate.grados
    { "mat_radianes",
      INC "escribir(mate.radianes(180))",
      "3.14159265358979" },

    { "mat_grados",
      INC "escribir(mate.grados(mate.pi()))",
      "180" },

    // mate.hipotenusa
    { "mat_hipotenusa",
      INC "escribir(mate.hipotenusa(3, 4))",
      "5" },

    // mate.distancia
    { "mat_distancia",
      INC "escribir(mate.distancia([1, 2], [4, 6]))",
      "5" },

    // mate.erf / mate.erfc
    { "mat_erf_cero",
      INC "escribir(mate.erf(0))",
      "0" },

    { "mat_erfc_cero",
      INC "escribir(mate.erfc(0))",
      "1" },

    // mate.gamma / mate.lgamma
    { "mat_gamma",
      INC "escribir(mate.gamma(5))",
      "24" },

    { "mat_lgamma",
      INC "escribir(mate.lgamma(1))",
      "0" },
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
