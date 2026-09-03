// test_harness.h — Infraestructura compartida para suites de prueba E2E (Fase 20).
//
// Uso:
//   #include "test_harness.h"
//   static const harness::CasoTest CASOS[] = { ... };
//   int main(int argc, char* argv[]) {
//       return harness::ejecutar_main(argc, argv, CASOS, std::size(CASOS));
//   }
//
// El ejecutable resultante se invoca con:
//   <suite> <ruta_compilador> <directorio_runtime> <directorio_temporal>

#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef popen
#    define popen  _popen
#    define pclose _pclose
#  endif
#else
#  include <sys/wait.h>
#endif

namespace harness {

// ---------------------------------------------------------------------------
// Utilidades internas
// ---------------------------------------------------------------------------

static std::string q(const std::string& s) {
    return s.find(' ') != std::string::npos ? "\"" + s + "\"" : s;
}

static int capturar_salida(const std::string& cmd, std::string& out) {
    out.clear();
    char buf[512];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    while (fgets(buf, sizeof(buf), pipe))
        out += buf;
    int st = pclose(pipe);
#ifdef _WIN32
    return st;
#else
    return WIFEXITED(st) ? WEXITSTATUS(st) : st;
#endif
}

static std::vector<std::string> tokenizar(const std::string& s) {
    std::vector<std::string> v;
    std::istringstream ss(s);
    std::string t;
    while (ss >> t) v.push_back(t);
    return v;
}

// Quita secuencias de escape ANSI CSI (ESC '[' ... letra), como las que
// emiten funciones de control de cursor/color, antes de comparar la salida.
// En una terminal real son invisibles; al capturarlas por tubería quedan
// como bytes literales que romperían la comparación por tokens.
static std::string limpiarAnsi(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            size_t j = i + 2;
            while (j < s.size() && !std::isalpha(static_cast<unsigned char>(s[j])))
                ++j;
            i = j;  // salta también la letra final de la secuencia
            continue;
        }
        r += s[i];
    }
    return r;
}

// ---------------------------------------------------------------------------
// Caso de prueba
// ---------------------------------------------------------------------------

struct CasoTest {
    const char* nombre;
    const char* codigo_lat;
    const char* esperado;   // tokens esperados separados por espacios
};

// ---------------------------------------------------------------------------
// Harness principal
// ---------------------------------------------------------------------------

class Harness {
    std::string comp_, rt_, tmp_, backend_;
    int pasaron_ = 0, fallaron_ = 0;

public:
    // 'backend' (Fase L11 de input/PLAN_LLVM.md): "c" (default) o "llvm" --
    // el mismo arnés de las suites test_lib_*/test_poo_e2e/
    // test_funciones_base/test_incluir se reutiliza contra --backend=llvm,
    // registrado por tests/CMakeLists.txt como pruebas "<suite>_llvm"
    // adicionales cuando LATINO_LLVM_BACKEND está habilitado.
    Harness(const char* comp, const char* rt, const char* tmp, const char* backend = "c")
        : comp_(comp), rt_(rt), tmp_(tmp), backend_(backend) {}

    bool ejecutar(const CasoTest& tc) {
        std::string nombre = tc.nombre;
        std::string lat_path = tmp_ + "/" + nombre + ".lat";
        std::string exe_path = tmp_ + "/" + nombre;
#ifdef _WIN32
        exe_path += ".exe";
#endif
        // 1. Escribir código fuente .lat
        {
            std::ofstream f(lat_path);
            if (!f) {
                std::cerr << "[FALLO] " << nombre << ": no se pudo crear " << lat_path << "\n";
                fallaron_++;
                return false;
            }
            f << tc.codigo_lat;
        }

        // 2. Compilar con latino
        std::string cmd_comp = q(comp_) + " " + q(lat_path)
                             + " -o " + q(exe_path)
                             + " --runtime " + q(rt_)
                             + " --backend " + backend_ + " 2>&1";
        std::string out_comp;
        int rc_comp = capturar_salida(cmd_comp, out_comp);
        if (rc_comp != 0) {
            std::cerr << "[FALLO] " << nombre << ": error de compilacion (rc=" << rc_comp << ")\n"
                      << out_comp << "\n";
            fallaron_++;
            return false;
        }

        // 3. Ejecutar el binario generado
        std::string out_exec;
        int rc_exec = capturar_salida(q(exe_path) + " 2>&1", out_exec);
        if (rc_exec != 0) {
            std::cerr << "[FALLO] " << nombre << ": error de ejecucion (rc=" << rc_exec << ")\n"
                      << out_exec << "\n";
            fallaron_++;
            return false;
        }

        // 4. Comparar tokens
        auto real     = tokenizar(limpiarAnsi(out_exec));
        auto esperado = tokenizar(std::string(tc.esperado));
        if (real == esperado) {
            std::cout << "[PASO] " << nombre << "\n";
            pasaron_++;
            return true;
        }

        std::cerr << "[FALLO] " << nombre << "\n"
                  << "  Esperado : " << tc.esperado << "\n"
                  << "  Real     : " << out_exec;
        fallaron_++;
        return false;
    }

    int resumen() const {
        std::cout << "\nResultado: " << pasaron_ << " pasaron, "
                  << fallaron_ << " fallaron.\n";
        return fallaron_ > 0 ? 1 : 0;
    }
};

// ---------------------------------------------------------------------------
// Punto de entrada estándar para todas las suites
// ---------------------------------------------------------------------------

static int ejecutar_main(int argc, char* argv[],
                         const CasoTest* casos, std::size_t n) {
    if (argc < 4) {
        std::cerr << "Uso: " << argv[0]
                  << " <compilador> <runtime_dir> <temp_dir> [backend c|llvm]\n";
        return 2;
    }
    const char* backend = (argc >= 5) ? argv[4] : "c";
    Harness h(argv[1], argv[2], argv[3], backend);
    for (std::size_t i = 0; i < n; i++)
        h.ejecutar(casos[i]);
    return h.resumen();
}

} // namespace harness
