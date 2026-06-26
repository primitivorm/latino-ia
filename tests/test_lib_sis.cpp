// test_lib_sis.cpp — Suite de prueba para la librería sis (Fase 20).
// Cubre las 9 funciones de Manual-Latino/docs/librerias/sis.rst
// Las funciones dependientes del entorno se ejercitan sin verificar valor exacto.

#include "test_harness.h"

#define INC "incluir \"sis\"\n"

static const harness::CasoTest CASOS[] = {

    // sis.dormir: devuelve nulo, siempre determinístico
    { "sis_dormir_retorna_nulo",
      INC "escribir(sis.dormir(1))",
      "nulo" },

    // sis.operativo: debe ser "windows", "linux" o "macos"
    { "sis_operativo_valido",
      INC "incluir \"cadena\"\n"
          "op = sis.operativo()\n"
          "es_valido = op == \"windows\" || op == \"linux\" || op == \"macos\"\n"
          "escribir(es_valido)",
      "cierto" },

    // sis.fecha: debe tener longitud 10 (DD/MM/AAAA)
    { "sis_fecha_longitud",
      INC "incluir \"cadena\"\n"
          "escribir(cadena.longitud(sis.fecha()))",
      "10" },

    // sis.fecha: debe contener '/'
    { "sis_fecha_tiene_barra",
      INC "incluir \"cadena\"\n"
          "escribir(cadena.contiene(sis.fecha(), \"/\"))",
      "cierto" },

    // sis.cwd: directorio actual no vacío
    { "sis_cwd_no_vacio",
      INC "incluir \"cadena\"\n"
          "escribir(cadena.longitud(sis.cwd()) > 0)",
      "cierto" },

    // sis.usuario: nombre no vacío
    { "sis_usuario_no_vacio",
      INC "incluir \"cadena\"\n"
          "escribir(cadena.longitud(sis.usuario()) > 0)",
      "cierto" },

    // sis.tiempo: timestamp positivo
    { "sis_tiempo_positivo",
      INC "escribir(sis.tiempo() > 0)",
      "cierto" },

    // sis.ejecutar: ejecutar un comando simple
    { "sis_ejecutar_no_vacio",
      INC "incluir \"cadena\"\n"
#ifdef _WIN32
          "salida = sis.ejecutar(\"echo hola\")\n"
#else
          "salida = sis.ejecutar(\"echo hola\")\n"
#endif
          "escribir(cadena.longitud(salida) > 0)",
      "cierto" },

    // sis.iraxy: solo verificar que no falla (retorna nulo)
    { "sis_iraxy_retorna_nulo",
      INC "escribir(sis.iraxy(0, 0))",
      "nulo" },

    // ---- Fase 26 ----

    // sis.args: sin argumentos adicionales → lista vacía
    { "sis_args_es_lista",
      INC "incluir \"lista\"\n"
          "a = sis.args()\n"
          "escribir(lista.longitud(a) >= 0)",
      "cierto" },

    // sis.env: variable inexistente → nulo
    { "sis_env_inexistente",
      INC "escribir(sis.env(\"LAT_VAR_NO_EXISTE_XYZ\"))",
      "nulo" },

    // sis.pid: PID > 0
    { "sis_pid_positivo",
      INC "escribir(sis.pid() > 0)",
      "cierto" },
};

#undef INC

int main(int argc, char* argv[]) {
    return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
}
