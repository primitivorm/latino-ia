// test_ffi_e2e.cpp — PLAN_FFI.md (F7): programas .lat reales que declaran y
// llaman funciones "externo", compilados y ejecutados de punta a punta con
// el binario 'latino' real (ambos backends, ver tests/CMakeLists.txt).
//
// Decisión de diseño 6 del plan: ninguno de estos casos depende de una
// ".dll"/".so" externa al repo -- todos llaman símbolos que ya están
// enlazados por defecto en cualquier ejecutable Latino (msvcrt/ucrt en
// Windows, libc en Linux/macOS), sin "enlazar". El caso con "enlazar"
// ("externo enlazar \"user32\"" + MessageBoxA, ver F5) no se formaliza aquí
// a propósito: abre un diálogo real que bloquearía la ejecución automática
// -- su cobertura vive en tests/test_ffi.cpp (codegen del backend C,
// verificando el C generado sin compilar a ejecutable, mismo criterio que
// la verificación manual de F5).

#include "test_harness.h"

static const harness::CasoTest CASOS[] = {
    // Caso recomendado por el plan: sin "enlazar", llamando símbolos del
    // runtime C que todo ejecutable Latino ya enlaza (abs/strlen).
    {
        "ffi_abs_strlen",
        "externo\n"
        "    funcion abs(n: entero32): entero32\n"
        "    funcion strlen(s: cadena): entero64\n"
        "fin\n"
        "\n"
        "inseguro\n"
        "    escribir(abs(-5))\n"
        "    escribir(strlen(\"hola\"))\n"
        "fin\n",
        "5 4"
    },
    // Punteros opacos: malloc/free + comparacion contra "nulo" (== / !=,
    // nunca "es" -- ver Decision de diseño 4 y el hallazgo de F3).
    {
        "ffi_malloc_free_puntero",
        "externo\n"
        "    funcion malloc(tam: entero64): puntero\n"
        "    funcion free(p: puntero)\n"
        "fin\n"
        "\n"
        "inseguro\n"
        "    p = malloc(16)\n"
        "    si p == nulo\n"
        "        escribir(\"sin memoria\")\n"
        "    sino\n"
        "        escribir(\"con memoria\")\n"
        "    fin\n"
        "    free(p)\n"
        "    escribir(\"liberado\")\n"
        "fin\n",
        "con memoria liberado"
    },
    // "funcion inseguro nombre(...)" (la otra forma de habilitar 'inseguro',
    // sin bloque envolvente) + un argumento SIN anotacion de tipo estatica
    // -- ejercita la ruta dinamica de F4 (lat_ffi_verificar_tipo en runtime,
    // no un chequeo en compilacion) para un valor que solo se conoce en
    // tiempo de ejecucion.
    {
        "ffi_funcion_inseguro_chequeo_dinamico",
        "externo\n"
        "    funcion abs(n: entero32): entero32\n"
        "fin\n"
        "\n"
        "funcion inseguro calcular(n)\n"
        "    retornar abs(n)\n"
        "fin\n"
        "\n"
        "escribir(calcular(-7))\n",
        "7"
    },
    // Retorno "logico": un entero de C distinto de 0/1 debe normalizarse a
    // cierto/falso (lat_logico ya hace b ? 1 : 0 en el runtime).
    {
        "ffi_retorno_logico",
        "externo\n"
        "    funcion isalpha(c: entero32): logico\n"
        "fin\n"
        "\n"
        "inseguro\n"
        "    si isalpha(65)\n"
        "        escribir(\"es letra\")\n"
        "    sino\n"
        "        escribir(\"no es letra\")\n"
        "    fin\n"
        "fin\n",
        "es letra"
    },
};

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
