// test_lib_paquete.cpp — Fase 20 (cobertura completa).
//
// Hallazgo de la auditoría de input/PLAN_LIBS.md: "paquete" (carga dinámica
// de bibliotecas nativas, Fase 16) estaba implementada y mapeada por el
// compilador como cualquier otra librería, pero sin ninguna prueba
// funcional -- ejemplos/paquete_ejemplo.lat existe, pero a propósito NO
// llama paquete.cargar de verdad (no hay ninguna .dll/.so real en el repo
// para cargar, así que solo imprime un mensaje estático).
//
// Esta suite compila una biblioteca nativa de prueba real
// (tests/fixtures/paquete_fixture.c, target CMake test_paquete_fixture) y
// la carga/llama de punta a punta con el binario 'latino' real. No usa la
// macro add_suite20 (a diferencia del resto de las suites de esta Fase):
// necesita un argumento extra -- la ruta de la biblioteca de prueba ya
// compilada -- que un harness::CasoTest estático (una cadena literal en
// tiempo de compilación) no puede parametrizar, así que el código Latino de
// cada caso se arma en tiempo de ejecución como std::string.

#include <iostream>
#include <string>

#include "test_harness.h"

namespace {

// Escapa backslashes para que la ruta sea un literal de cadena Latino
// válido -- en Windows, LoadLibraryA recibe rutas con "\" como separador.
std::string escaparRutaLat(const std::string& ruta) {
    std::string r;
    for (char c : ruta) {
        if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Uso: " << argv[0]
                  << " <compilador> <runtime_dir> <tmp_dir> <ruta_fixture> [backend c|llvm]\n";
        return 2;
    }
    const char* backend = (argc >= 6) ? argv[5] : "c";
    harness::Harness h(argv[1], argv[2], argv[3], backend);
    const std::string rutaFixture = escaparRutaLat(argv[4]);

    // paquete.cargar(...) + despacho por-instancia: "milib.fn(args)" resuelve
    // a lat_obj_llamar_metodo, que despacha a lat_paquete_llamar_args cuando
    // el objeto es LAT_MODULO (ver runtime/latino.c).
    {
        std::string codigo =
            "incluir \"paquete\"\n"
            "milib = paquete.cargar(\"" + rutaFixture + "\")\n"
            "escribir(milib.sumar_dos(3, 4))\n";
        harness::CasoTest caso{"paq_cargar_y_llamar_por_instancia", codigo.c_str(), "7"};
        h.ejecutar(caso);
    }

    // paquete.llamar(modulo, "fn", nargs, args...) explícito (la forma
    // documentada en PLAN_FFI.md como contraste con "externo").
    {
        std::string codigo =
            "incluir \"paquete\"\n"
            "milib = paquete.cargar(\"" + rutaFixture + "\")\n"
            "escribir(paquete.llamar(milib, \"saludo_fijo\", 0))\n";
        harness::CasoTest caso{"paq_llamar_explicito", codigo.c_str(),
                               "hola desde paquete_fixture"};
        h.ejecutar(caso);
    }

    // Ruta inexistente: lat_paquete_cargar no debe crashear, debe devolver
    // "nulo" -- el harness mezcla stdout/stderr (2>&1), así que el
    // diagnóstico que lat_paquete_cargar imprime por stderr queda como
    // parte de la salida esperada, junto al "nulo" real de escribir(milib).
    {
        std::string codigo =
            "incluir \"paquete\"\n"
            "milib = paquete.cargar(\"no_existe_este_archivo.dll\")\n"
            "escribir(milib)\n";
        harness::CasoTest caso{
            "paq_cargar_ruta_inexistente", codigo.c_str(),
            "paquete.cargar: no se pudo cargar 'no_existe_este_archivo.dll' nulo"};
        h.ejecutar(caso);
    }

    // Símbolo inexistente dentro de un módulo real: tampoco debe crashear
    // (mismo motivo de arriba para el diagnóstico esperado por stderr).
    {
        std::string codigo =
            "incluir \"paquete\"\n"
            "milib = paquete.cargar(\"" + rutaFixture + "\")\n"
            "escribir(paquete.llamar(milib, \"funcion_que_no_existe\", 0))\n";
        harness::CasoTest caso{
            "paq_llamar_funcion_inexistente", codigo.c_str(),
            "paquete: función 'funcion_que_no_existe' no encontrada en el módulo nulo"};
        h.ejecutar(caso);
    }

    return h.resumen();
}
