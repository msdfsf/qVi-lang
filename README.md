# The qVi Programming Language (under construction)

qVi (pronounced "Kiwi") is yet another procedural system language with flavor of compile-time execution and array *vectorization*.

## How to Build

### Dependencies
* **C++20 Compiler:** `clang-cl` (recommended for Windows), `clang++`, or `g++`

### Windows

Run `build.bat` from the project root:

```cmd
build.bat [project] [compiler] [mode]
```

**Arguments:**
- `project` — `compiler` (default), `lsp`, or `test`
- `compiler` — `clang-cl` (default), `clang++`, or `g++`
- `mode` — `release` (default) or `debug`

**Examples:**
```cmd
build.bat
build.bat compiler clang-cl debug
build.bat test g++ release
```

The output executable will be placed in `./build/` (release) or `./build/debug/`.

### Linux (TODO)

Run `build_linux.sh`:

```bash
./build_linux.sh
```

## Compiler Usage

```bash
compiler <command> <input_file> [options]
```

### Commands

| Command     | Description |
|-------------|-------------|
| `help`      | Print help message |
| `version`   | Print compiler version |
| `check`     | Perform semantic analysis only (no output generated) |
| `translate` | Generate intermediate source files (.c, .bc) |
| `build`     | Compile into a final executable binary |
| `run`       | Compile and immediately execute the program |

### Options

| Option | Description |
|--------|-------------|
| `-t, --target <list>` | Comma-separated list of targets. Default: `vm` |
| `-o, --output <name>` | Base name for output files (without extension). Default: same as input file name |
| `-d, --outdir <path>` | Directory where output files will be saved. Default: `./out` |
| `-g, --debug` | Generate debug information |
| `-O<level>` | Optimization level: 0 (default), 1, 2, 3 |
| `-b, --bat` | Indicate the program is called from a batch script |
| `-h, --help` | Print help message |

### Targets

| Target  | Description | Supports |
|---------|-------------|----------|
| `vm`    | Virtual Machine backend (default) | translate, build, run |
| `c`     | TODO | |
| `llvm`  | TODO | |
| `debug` | Debug AST emitter | translate only |

### Examples

```bash
# Compile and run with the default backend
compiler run main.qv

# Compile and run with llvm backend
compiler run main.qv -t llvm

# Build executable with debug info
compiler build main.qv -g -O2

# Translate to C code only
compiler translate main.qv -t c

# Semantic check only
compiler check main.qv

# Translate to multiple targets
compiler translate main.qv -t llvm, vm, debug

# Custom output directory and name
compiler build main.vi -d ./bin -o myapp
```

> **Note for Windows Users:** For proper UTF-8 and ANSI color escape rendering in `cmd.exe` or PowerShell, set your terminal code page to UTF-8 before running:
> ```cmd
> chcp 65001
> ```

## Documentation

- **Language Reference:** `doc/doc.md` — Language syntax and semantics
- **Bytecode Spec:** `doc/bytecode.md` — VM bytecode instruction set
- **HTML Docs:** `doc/doc.html`, `doc/bytecode.html` — Rendered documentation

## Language Support (outdated)

Editor plugins are available in `./tools/`:
- **VS Code:** `tools/VS-Code plugins/`
- **Neovim:** `tools/Nvim plugins/language-support/`
- **Zed:** `tools/Zed plugins/language-support/`
