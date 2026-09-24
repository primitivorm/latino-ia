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

# Fase L10 (ver input/PLAN_LLVM.md, Decisión 4): modo --jit. El runtime +
# las 7 bibliotecas se compilan una segunda vez aquí y se cargan JUNTO a
# latino.exe (ver src/CMakeLists.txt) -- distinto del uso normal, donde
# invocador_c.cpp los recompila desde cero en cada invocación para el
# ejecutable del USUARIO. El propósito de esta copia es que sus símbolos
# lat_* sean resolubles en el propio proceso de latino.exe, para que
# llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess() los
# encuentre por nombre. Solo se agrega si el backend LLVM está habilitado --
# el modo --jit no existe sin él.
#
# Hallazgo real (no anticipado en el texto original de la Decisión 4): en
# Windows, GetForCurrentProcess() resuelve símbolos vía GetProcAddress()
# sobre los módulos cargados del proceso -- y GetProcAddress() SOLO ve
# símbolos que un módulo EXPORTA explícitamente. Un símbolo enlazado
# ESTÁTICAMENTE dentro de latino.exe (STATIC, la lectura literal de
# "estático" en el nombre del target) nunca aparece en la tabla de
# exportación de latino.exe (MSVC no exporta nada de un .exe sin un .def o
# __declspec(dllexport) explícitos) y por tanto es invisible para ORC en
# Windows, aunque perfectamente enlazado y llamable desde C++ normal --
# confirmado en la práctica: el runtime estaba enlazado y el programa AOT
# equivalente funcionaba, pero el JIT fallaba con "Symbols not found:
# [lat_abi_verificar, lat_cadena, lat_escribir, lat_set_args]" seguido de un
# segfault al intentar ejecutar un `main` nunca materializado. La solución
# que sí resuelve esto en la práctica (y que la propia Decisión 4 ya preveía
# como alternativa): compilar el runtime como biblioteca DINÁMICA en vez de
# estática, con CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS (exporta automáticamente
# todo símbolo de función del target en Windows, sin necesitar un .def a
# mano para las ~185 funciones del runtime; no-op en Linux/macOS, donde los
# símbolos de una biblioteca dinámica ya son visibles por defecto). Cargada
# implícitamente al iniciar latino.exe (enlace dinámico normal), sus
# símbolos exportados sí quedan dentro del conjunto de módulos que
# GetForCurrentProcess() enumera. El nombre del target se conserva
# (`latino_runtime_estatico`) porque es el que fija la Decisión 4 del plan;
# lo que cambió es CÓMO se enlaza dentro de latino.exe, no su propósito.
if(LATINO_LLVM_BACKEND)
  add_library(latino_runtime_estatico SHARED
    runtime/latino.c
    runtime/libs/archivo.c
    runtime/libs/cadena.c
    runtime/libs/dic.c
    runtime/libs/lista.c
    runtime/libs/mate.c
    runtime/libs/paquete.c
    runtime/libs/sis.c)
  target_include_directories(latino_runtime_estatico PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
  set_target_properties(latino_runtime_estatico PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

add_subdirectory(src)

# Runtime en C que usan los programas Latino generados. Se compila aquí para
# verificar que sigue siendo válido; los programas generados se enlazan con él.
add_library(latino_runtime STATIC runtime/latino.c)
target_include_directories(latino_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime)

enable_testing()
add_subdirectory(tests)