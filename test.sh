#!/usr/bin/env bash

set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build-llvm"
LLVM_BACKEND=ON
BUILD_CONFIG=""

usage() {
    cat <<EOF
Uso: $(basename "$0") [opciones]

Opciones:
  --build-dir DIR  Directorio de build (por defecto: build-llvm)
  --backend NAME   Backend C o LLVM (por defecto: LLVM)
  --config CONFIG  Configuración del build, por ejemplo Release
  -h, --help       Mostrar esta ayuda
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            [[ $# -ge 2 ]] || { printf 'Falta el valor de --build-dir.\n' >&2; exit 2; }
            BUILD_DIR="$2"
            shift 2
            ;;
        --backend)
            [[ $# -ge 2 ]] || { printf 'Falta el valor de --backend.\n' >&2; exit 2; }
            case "$2" in
                c) LLVM_BACKEND=OFF ;;
                llvm) LLVM_BACKEND=ON ;;
                *) printf 'Backend inválido: %s (usa c o llvm).\n' "$2" >&2; exit 2 ;;
            esac
            shift 2
            ;;
        --config)
            [[ $# -ge 2 ]] || { printf 'Falta el valor de --config.\n' >&2; exit 2; }
            BUILD_CONFIG="$2"
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

if [[ "${BUILD_DIR}" != /* ]]; then
    BUILD_DIR="${ROOT_DIR}/${BUILD_DIR}"
fi

printf '==> Configurando %s (LLVM_BACKEND=%s)\n' "${BUILD_DIR}" "${LLVM_BACKEND}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DLATINO_LLVM_BACKEND="${LLVM_BACKEND}" \
    ${BUILD_CONFIG:+--config "${BUILD_CONFIG}"}

printf '\n==> Compilando tests\n'
if [[ -n "${BUILD_CONFIG}" ]]; then
    cmake --build "${BUILD_DIR}" --config "${BUILD_CONFIG}"
else
    cmake --build "${BUILD_DIR}"
fi

printf '\n==> Ejecutando CTest en serie\n'
ctest --test-dir "${BUILD_DIR}" --output-on-failure --no-tests=error