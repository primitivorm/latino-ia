// invocador_llvm.cpp — ver invocador_llvm.h
//
// Verificado contra LLVM 18.1.6 real (vcpkg, x64-windows-release, ver Fase
// L1 de input/PLAN_LLVM.md). Notas de esa verificación:
//   - getDefaultTargetTriple() vive en llvm/TargetParser/Host.h en 18.x.
//   - El tipo de archivo a emitir es el enum class llvm::CodeGenFileType
//     (llvm/Support/CodeGen.h), valor ObjectFile — NO llvm::CGFT_ObjectFile
//     (nombre de la API vieja).
//   - El raw_fd_ostream del objeto debe cerrarse (destruirse) ANTES de
//     invocar al linker: en Windows, cl.exe/link.exe fallan con
//     "no se puede abrir el archivo" si este proceso todavía tiene el
//     objeto abierto para escritura.

#include "invocador_llvm.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <system_error>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include "invocador_c.h"

namespace fs = std::filesystem;

namespace {

// Inicializa una sola vez el target nativo (la arquitectura de la máquina
// que ejecuta latino.exe). No se contempla cross-compilación de target en
// el alcance de este plan.
void inicializarTargetNativoUnaVez() {
    static bool inicializado = false;
    if (inicializado) return;
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    inicializado = true;
}

}  // namespace

int compilarLLVMAEjecutable(llvm::Module& modulo, const std::string& salidaExe,
                            const OpcionesLLVM& opciones) {
    inicializarTargetNativoUnaVez();

    std::string triple = llvm::sys::getDefaultTargetTriple();
    modulo.setTargetTriple(triple);

    std::string errorTarget;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, errorTarget);
    if (!target) {
        std::cerr << "Error: no se encontró un target de LLVM para '" << triple
                  << "': " << errorTarget << std::endl;
        return 1;
    }

    llvm::TargetOptions opcionesTarget;
    std::unique_ptr<llvm::TargetMachine> maquina(target->createTargetMachine(
        triple, "generic", "", opcionesTarget, llvm::Reloc::PIC_));
    if (!maquina) {
        std::cerr << "Error: no se pudo crear el TargetMachine de LLVM para '"
                  << triple << "'." << std::endl;
        return 1;
    }
    modulo.setDataLayout(maquina->createDataLayout());

    // ".obj" (no ".o") para que cl.exe la reconozca sin advertencia; gcc/
    // clang no distinguen por extensión al enlazar un objeto ya compilado.
    fs::path rutaObj = fs::temp_directory_path() / "latino_llvm_obj.obj";
    {
        std::error_code ec;
        llvm::raw_fd_ostream flujoObj(rutaObj.string(), ec, llvm::sys::fs::OF_None);
        if (ec) {
            std::cerr << "Error: no se pudo abrir " << rutaObj << " para escritura: "
                      << ec.message() << std::endl;
            return 1;
        }

        llvm::legacy::PassManager pm;
        if (maquina->addPassesToEmitFile(pm, flujoObj, nullptr, llvm::CodeGenFileType::ObjectFile)) {
            std::cerr << "Error: el TargetMachine no puede emitir archivos de "
                         "objeto para el target '" << triple << "'." << std::endl;
            return 1;
        }
        pm.run(modulo);
        // flujoObj (y por tanto el descriptor de archivo) se cierra al salir
        // de este bloque, ANTES de invocar al linker más abajo — en Windows,
        // dejar el archivo abierto aquí hace que cl.exe/link.exe fallen con
        // "no se puede abrir el archivo" al intentar leerlo.
    }

    // Reutiliza el mismo paso de enlace que ya usa el backend de C: el
    // compilador de C del sistema (cl.exe/VsDevCmd o cc/gcc/clang) acepta
    // tanto fuentes .c como objetos .obj/.o en la misma línea de enlace, así
    // que compilarAEjecutable no necesita saber que este archivo vino de
    // LLVM en vez de GeneradorC.
    OpcionesC opcionesC;
    opcionesC.runtimeDir = opciones.runtimeDir;
    return compilarAEjecutable(rutaObj.string(), salidaExe, opcionesC);
}
