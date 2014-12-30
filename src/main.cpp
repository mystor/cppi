//
//  main.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sstream>

#include <llvm/ADT/Triple.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetSubtargetInfo.h>

#include "lexer.h"
#include "ast.h"
#include "parse.h"
#include "gen.h"

int main(int argc, const char * argv[]) {
    // Usage Message (TODO: Improve)
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <FileName>\n"
                  << "Compiles the file given by <FileName>\n";
        return 1;
    }

    // Read in the file passed as the first argument
    std::ifstream file_stream;
    file_stream.open(argv[1]);

    // We'll output to the file passed in as the second argument
    std::error_code ec;
    auto openflags = llvm::sys::fs::F_None;
    auto out = std::make_unique<llvm::tool_output_file>(argv[2], ec, openflags);

    if (ec) {
        std::cerr << argv[0] << ": " << ec.message() << "\n";
        return 1;
    }

    // Parse it!
    auto lex = Lexer(&file_stream);
    auto stmts = parse(&lex);

    std::cout << "Result of parsing: \n";
    for (auto &stmt : stmts) {
        std::cout << *stmt << ";\n";
    }

    auto mod = generate_module(stmts);

    mod->dump();

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    llvm::PassRegistry *registry = llvm::PassRegistry::getPassRegistry();
    llvm::initializeCore(*registry);
    llvm::initializeCodeGen(*registry);
    llvm::initializeLoopStrengthReducePass(*registry);
    llvm::initializeLowerIntrinsicsPass(*registry);
    llvm::initializeUnreachableBlockElimPass(*registry);

    llvm::Triple targetTriple(mod->getTargetTriple());
    if (targetTriple.getTriple().empty()) {
        targetTriple.setTriple(llvm::sys::getDefaultTargetTriple());
    }

    std::cout << "Target Triple: " << targetTriple.getTriple() << "\n";

    std::string error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget("", targetTriple, error);
    if (! target) {
        std::cerr << argv[0] << ": " << error;
        return 1;
    }

    auto optLvl = llvm::CodeGenOpt::Default;
    switch ('0') { // TEmporary
    default:
        std::cerr << argv[0] << ": invalid optimization level.\n";
        return 1;
    case ' ': break;
    case '0': optLvl = llvm::CodeGenOpt::None; break;
    case '1': optLvl = llvm::CodeGenOpt::Less; break;
    case '2': optLvl = llvm::CodeGenOpt::Default; break;
    case '3': optLvl = llvm::CodeGenOpt::Aggressive; break;
    }

    // llvm::TargetOptions options;
    // TODO: llc line 268

    std::unique_ptr<llvm::TargetMachine> target_machine(target->createTargetMachine(targetTriple.getTriple(),
                                                                                    llvm::sys::getHostCPUName(),
                                                                                    "",
                                                                                    llvm::TargetOptions(),
                                                                                    llvm::Reloc::Default,
                                                                                    llvm::CodeModel::Default,
                                                                                    optLvl));
    assert(target_machine && "Could not allocate target machine!");

    llvm::PassManager passmanager;
    llvm::TargetLibraryInfo *TLI = new llvm::TargetLibraryInfo(targetTriple);
    passmanager.add(TLI);

    if (const llvm::DataLayout *datalayout = target_machine->getSubtargetImpl()->getDataLayout())
        mod->setDataLayout(datalayout);

    llvm::formatted_raw_ostream ostream(out->os());

    // Ask the target to add backend passes as necessary.
    if (target_machine->addPassesToEmitFile(passmanager, ostream, llvm::TargetMachine::CGFT_ObjectFile)) {
        std::cerr << argv[0] << ": target does not support generation of this"
                  << " file type!\n";
        return 1;
    }

    passmanager.run(*mod);

    out->keep();

    return 0;
}
