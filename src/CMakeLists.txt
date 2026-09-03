# Valores horneados para que el driver localice el compilador de C y el runtime.
if(MSVC)
  set(LATINO_CC_ESTILO "msvc")
  # CMAKE_GENERATOR_INSTANCE es la ruta de la instancia de VS (generador VS).
  set(LATINO_VSDEVCMD "${CMAKE_GENERATOR_INSTANCE}/Common7/Tools/VsDevCmd.bat")
else()
  set(LATINO_CC_ESTILO "gnu")
  set(LATINO_VSDEVCMD "")
endif()
set(LATINO_CC "${CMAKE_C_COMPILER}")
set(LATINO_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/runtime")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

add_executable(latino
  main.cpp
  lexer.cpp
  ast.cpp
  ast_impresor.cpp
  parser.cpp
  analizador_semantico.cpp
  compiler.cpp
  fn_binaria.cpp
  recolector_variables.cpp
  invocador_c.cpp
  interpreter.cpp
)

target_include_directories(latino PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR})

# Backend LLVM (ver input/PLAN_LLVM.md). Solo se agrega si el CMakeLists raíz
# encontró una instalación de LLVM 17.x compatible (LATINO_LLVM_BACKEND).
if(LATINO_LLVM_BACKEND)
  target_sources(latino PRIVATE compiler_llvm.cpp invocador_llvm.cpp runtime_abi_llvm.cpp)
  target_compile_definitions(latino PRIVATE LATINO_CON_LLVM)
  # compiler_llvm.cpp incluye runtime/latino.h (extern "C") para reusar los
  # valores reales del enum LatTipo (LAT_NUMERO, ...) en vez de re-derivarlos
  # a mano -- ver Fase L4 de input/PLAN_LLVM.md.
  target_include_directories(latino PRIVATE ${LATINO_RUNTIME_DIR})

  # generated/runtime_abi.ll (Fase L2) debe existir antes de que latino.exe
  # lo importe en tiempo de ejecución (RuntimeAbiLLVM); no es una dependencia
  # de compilación de latino.exe, pero sí de su ejecución/pruebas.
  add_dependencies(latino latino_runtime_abi)

  # Componentes: los de la Fase L1 (construir un Module y emitir un objeto
  # nativo vía TargetMachine) + 'irreader' de la Fase L2 (importar
  # runtime_abi.ll) + 'orcjit' de la Fase L10 (modo --jit, llvm::orc::LLJIT).
  llvm_map_components_to_libnames(LATINO_LLVM_LIBS
    core support nativecodegen native mc object target irreader orcjit)

  target_include_directories(latino PRIVATE ${LLVM_INCLUDE_DIRS})
  separate_arguments(LATINO_LLVM_DEFS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
  target_compile_definitions(latino PRIVATE ${LATINO_LLVM_DEFS})
  target_link_libraries(latino PRIVATE ${LATINO_LLVM_LIBS})

  # Fase L10 (Decisión 4): el runtime + las 7 bibliotecas cargados junto a
  # latino.exe, para que el modo --jit resuelva lat_* contra el propio
  # proceso (ver el comentario junto a latino_runtime_estatico en el
  # CMakeLists.txt raíz -- ahí también el hallazgo de por qué es una
  # biblioteca dinámica, no estática, en Windows).
  target_link_libraries(latino PRIVATE latino_runtime_estatico)

  # invocador_llvm.cpp (ejecutarJit) necesita el nombre de archivo real de
  # la biblioteca (varía por plataforma: latino_runtime_estatico.dll en
  # Windows, liblatino_runtime_estatico.so/.dylib en Linux/macOS) para
  # cargarla explícitamente por ruta -- ver el hallazgo junto a
  # latino_runtime_estatico en el CMakeLists.txt raíz sobre por qué no basta
  # con DynamicLibrarySearchGenerator::GetForCurrentProcess() a secas en
  # Windows. $<TARGET_FILE_NAME:...> es una expresión de generador (resuelta
  # en tiempo de build, no de configuración), por eso va aquí y no en
  # config.h.in (que solo admite variables de CMake normales).
  target_compile_definitions(latino PRIVATE
    LATINO_RUNTIME_ESTATICO_NOMBRE="$<TARGET_FILE_NAME:latino_runtime_estatico>")

  # latino_runtime_estatico ahora es una biblioteca dinámica (ver hallazgo
  # arriba): Windows la busca junto al .exe al cargarlo. El generador de
  # Visual Studio coloca cada target en su propio árbol de salida
  # (build/src/Release/latino.exe vs. build/Release/latino_runtime_estatico.dll,
  # porque el target se declara en el CMakeLists.txt raíz), así que sin este
  # paso el .exe no la encontraría al arrancar --jit.
  add_custom_command(TARGET latino POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:latino_runtime_estatico>"
            "$<TARGET_FILE_DIR:latino>"
    COMMENT "Fase L10: copiando latino_runtime_estatico junto a latino.exe (modo --jit)")
endif()
