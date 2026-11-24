#!/usr/bin/env bash
# tools/valgrind.sh — gate de memoria de PLAN_PERF.
#
# Compila una selección de ejemplos .lat (o los archivos que se pasen como
# argumentos) con los backends disponibles y ejecuta cada binario bajo Valgrind.
# Falla si hay errores de memoria o fugas definite/indirect. El estado
# "still reachable" legítimo de LLVM/glibc no cuenta como fallo (ver
# tests/valgrind.supp).
#
# Uso:
#   tools/valgrind.sh                       # ejemplos curados, backends disponibles
#   tools/valgrind.sh ejemplos/hola.lat ... # ejemplos concretos
#   BUILD=build-llvm tools/valgrind.sh      # usar otro directorio de build
#   tools/valgrind.sh --estres 10000        # agregar el estrés de concatenaciones
#   tools/valgrind.sh --build               # configurar/compilar build-valgrind antes
#   BACKEND=llvm tools/valgrind.sh          # solo LLVM (por defecto, todos)
#
# Requiere: cmake, compilador C y valgrind. No usa --trace-children: se mide el
# ejecutable Latino, no gcc/clang invocados por el compilador.

set -uo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD:-build-valgrind}"
BUILD_DIR="$BUILD"
[[ "$BUILD_DIR" == /* ]] || BUILD_DIR="$RAIZ/$BUILD_DIR"
BIN="$BUILD_DIR/src/latino"
LOGDIR="${LOGDIR:-/tmp/opencode/valgrind/gate}"
TMPDIR_GATE="$(mktemp -d /tmp/opencode/valgrind/gate.XXXXXX)"
trap 'rm -rf "$TMPDIR_GATE"' EXIT

ESTRES=0
RECONSTRUIR=0
CASOS=()
for arg in "$@"; do
    if [[ "$arg" == "--estres" ]]; then
        ESTRES=1
    elif [[ "$arg" == "--build" ]]; then
        RECONSTRUIR=1
    elif [[ "$ESTRES" == 1 && "$arg" =~ ^[0-9]+$ ]]; then
        # El número se consume en el bucle siguiente; ver más abajo.
        :
    else
        CASOS+=("$arg")
    fi
done

if ! command -v valgrind >/dev/null 2>&1; then
    echo "ERROR: no se encontró valgrind en PATH" >&2
    exit 2
fi
if (( RECONSTRUIR == 1 )); then
    if [[ ! -d "$BUILD_DIR" ]]; then
        if [[ "$BUILD" == build-valgrind ]]; then
            cmake -S "$RAIZ" -B "$BUILD_DIR" \
                -DCMAKE_BUILD_TYPE=Debug -DLATINO_LLVM_BACKEND=OFF || exit 2
        else
            cmake -S "$RAIZ" -B "$BUILD_DIR" \
                -DCMAKE_BUILD_TYPE=Debug -DLATINO_LLVM_BACKEND=ON || exit 2
        fi
    fi
    cmake --build "$BUILD_DIR" -j 4 || exit 2
fi
if [[ ! -x "$BIN" ]]; then
    echo "ERROR: no existe $BIN (¿cmake --build $BUILD?)" >&2
    exit 2
fi

if [[ ! -d "$LOGDIR" ]]; then
    mkdir -p "$LOGDIR"
fi

# Backends: C siempre; LLVM solo si el build lo tiene habilitado.
# BACKEND=c|llvm permite restringir el barrido.
BACKENDS=()
if [[ "${BACKEND:-}" != "llvm" ]]; then BACKENDS+=(c); fi
if [[ "${BACKEND:-}" != "c" ]] && \
   cmake -LA -N "$BUILD_DIR" 2>/dev/null | grep -q '^LATINO_LLVM_BACKEND:BOOL=ON'; then
    BACKENDS+=(llvm)
fi
if (( ${#BACKENDS[@]} == 0 )); then
    echo "ERROR: BACKEND=${BACKEND} no disponible en $BUILD" >&2
    exit 2
fi

# Segunda pasada: --estres N (N se guarda aparte para no mezclarlo con casos).
N_ESTRES=0
i=0
while (( i < $# )); do
    if [[ "${!i}" == "--estres" ]]; then
        j=$((i + 1))
        if (( j < $# )) && [[ "${!j}" =~ ^[0-9]+$ ]]; then
            N_ESTRES=${!j}
        else
            N_ESTRES=1000
        fi
    fi
    i=$((i + 1))
done

if (( ${#CASOS[@]} == 0 )); then
    CASOS=(
        hola cadena_ejemplo lista_ejemplo diccionarios mate_ejemplo
        herencia interfaces funciones genericos alcance_variables
    )
fi

if (( ESTRES == 1 )); then
    (( N_ESTRES == 0 )) && N_ESTRES=1000
    archivoEstres="$TMPDIR_GATE/estres_memoria.lat"
    {
        echo "desde (i = 0; i < $N_ESTRES; i++)"
        echo '    s = "abc" .. "def"'
        echo 'fin'
        echo 'escribir("fin")'
    } > "$archivoEstres"
    CASOS+=("$archivoEstres")
fi

FALLOS=0
TOTALES=0
for caso in "${CASOS[@]}"; do
    ruta="$caso"
    [[ "$ruta" == /* ]] || ruta="$RAIZ/$caso"
    if [[ ! -f "$ruta" ]]; then
        echo "  OMITIDO (no existe): $caso" >&2
        continue
    fi
    nombre="$(basename "${ruta%.lat}")"

    for backend in "${BACKENDS[@]}"; do
        TOTALES=$((TOTALES + 1))
        ejecutable="$TMPDIR_GATE/$nombre-$backend"
        log="$LOGDIR/${nombre}-${backend}.log"
        etiqueta="$nombre [$backend]"

        if ! (cd "$RAIZ" && "$BIN" "$ruta" --backend "$backend" -o "$ejecutable" \
              --runtime "$RAIZ/runtime") >"$TMPDIR_GATE/compilar.log" 2>&1; then
            echo "FALLO COMPILACIÓN: $etiqueta" >&2
            cat "$TMPDIR_GATE/compilar.log" >&2
            FALLOS=$((FALLOS + 1))
            continue
        fi

        valgrind --leak-check=full --show-leak-kinds=all \
            --errors-for-leak-kinds=definite,indirect \
            --suppressions="$RAIZ/tests/valgrind.supp" \
            --log-file="$log" --error-exitcode=99 \
            "$ejecutable" >"$TMPDIR_GATE/ejecutar.log" 2>&1
        rc=$?

        definite="$(grep -m1 'definitely lost:' "$log" | sed 's/.*definitely lost: //')"
        indirect="$(grep -m1 'indirectly lost:' "$log" | sed 's/.*indirectly lost: //')"
        if (( rc == 0 )); then
            echo "OK   $etiqueta — definite: ${definite:-0 bytes}, indirect: ${indirect:-0 bytes}"
        else
            echo "FALLO $etiqueta (rc=$rc) — definite: ${definite:-?}, indirect: ${indirect:-?}" >&2
            grep -E 'definitely lost|indirectly lost|Invalid|uninitialised|ERROR SUMMARY' "$log" >&2
            FALLOS=$((FALLOS + 1))
        fi
    done
done

echo "Resultado: $((TOTALES - FALLOS))/$TOTALES ejecuciones sin fugas ni errores. Logs: $LOGDIR"
(( FALLOS == 0 ))
