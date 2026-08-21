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
  endif()
endif()

add_subdirectory(src)

# Runtime en C que usan los programas Latino generados. Se compila aquí para
# verificar que sigue siendo válido; los programas generados se enlazan con él.
add_library(latino_runtime STATIC runtime/latino.c)
target_include_directories(latino_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/runtime)

enable_testing()
add_subdirectory(tests)