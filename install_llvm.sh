#!/usr/bin/env bash

set -Eeuo pipefail

MIN_MAJOR=18
LLVM_PREFIX="/usr/lib/llvm-18"

fail() {
    printf 'Error: %s\n' "$1" >&2
    exit 1
}

if [[ "${EUID}" -eq 0 ]]; then
    APT=(apt-get)
else
    command -v sudo >/dev/null 2>&1 || fail "se necesita root o sudo para instalar paquetes."
    APT=(sudo apt-get)
fi

command -v apt-get >/dev/null 2>&1 || fail "este instalador requiere una distribución Debian/Ubuntu con apt-get."

if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    source /etc/os-release
    case "${ID:-}" in
        debian|ubuntu|linuxmint|pop)
            ;;
        *)
            printf 'Advertencia: distribución no verificada (%s); se intentará usar apt-get.\n' "${PRETTY_NAME:-desconocida}" >&2
            ;;
    esac
fi

printf '%s\n' '==> Instalando LLVM 18, Clang 18, Zlib, Zstd, CURL y los archivos de desarrollo...'
"${APT[@]}" update
"${APT[@]}" install -y cmake llvm-18 llvm-18-dev llvm-18-tools clang-18 \
    zlib1g-dev libzstd-dev libcurl4-openssl-dev

LLVM_CONFIG="${LLVM_PREFIX}/bin/llvm-config"
CLANG="${LLVM_PREFIX}/bin/clang"
[[ -x "${LLVM_CONFIG}" ]] || LLVM_CONFIG="$(command -v llvm-config-18 || true)"
[[ -x "${CLANG}" ]] || CLANG="$(command -v clang-18 || true)"

[[ -n "${LLVM_CONFIG}" && -x "${LLVM_CONFIG}" ]] || fail "no se encontró llvm-config de LLVM 18."
[[ -n "${CLANG}" && -x "${CLANG}" ]] || fail "no se encontró clang de LLVM 18."

LLVM_VERSION="$(${LLVM_CONFIG} --version)"
LLVM_MAJOR="${LLVM_VERSION%%.*}"
[[ "${LLVM_MAJOR}" == "${MIN_MAJOR}" ]] || fail "se instaló LLVM ${LLVM_VERSION}; se requiere la serie ${MIN_MAJOR}.x."

printf '\nLLVM instalado correctamente: %s\n' "${LLVM_VERSION}"
printf 'llvm-config: %s\n' "${LLVM_CONFIG}"
printf 'clang:       %s\n' "${CLANG}"
printf '\nPara configurar el proyecto:\n\n'
printf '    cmake -B build-llvm -DLATINO_LLVM_BACKEND=ON -DCMAKE_PREFIX_PATH=%s\n' "${LLVM_PREFIX}"
printf '    cmake --build build-llvm --config Release\n'