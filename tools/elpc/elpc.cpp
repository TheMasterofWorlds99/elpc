// =====================================================================
// elpc — CLI driver for the ELPC compiler framework
//
// Reads a source file, runs the full pipeline, and outputs the result.
// Can emit C code, IR dump, or token/ast debugging info.
//
// Usage:
//   elpc file.my                  # → stdout.c
//   elpc file.my -o out.c         # → out.c
//   elpc file.my --dump-ir        # → IR text
//   elpc file.my --tokens         # → token list
//   elpc file.my --ast            # → AST tree
// =====================================================================

#include <elpc/elpc.hpp>
#include <iostream>
#include <string>
#include <vector>

struct Args {
  std::string inputFile;
  std::string outputFile;
  bool dumpIR = false;
  bool dumpTokens = false;
  bool dumpAST = false;
  bool help = false;
};

static Args parseArgs(int argc, char **argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) a.outputFile = argv[++i];
    else if (arg == "--dump-ir")  a.dumpIR = true;
    else if (arg == "--tokens")   a.dumpTokens = true;
    else if (arg == "--ast")      a.dumpAST = true;
    else if (arg == "-h" || arg == "--help") a.help = true;
    else a.inputFile = arg;
  }
  return a;
}

static void printHelp(const char *prog) {
  std::cerr << "ELPC Compiler Framework v" << ELPC_VERSION << "\n\n";
  std::cerr << "Usage: " << prog << " [options] <file>\n\n";
  std::cerr << "Options:\n";
  std::cerr << "  -o <file>      Output file (default: stdout)\n";
  std::cerr << "  --dump-ir      Dump IR before codegen\n";
  std::cerr << "  --tokens       Print token list\n";
  std::cerr << "  --ast          Print AST tree\n";
  std::cerr << "  -h, --help     Print this help\n";
}

int main(int argc, char **argv) {
  auto args = parseArgs(argc, argv);

  if (args.help || args.inputFile.empty()) {
    printHelp(argv[0]);
    return args.help ? 0 : 1;
  }

  try {
    auto src = elpc::SourceManager::fromFile(args.inputFile);

    // For now, just show the source stats as a demo
    // (The actual pipeline is provided by the Mini language frontend)
    std::cerr << "elpc: loaded " << src.size() << " bytes from '"
              << args.inputFile << "'\n";
    std::cerr << "elpc: pipe your source through a language frontend\n";
    std::cerr << "      (e.g., examples/mini_ir/mini_ir.cpp)\n";

    if (args.dumpTokens || args.dumpAST || args.dumpIR) {
      std::cout << "// Source:\n" << src.text() << "\n";
    }

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "elpc: error: " << e.what() << "\n";
    return 1;
  }
}
