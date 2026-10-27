// Verifica errores de rango a través del compilador y runtime del backend elegido.

#include "test_harness.h"

struct CasoErrorRango {
    const char* nombre;
    const char* codigo;
    const char* error;
};

static const CasoErrorRango CASOS[] = {
    {"aridad_vacia", "rango()\n", "rango espera entre 1 y 3 argumentos"},
    {"aridad_exceso", "rango(1, 2, 3, 4)\n", "rango espera entre 1 y 3 argumentos"},
    {"no_numerico", "rango(\"1\")\n", "los argumentos de rango deben ser números enteros"},
    {"fraccionario", "rango(1.5)\n", "los argumentos de rango deben ser números enteros"},
    {"no_finito", "rango(10 ^ 1000)\n", "los argumentos de rango deben ser finitos"},
    {"paso_cero", "rango(0, 5, 0)\n", "el paso de rango no puede ser cero"},
    {"lista_tipo", "incluir \"lista\"\nlista.a_lista(3)\n", "lista.a_lista espera un valor rango"},
    {"materializacion_overflow",
     "incluir \"lista\"\nlista.a_lista(rango(0, 10 ^ 300, 1))\n",
     "el rango es demasiado grande para materializarlo como lista"},
};

static int fallos = 0;
static int comprobaciones = 0;

#define CHECK(cond, mensaje) do { \
    ++comprobaciones; \
    if (!(cond)) { ++fallos; std::cerr << "FALLO: " << mensaje << "\n"; } \
} while (0)

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Uso: test_rango_errores_e2e <compilador> <runtime> <temporal> <backend>\n";
        return 2;
    }

    const std::string compilador = argv[1];
    const std::string runtime = argv[2];
    const std::string temporal = argv[3];
    const std::string backend = argv[4];

    for (const CasoErrorRango& caso : CASOS) {
        const std::string base = temporal + "/" + caso.nombre;
        const std::string fuente = base + ".lat";
        std::string ejecutable = base;
#ifdef _WIN32
        ejecutable += ".exe";
#endif
        {
            std::ofstream archivo(fuente);
            if (!archivo) {
                CHECK(false, caso.nombre << ": no se pudo crear " << fuente);
                continue;
            }
            archivo << caso.codigo;
        }

        const std::string comandoCompilar =
            harness::q(compilador) + " " + harness::q(fuente) + " -o " +
            harness::q(ejecutable) + " --runtime " + harness::q(runtime) +
            " --backend " + backend + " 2>&1";
        std::string salidaCompilacion;
        int rcCompilacion = harness::capturar_salida(comandoCompilar, salidaCompilacion);
        CHECK(rcCompilacion == 0,
              caso.nombre << ": el programa de error debe compilar; salida: "
                          << salidaCompilacion);
        if (rcCompilacion != 0) continue;

        std::string salidaEjecucion;
        int rcEjecucion =
            harness::capturar_salida(harness::q(ejecutable) + " 2>&1", salidaEjecucion);
        CHECK(rcEjecucion != 0, caso.nombre << ": debe terminar con código distinto de cero");
        CHECK(salidaEjecucion.find(caso.error) != std::string::npos,
              caso.nombre << ": mensaje de error esperado '" << caso.error
                          << "', recibido: " << salidaEjecucion);
    }

    std::cout << "Comprobaciones: " << comprobaciones << "  Fallos: " << fallos << "\n";
    return fallos ? 1 : 0;
}
