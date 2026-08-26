// fn_binaria.cpp — ver fn_binaria.h

#include "fn_binaria.h"

const char* fnBinaria(const std::string& op) {
    if (op == "+")  return "lat_sumar";
    if (op == "-")  return "lat_restar";
    if (op == "*")  return "lat_multiplicar";
    if (op == "/")  return "lat_dividir";
    if (op == "%")  return "lat_modulo";
    if (op == "^")  return "lat_potencia";
    if (op == "..") return "lat_concatenar";
    if (op == "==") return "lat_igual";
    if (op == "!=") return "lat_distinto";
    if (op == "<")  return "lat_menor";
    if (op == ">")  return "lat_mayor";
    if (op == "<=") return "lat_menor_igual";
    if (op == ">=") return "lat_mayor_igual";
    if (op == "&&") return "lat_y";
    if (op == "||") return "lat_o";
    if (op == "~=") return "lat_coincide";
    return nullptr;
}
