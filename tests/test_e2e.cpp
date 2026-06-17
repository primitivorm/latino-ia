// test_e2e.cpp
//
// Driver de pruebas extremo a extremo (E2E) portátil.
// Ejecuta el compilador Latino, luego el binario generado, y compara la salida
// real con la esperada (extraída de los comentarios #salida: del archivo .lat).

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

// Entrecomilla una ruta/parámetro si contiene espacios.
static std::string entrecomillar(const std::string& s) {
    if (s.find(' ') != std::string::npos) {
        return "\"" + s + "\"";
    }
    return s;
}

// Ejecuta un comando del sistema y captura su salida estándar y código de retorno.
static int ejecutarComando(const std::string& cmd, std::string& salidaStd) {
    char buffer[256];
    salidaStd.clear();
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return -1;
    }
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        salidaStd += buffer;
    }
    int status = pclose(pipe);
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return status;
#endif
}

// Extrae todos los tokens esperados de un archivo .lat.
static std::vector<std::string> extraerTokensEsperados(const std::string& rutaLat) {
    std::ifstream archivo(rutaLat);
    std::vector<std::string> tokens;
    if (!archivo) return tokens;

    std::string linea;
    bool esperandoLineasSalida = false;

    while (std::getline(archivo, linea)) {
        while (!linea.empty() && (linea.back() == '\r' || linea.back() == '\n')) {
            linea.pop_back();
        }

        size_t posSalida = linea.find("#salida:");
        if (posSalida != std::string::npos) {
            std::string resto = linea.substr(posSalida + 8); // Longitud de "#salida:"
            size_t start = resto.find_first_not_of(" \t");
            if (start != std::string::npos) {
                size_t end = resto.find_last_not_of(" \t");
                std::string contenido = resto.substr(start, end - start + 1);
                std::stringstream ss(contenido);
                std::string tok;
                while (ss >> tok) {
                    tokens.push_back(tok);
                }
                esperandoLineasSalida = false;
            } else {
                esperandoLineasSalida = true;
            }
        } else if (esperandoLineasSalida) {
            size_t firstNonSpace = linea.find_first_not_of(" \t");
            if (firstNonSpace != std::string::npos && linea[firstNonSpace] == '#') {
                std::string resto = linea.substr(firstNonSpace + 1);
                std::stringstream ss(resto);
                std::string tok;
                while (ss >> tok) {
                    tokens.push_back(tok);
                }
            } else {
                esperandoLineasSalida = false;
            }
        }
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Uso: " << argv[0] << " <ruta_a_latino> <archivo.lat> <ruta_salida_exe> <directorio_runtime>\n";
        return 2;
    }

    std::string compilador = argv[1];
    std::string archivoLat = argv[2];
    std::string salidaExe = argv[3];
    std::string runtimeDir = argv[4];

    std::cout << "[E2E] Analizando: " << archivoLat << std::endl;

    // 1. Extraer los tokens de salida esperada.
    std::vector<std::string> esperado = extraerTokensEsperados(archivoLat);
    if (esperado.empty()) {
        std::cerr << "[E2E Error] No se encontraron comentarios '#salida:' en " << archivoLat << std::endl;
        return 1;
    }

    // 2. Compilar el archivo .lat usando latino.
    std::string cmdCompilacion = entrecomillar(compilador) + " " + entrecomillar(archivoLat) +
                                 " -o " + entrecomillar(salidaExe) +
                                 " --runtime " + entrecomillar(runtimeDir);

    std::cout << "[E2E] Compilando: " << cmdCompilacion << std::endl;
    std::string stdoutCompilacion;
    int codigoCompilacion = ejecutarComando(cmdCompilacion, stdoutCompilacion);
    if (codigoCompilacion != 0) {
        std::cerr << "[E2E Error] Fallo al compilar (codigo " << codigoCompilacion << ")\n";
        std::cerr << "Salida del compilador:\n" << stdoutCompilacion << std::endl;
        return 1;
    }

    // 3. Ejecutar el binario compilado.
    std::cout << "[E2E] Ejecutando: " << salidaExe << std::endl;
    std::string stdoutEjecutable;
    int codigoEjecutable = ejecutarComando(entrecomillar(salidaExe), stdoutEjecutable);
    if (codigoEjecutable != 0) {
        std::cerr << "[E2E Error] El ejecutable retorno codigo de error " << codigoEjecutable << "\n";
        std::cerr << "Salida del ejecutable:\n" << stdoutEjecutable << std::endl;
        return 1;
    }

    // 4. Tokenizar la salida real y comparar con lo esperado.
    std::vector<std::string> real;
    {
        std::stringstream ss(stdoutEjecutable);
        std::string tok;
        while (ss >> tok) {
            real.push_back(tok);
        }
    }

    bool coincide = true;
    if (real.size() != esperado.size()) {
        coincide = false;
    } else {
        for (size_t i = 0; i < real.size(); ++i) {
            if (real[i] != esperado[i]) {
                coincide = false;
                break;
            }
        }
    }

    if (!coincide) {
        std::cerr << "[E2E Error] Discrepancia en la salida para " << archivoLat << "\n";
        std::cerr << "ESPERADO (tokens):\n";
        for (const auto& tok : esperado) {
            std::cerr << "  " << tok << "\n";
        }
        std::cerr << "\nREAL (tokens):\n";
        for (const auto& tok : real) {
            std::cerr << "  " << tok << "\n";
        }
        std::cerr << "\nSalida cruda del ejecutable:\n" << stdoutEjecutable << std::endl;
        return 1;
    }

    std::cout << "[E2E] PASO CORRECTAMENTE: " << archivoLat << std::endl;
    return 0;
}
