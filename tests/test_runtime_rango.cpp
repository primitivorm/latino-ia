// Pruebas directas del valor rango y de lista.a_lista, incluidas rutas de error.

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

extern "C" {
#include "latino.h"
#include "lista.h"
}

static int fallos = 0;
static int comprobaciones = 0;

#define CHECK(cond, mensaje) do { \
    ++comprobaciones; \
    if (!(cond)) { ++fallos; std::cerr << "FALLO: " << mensaje << "\n"; } \
} while (0)

static LatValor crearRango(std::initializer_list<LatValor> valores) {
    return lat_rango(valores.size(), valores.begin());
}

static void prueba_rangos() {
    LatValor uno = crearRango({lat_numero(5)});
    CHECK(uno.tipo == LAT_RANGO, "rango(fin) crea LAT_RANGO");
    CHECK(lat_rango_contiene(uno, lat_numero(0)), "rango(5) inicia en cero");
    CHECK(lat_rango_contiene(uno, lat_numero(4)), "rango(5) incluye fin-1");
    CHECK(!lat_rango_contiene(uno, lat_numero(5)), "rango(5) excluye el fin");

    LatValor dos = crearRango({lat_numero(2), lat_numero(7)});
    CHECK(lat_rango_inicio(dos).como.numero == 2, "rango(inicio, fin) conserva inicio");
    CHECK(lat_rango_contiene(dos, lat_numero(6)) &&
              !lat_rango_contiene(dos, lat_numero(7)),
          "rango(inicio, fin) usa fin exclusivo");

    LatValor tres = crearRango({lat_numero(1), lat_numero(10), lat_numero(2)});
    LatValor actual = lat_rango_inicio(tres);
    const double esperados[] = {1, 3, 5, 7, 9};
    for (double esperado : esperados) {
        CHECK(lat_rango_contiene(tres, actual), "rango con salto positivo contiene el siguiente");
        CHECK(actual.como.numero == esperado, "rango con salto positivo");
        actual = lat_rango_siguiente(tres, actual);
    }
    CHECK(!lat_rango_contiene(tres, actual), "rango con salto positivo termina excluyendo el fin");

    LatValor negativo = crearRango({lat_numero(5), lat_numero(0), lat_numero(-1)});
    actual = lat_rango_inicio(negativo);
    const double esperadosNegativos[] = {5, 4, 3, 2, 1};
    for (double esperado : esperadosNegativos) {
        CHECK(lat_rango_contiene(negativo, actual), "rango negativo contiene el siguiente");
        CHECK(actual.como.numero == esperado, "rango negativo");
        actual = lat_rango_siguiente(negativo, actual);
    }
    CHECK(!lat_rango_contiene(negativo, actual), "rango negativo excluye el fin");

    LatValor tipo = lat_tipo(uno);
    CHECK(tipo.tipo == LAT_CADENA && std::strcmp(tipo.como.cadena, "rango") == 0,
          "tipo(rango) devuelve rango");
    lat_valor_liberar(tipo);
    lat_valor_liberar(uno);
    lat_valor_liberar(dos);
    lat_valor_liberar(tres);
    lat_valor_liberar(negativo);
}

static void prueba_materializar() {
    LatValor rango = crearRango({lat_numero(0), lat_numero(10), lat_numero(2)});
    LatValor lista = lat_lista_a_lista(rango);
    CHECK(lista.tipo == LAT_LISTA && lista.como.lista->longitud == 5,
          "lista.a_lista materializa exactamente cinco valores");
    const double esperados[] = {0, 2, 4, 6, 8};
    for (size_t i = 0; i < 5; ++i)
        CHECK(lista.como.lista->datos[i].como.numero == esperados[i],
              "lista.a_lista conserva orden y paso");
    lat_valor_liberar(lista);
    lat_valor_liberar(rango);

    LatValor grande = crearRango(
        {lat_numero(9007199254740992.0), lat_numero(9007199254740996.0), lat_numero(3)});
    LatValor materializado = lat_lista_a_lista(grande);
    CHECK(materializado.como.lista->longitud == 1,
          "la lista no debe incluir un valor redondeado al límite exclusivo del rango");
    CHECK(materializado.como.lista->datos[0].como.numero == 9007199254740992.0,
          "la materialización conserva el primer valor cuando el paso redondea");
    lat_valor_liberar(materializado);
}

static int probarError(const std::string& caso) {
    if (caso == "aridad_cero") (void)lat_rango(0, nullptr);
    if (caso == "aridad_cuatro") {
        LatValor args[] = {lat_numero(0), lat_numero(1), lat_numero(1), lat_numero(2)};
        (void)lat_rango(4, args);
    }
    if (caso == "no_numero") {
        LatValor args[] = {lat_cadena("1")};
        (void)lat_rango(1, args);
    }
    if (caso == "fraccionario") {
        LatValor args[] = {lat_numero(1.5)};
        (void)lat_rango(1, args);
    }
    if (caso == "no_finito") {
        LatValor args[] = {lat_numero(HUGE_VAL)};
        (void)lat_rango(1, args);
    }
    if (caso == "paso_cero") {
        LatValor args[] = {lat_numero(0), lat_numero(5), lat_numero(0)};
        (void)lat_rango(3, args);
    }
    if (caso == "lista_tipo") (void)lat_lista_a_lista(lat_numero(3));
    if (caso == "materializacion_overflow") {
        LatValor args[] = {lat_numero(0), lat_numero(1e300), lat_numero(1)};
        LatValor rango = lat_rango(3, args);
        (void)lat_lista_a_lista(rango);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1)
        return probarError(argv[1]);
    prueba_rangos();
    prueba_materializar();
    std::cout << "Comprobaciones: " << comprobaciones << "  Fallos: " << fallos << "\n";
    return fallos ? 1 : 0;
}
