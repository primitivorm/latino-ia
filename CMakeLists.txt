cmake_minimum_required(VERSION 3.12)
project(latino)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Backend de generación de código basado en LLVM (ver input/PLAN_LLVM.md).
# Convive con el backend de C (--backend=c, sigue siendo el predeterminado).
# Se habilita solo si se encuentra una instalación de LLVM 18.1.x compatible;
# si no, el proyecto compila igual, solo sin la opción --backend=llvm.
# (El plan original fijaba 17.x; se subió a 18.1.x porque es la versión que
# provee el registro de vcpkg al momento de instalar — ver Fase L1 del plan.
# Nota: find_package(LLVM 18 CONFIG), con solo el major, FALLA — el archivo
# de compatibilidad de versión de LLVM exige coincidencia de major.minor, no
# solo major; hay que pedir "18.1" explícitamente.)
option(LATINO_LLVM_BACKEND "Habilitar el backend de codegen basado en LLVM" ON)
if(LATINO_LLVM_BACKEND)
  find_package(LLVM 18.1 CONFIG QUIET)
  if(NOT LLVM_FOUND)
    message(WARNING
      "LATINO_LLVM_BACKEND=ON pero no se encontró LLVM 18.1.x "
      "(find_package(LLVM 18.1 CONFIG) falló). Se compilará sin el backend "
      "LLVM (--backend=llvm no estará disponible). Ver input/PLAN_LLVM.md "
      "para instrucciones de instalación por plataforma.")
    set(LATINO_LLVM_BACKEND OFF)
  else()
    message(STATUS "LLVM ${LLVM_PACKAGE_VERSION} encontrado en ${LLVM_DIR} — backend LLVM habilitado.")

    # Fase L2 (ver input/PLAN_LLVM.md, Decisión 2): el mecanismo de ABI de
    # LatValor necesita Clang -- el mismo compilador que compila
    # runtime/latino.c -- para derivar generated/runtime_abi.ll. Sin Clang,
    # GeneradorLLVM no tiene forma segura de conocer la clasificación ABI
    # real de LatValor (struct de 16 bytes pasado/devuelto por valor).
    find_program(LATINO_CLANG_EXECUTABLE
      NAMES clang clang.exe
      HINTS ${LLVM_TOOLS_BINARY_DIR}
      DOC "Clang usado para derivar el ABI real de LatValor (Fase L2 de input/PLAN_LLVM.md)")
    if(NOT LATINO_CLANG_EXECUTABLE)
      message(WARNING
        "LATINO_LLVM_BACKEND=ON pero no se encontró clang (buscado en "
        "${LLVM_TOOLS_BINARY_DIR}), necesario para el mecanismo de ABI de la "
        "Fase L2 de input/PLAN_LLVM.md. Se compilará sin el backend LLVM. "
        "Instalar con la feature 'clang' del puerto de vcpkg (ver install_llvm.ps1).")
      set(LATINO_LLVM_BACKEND OFF)
    else()
      message(STATUS "Clang encontrado en ${LATINO_CLANG_EXECUTABLE} (Fase L2: derivación de ABI).")
    endif()
  endif()
endif()

# Ruta al IR de LLVM con el ABI real de LatValor derivado por Clang (Fase L2
# de input/PLAN_LLVM.md). Cadena vacía si el backend LLVM no está habilitado
# (ver src/config.h.in).
set(LATINO_RUNTIME_ABI_LL "")
if(LATINO_LLVM_BACKEND)
  set(LATINO_RUNTIME_ABI_LL "${CMAKE_BINARY_DIR}/generated/runtime_abi.ll")

  if(MSVC)
    # Triple real del toolchain de destino (Decisión 2 del plan): no el
    # triple genérico de MinGW.
    set(LATINO_ABI_CLANG_ARGS -target x86_64-pc-windows-msvc)
  else()
    # Mismo Clang/GCC de sistema que compilará runtime/latino.c: se usa su
    # triple por defecto.
    set(LATINO_ABI_CLANG_ARGS "")
  endif()

  file(GLOB LATINO_RUNTIME_ABI_HEADERS
    "${CMAKE_SOURCE_DIR}/runtime/*.h"
    "${CMAKE_SOURCE_DIR}/runtime/libs/*.h")

  add_custom_command(
    OUTPUT "${LATINO_RUNTIME_ABI_LL}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${LATINO_CLANG_EXECUTABLE}" -S -emit-llvm ${LATINO_ABI_CLANG_ARGS}
            "-I${CMAKE_SOURCE_DIR}/runtime"
            "${CMAKE_SOURCE_DIR}/tools/abi_probe.c"
            -o "${LATINO_RUNTIME_ABI_LL}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/abi_probe.c" ${LATINO_RUNTIME_ABI_HEADERS}
    COMMENT "Fase L2 (PLAN_LLVM.md): derivando el ABI de LatValor con Clang -> generated/runtime_abi.ll"
    VERBATIM)
  add_custom_target(latino_runtime_abi DEPENDS "${LATINO_RUNTIME_ABI_LL}")
endif()

add_subdirectory(src)

# Runtime en C que usan los programas Latino generados. Se compila aquí para
# verificar que sigue siendo válido; los programas generados se enlazan con él.
add_library(latino_runtime STATIC runtime/latino.c)
target_include_directories(latino_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime)

enable_testing()
add_subdirectory(tests)