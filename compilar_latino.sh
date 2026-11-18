#!/usr/bin/env bash

# Equivalente en Linux de compilar_latino.ps1 (ver README.md / input/PLAN_LLVM.md).
# Pregunta si se quiere soporte para el backend LLVM (--backend=llvm). Si se
# pide, instala LLVM 18 con install_llvm.sh y genera un directorio de build
# separado ("build-llvm") apuntando a esa instalación, ya que el prefijo de
# CMake solo se fija en la primera configuración de un directorio de build.
# Sin LLVM se usa "build" y se deshabilita explícitamente el backend, para que
# un LLVM instalado en el sistema no lo active en contra de la respuesta.

set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLVM_PREFIX="${LLVM_PREFIX:-/usr/lib/llvm-18}"
BUILD_DIR=""
CON_LLVM=""

usage() {
    cat <<EOF
Uso: $(basename "$0") [opciones]

Opciones:
  --llvm        Habilitar el backend LLVM (instala LLVM 18 si hace falta)
  --sin-llvm    Compilar solo con el backend C
  --build-dir   Directorio de build (por defecto: build o build-llvm)
  -h, --help    Mostrar esta ayuda

Sin opciones, el script pregunta si se desea soporte LLVM, igual que
compilar_latino.ps1.
EOF
}

fail() {
    printf 'Error: %s\n' "$1" >&2
    exit 1
}

# LLVM ya usable: llvm-config/clang de la serie 18.1 y LLVMConfig.cmake
# (este último lo aporta llvm-18-dev y es lo que usa find_package de CMake).
llvm_ya_instalado() {
    local config="${LLVM_PREFIX}/bin/llvm-config"
    local clang="${LLVM_PREFIX}/bin/clang"
    local version

    [[ -f "${LLVM_PREFIX}/lib/cmake/llvm/LLVMConfig.cmake" ]] || return 1
    [[ -x "${config}" ]] || config="$(command -v llvm-config-18 || true)"
    [[ -n "${config}" && -x "${config}" ]] || return 1
    version="$("${config}" --version 2>/dev/null)" || return 1
    [[ "${version}" == 18.1.* ]] || return 1
    [[ -x "${clang}" ]] || clang="$(command -v clang-18 || true)"
    [[ -n "${clang}" && -x "${clang}" ]]
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --llvm)
            CON_LLVM=si
            shift
            ;;
        --sin-llvm)
            CON_LLVM=no
            shift
            ;;
        --build-dir)
            [[ $# -ge 2 ]] || { printf 'Falta el valor de --build-dir.\n' >&2; exit 2; }
            BUILD_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Opción desconocida: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "${CON_LLVM}" ]]; then
    respuesta=""
    read -r -p "¿Deseas soporte para el backend LLVM (--backend=llvm)? (s/N) " respuesta || respuesta=""
    if [[ "${respuesta}" =~ ^[sS] ]]; then
        CON_LLVM=si
    else
        CON_LLVM=no
    fi
fi

if [[ "${CON_LLVM}" == "si" ]]; then
    if llvm_ya_instalado; then
        printf '\n==> LLVM 18.1 ya está instalado en %s; se omite install_llvm.sh.\n' "${LLVM_PREFIX}"
    else
        printf '\n==> Instalando LLVM con install_llvm.sh...\n'
        [[ -f "${ROOT_DIR}/install_llvm.sh" ]] || fail "no se encontró install_llvm.sh en ${ROOT_DIR}."
        bash "${ROOT_DIR}/install_llvm.sh"
        llvm_ya_instalado || fail "LLVM 18.1 sigue sin estar disponible en ${LLVM_PREFIX} después de install_llvm.sh."
    fi
    [[ -n "${BUILD_DIR}" ]] || BUILD_DIR="${ROOT_DIR}/build-llvm"
else
    [[ -n "${BUILD_DIR}" ]] || BUILD_DIR="${ROOT_DIR}/build"
fi

if [[ "${BUILD_DIR}" != /* ]]; then
    BUILD_DIR="${ROOT_DIR}/${BUILD_DIR}"
fi
[[ "${BUILD_DIR}" != "/" && "${BUILD_DIR}" != "${ROOT_DIR}" ]] || fail "--build-dir no puede ser la raíz del proyecto ni '/'."

# Igual que el script de Windows: se descarta la configuración previa para
# evitar conflictos de caché (prefijo de LLVM / generador) entre build y
# build-llvm.
if [[ -e "${BUILD_DIR}" ]]; then
    printf '\n==> Se elimina la configuración previa de CMake en %s para evitar conflictos.\n' "${BUILD_DIR}"
    rm -rf -- "${BUILD_DIR}"
fi
mkdir -p -- "${BUILD_DIR}"

printf '\n==> Configurando CMake en %s (LATINO_LLVM_BACKEND=%s)\n' "${BUILD_DIR}" "$([[ "${CON_LLVM}" == "si" ]] && printf ON || printf OFF)"
if [[ "${CON_LLVM}" == "si" ]]; then
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DLATINO_LLVM_BACKEND=ON \
        -DCMAKE_PREFIX_PATH="${LLVM_PREFIX}"
else
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DLATINO_LLVM_BACKEND=OFF
fi

printf '\n==> Compilando en modo Release\n'
cmake --build "${BUILD_DIR}" --config Release

if [[ "${CON_LLVM}" == "si" ]]; then
    # CMake solo avisa (WARNING) si no encuentra LLVM 18.1.x y sigue
    # compilando sin backend; fallar aquí evita entregar un build engañoso.
    [[ -f "${BUILD_DIR}/generated/runtime_abi.ll" ]] || \
        fail "no se generó generated/runtime_abi.ll: LLVM 18.1.x o clang no fueron detectados por CMake (ver el aviso de configuración e input/PLAN_LLVM.md)."
fi

EJECUTABLE="$(find "${BUILD_DIR}/src" -maxdepth 2 -type f -name latino -perm -u+x -print -quit)"
[[ -n "${EJECUTABLE}" ]] || fail "no se encontró el ejecutable latino en ${BUILD_DIR}/src."

printf '\nCompilación completada en %s (Release). El ejecutable latino está en %s.\n' "${BUILD_DIR}" "${EJECUTABLE}"
