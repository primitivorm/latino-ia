#include <fstream>
#include <iostream>
#include <string>

#include "test_harness.h"

static bool comprobarCaso(const std::string& compilador, const std::string& runtime,
                          const std::string& temporal, const std::string& backend,
                          const std::string& nombre, const std::string& codigoLat,
                          const std::string& argumentos, const std::string& esperada) {
    const std::string archivoLat = temporal + "/" + nombre + "_" + backend + ".lat";
    std::string ejecutable = temporal + "/" + nombre + "_" + backend;
#ifdef _WIN32
    ejecutable += ".exe";
#endif

    {
        std::ofstream archivo(archivoLat);
        if (!archivo) {
            std::cerr << "No se pudo crear " << archivoLat << "\n";
            return false;
        }
        archivo << codigoLat;
    }

    const std::string comando = harness::q(compilador) + " " + harness::q(archivoLat) +
                                " -o " + harness::q(ejecutable) + " --runtime " +
                                harness::q(runtime) + " --backend " + backend +
                                argumentos;
    std::string salida;
    const int codigo = harness::capturar_salida(comando, salida);
    if (codigo != 0 || salida != esperada) {
        std::cerr << "La ejecucion mediante latino no produjo el resultado esperado.\n"
                  << "Codigo: " << codigo << "\nEsperado:\n" << esperada
                  << "Obtenido:\n" << salida;
        return false;
    }

    std::cout << "[PASO] " << nombre << ", backend " << backend << "\n";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Uso: " << argv[0]
                  << " <compilador> <runtime_dir> <directorio_temporal> <backend>\n";
        return 2;
    }

    const std::string compilador = argv[1];
    const std::string runtime = argv[2];
    const std::string temporal = argv[3];
    const std::string backend = argv[4];
    const bool principalOk = comprobarCaso(
        compilador, runtime, temporal, backend, "principal_args",
        "funcion principal(argc, argv)\n"
        "    poner(\"recibiendo \" .. argc .. \" parametros:\")\n"
        "    poner(argv)\n"
        "fin\n",
        " parametros aqui",
        "recibiendo 2 parametros:\n[\"parametros\", \"aqui\"]\n");
    const bool inicioOk = comprobarCaso(
        compilador, runtime, temporal, backend, "inicio_alias",
        "funcion inicio()\n"
        "    poner(\"inicio\")\n"
        "fin\n",
        "",
        "inicio\n");
    if (!principalOk || !inicioOk) return 1;
    return 0;
}
