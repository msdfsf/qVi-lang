// #pragma once
#include <stdio.h>
#include <string.h>
#include <chrono>

#include "compiler.h"
#include "io.h"
#include "logger.h"

#define OPTION_SYMBOL '-'

enum Command : uint8_t {
    CMD_HELP,
    CMD_VERSION,
    CMD_CHECK,     // Semantic check only
    CMD_TRANSLATE, // Generate source
    CMD_BUILD,     // Generate binary
    CMD_RUN,       // Execute
};

enum Option {
    UNKNOWN,      // Unrecognized argument
    TARGET,       // -t, --target
    OUTPUT_FILE,  // -o, --output
    OUTPUT_DIR,   // -d, --outdir
    DEBUG_INFO,   // -g, --debug
    OPT_LEVEL,    // -O<number>
    BAT_MODE      // -b, --bat
};



static Logger::Type logErr = { .level = Logger::ERROR, .tag = "main" };
static Logger::Type logWrn = { .level = Logger::WARNING, .tag = "main" };
static Logger::Type logPln = { .level = Logger::PLAIN, .tag = NULL };

IO::Stream flushStreams[] = {
    {
        .kind = IO::Stream::SK_C_STREAM,
        .cstream = stdout
    }
};

static bool calledFromBat = false;

int run();



const char* str(Command cmd) {
    switch (cmd) {
        case CMD_HELP:      return "help";
        case CMD_VERSION:   return "version";
        case CMD_CHECK:     return "check";
        case CMD_TRANSLATE: return "translate";
        case CMD_BUILD:     return "build";
        case CMD_RUN:       return "run";
        default:            return "unknown";
    }
}

const char* getCommandHint(Command cmd) {
    switch (cmd) {
        case CMD_HELP:      return "Print this help message.";
        case CMD_VERSION:   return "Print compiler version.";
        case CMD_CHECK:     return "Perform semantic analysis.";
        case CMD_TRANSLATE: return "Generate intermediate source files (.c, .bc).";
        case CMD_BUILD:     return "Compile into a final executable binary.";
        case CMD_RUN:       return "Compile and immediately execute the program.";
        default:            return "";
    }
}

Command toCommand(const char* arg, bool* isValid) {
    *isValid = true;
    if (!strcmp(arg, "help"))      return CMD_HELP;
    if (!strcmp(arg, "version"))   return CMD_VERSION;
    if (!strcmp(arg, "check"))     return CMD_CHECK;
    if (!strcmp(arg, "translate")) return CMD_TRANSLATE;
    if (!strcmp(arg, "build"))     return CMD_BUILD;
    if (!strcmp(arg, "run"))       return CMD_RUN;

    *isValid = false;
    return CMD_HELP;
}

const char* str(Option opt) {
    switch (opt) {
        case Option::TARGET:      return "-t/--target";
        case Option::OUTPUT_FILE: return "-o/--output";
        case Option::OUTPUT_DIR:  return "-d/--outdir";
        case Option::DEBUG_INFO:  return "-g/--debug";
        case Option::OPT_LEVEL:   return "-O<level>";
        case Option::BAT_MODE:    return "-b/--bat";
        case Option::UNKNOWN:
        default:                  return "Unknown Option";
    }
}

const char* getOptionArgsHint(Option opt) {
    switch (opt) {
        case Option::TARGET:
            return "Comma-separated list of targets. Default: c\n"
                   "Available: c, vm, debug";

        case Option::OUTPUT_FILE:
            return "Base name for output files (without extension).\n"
                   "Default: same as input file name.";

        case Option::OUTPUT_DIR:
            return "Directory where output files will be saved.\n"
                   "Default: './out'.";

        case Option::DEBUG_INFO:
            return "Generate debug information.";

        case Option::OPT_LEVEL:
            return "Optimization level: 0 (default), 1, 2, 3.";

        case Option::BAT_MODE:
            return "Indicate the program is called from a batch script.";

        default:
            return "";
    }
}

Option toOption(const char* arg) {
    if (!arg || arg[0] != '-') return Option::UNKNOWN;

    // Skip the leading dashes
    const char* flag = arg + 1;
    if (flag[0] == '-') flag++;

    if (flag[0] == 'O' && flag[1] >= '0' && flag[1] <= '3') {
        return Option::OPT_LEVEL;
    }

    if (strcmp(flag, "t") == 0 || strcmp(flag, "target") == 0) return Option::TARGET;
    if (strcmp(flag, "o") == 0 || strcmp(flag, "output") == 0) return Option::OUTPUT_FILE;
    if (strcmp(flag, "d") == 0 || strcmp(flag, "outdir") == 0) return Option::OUTPUT_DIR;
    if (strcmp(flag, "g") == 0 || strcmp(flag, "debug") == 0)  return Option::DEBUG_INFO;
    if (strcmp(flag, "b") == 0 || strcmp(flag, "bat") == 0)    return Option::BAT_MODE;

    return Option::UNKNOWN;
}



void printIndented(const char* str, int indent) {
    int i = 0;
    int s = 0;
    while (1) {
        const char ch = str[i];
        if (ch == '\0' || ch == '\n') {
            printf("%*c%.*s\n", indent, ' ', i - s, str + s);
            s = i + 1;
            if (ch == '\0') return;
        }
        i++;
    }
}

void printHelp() {
    Logger::log(logPln, "Usage: compiler <command> <input_file> [options]\n");

    Logger::log(logPln, "Commands:");

    Command allCommands[] = {
        CMD_HELP,
        CMD_VERSION,
        CMD_CHECK,
        CMD_TRANSLATE,
        CMD_BUILD,
        CMD_RUN
    };

    for (Command cmd : allCommands) {
        Logger::log(logPln, "  %-12s %s", NULL, str(cmd), getCommandHint(cmd));
    }

    Logger::log(logPln, "\nOptions:");

    Option allOptions[] = {
        Option::TARGET,
        Option::OUTPUT_FILE,
        Option::OUTPUT_DIR,
        Option::DEBUG_INFO,
        Option::OPT_LEVEL,
        Option::BAT_MODE
    };

    for (Option opt : allOptions) {
        const char* name = str(opt);
        Logger::log(logPln, "  %s", NULL, name);
        printIndented(getOptionArgsHint(opt),  14);
    }
}

void printVersion() {
    Logger::log(logPln, "version");
}

void parseTargetList(const char* listStr) {
    uint32_t targetCount = 0;

    char buffer[256];
    strncpy(buffer, listStr, sizeof(buffer));

    char* token = strtok(buffer, ",");
    while (token != NULL && targetCount < Compiler::TK_COUNT) {
        if (strcmp(token, "c") == 0) {
            Compiler::targets[targetCount++] = (Compiler::Target*)
                Compiler::bakedTargets + Compiler::TK_C_LANG;
        } else if (strcmp(token, "vm") == 0) {
            Compiler::targets[targetCount++] = (Compiler::Target*)
                Compiler::bakedTargets + Compiler::TK_VM;
        } else if (strcmp(token, "debug") == 0) {
            Compiler::targets[targetCount++] = (Compiler::Target*)
                Compiler::bakedTargets + Compiler::TK_DEBUG;
        } else {
            Logger::log(logErr, "Error: Unknown target '%s'\n", NULL, token);
            exit(1);
        }

        token = strtok(NULL, ",");
    }

    Compiler::targets[targetCount] = NULL;
}

bool parseArgs(char* argv[], int argc) {
	bool targetSpecified = false;

	Option errorOption = Option::UNKNOWN;
	for (int i = 0; i < argc; i++) {
        const char* arg = argv[i];

        Option option = toOption(arg);
        switch (option) {
            case Option::DEBUG_INFO:
                Compiler::debugInfo = true;
                break;

            case Option::BAT_MODE:
                calledFromBat = true;
                break;

            case Option::OPT_LEVEL:
                Compiler::optLevel = atoi(arg + 2);
                break;

            case Option::TARGET:
                if (i + 1 < argc) {
                    parseTargetList(argv[++i]);
                    targetSpecified = true;
                } else {
                    errorOption = option;
                }
                break;

            case Option::OUTPUT_FILE:
                if (i + 1 < argc) {
                    Compiler::outFile = String(argv[++i]);
                } else {
                    errorOption = option;
                }
                break;

            case Option::OUTPUT_DIR:
                if (i + 1 < argc) {
                    Compiler::outDir = String(argv[++i]);
                } else {
                    errorOption = option;
                }
                break;

            case Option::UNKNOWN:
            default:
                Logger::log(logWrn, "Warning: Unknown argument '%s' ignored.", NULL, arg);
                break;
        }

        if (errorOption != Option::UNKNOWN) {
            break;
        }
	}

	if (errorOption != Option::UNKNOWN) {
        Logger::log(logErr, "Error: Missing argument for option '%s'.", NULL, str(errorOption));
        Logger::log(logPln, "Hint: %s\n", NULL, getOptionArgsHint(errorOption));

        std::exit(1);
    }

	if (!targetSpecified) {
	    Compiler::targets[0] = (Compiler::Target*)
			Compiler::bakedTargets + Compiler::TK_VM;
	}

	return true;
}

bool requiresInputFile(Command command) {
    return command == CMD_CHECK ||
           command == CMD_TRANSLATE ||
           command == CMD_BUILD ||
           command == CMD_RUN;
}

int main(int argc, char* argv[]) {
    Logger::flushStreams = flushStreams;
    Logger::flushStreamCount = sizeof(flushStreams) / sizeof(IO::Stream);

	if (argc < 2) {
        Logger::log(logErr, "No command specified.");
        printHelp();
        return -1;
    }

	bool isCommandValid = false;
    Command command = toCommand(argv[1], &isCommandValid);

    if (!isCommandValid) {
        Logger::log(logErr, "Unknown command '%s'.", NULL, argv[1]);
        Logger::log(logPln, "Use 'help' command for a list of available commands.");
        return -1;
    }

    if (requiresInputFile(command)) {
        if (argc < 3) {
            Logger::log(logErr, "Error: Missing input file.");
            Logger::log(logPln, "A build command requires a source file to compile.");
            Logger::log(logPln, "Use 'help' command for more information.");

            std::exit(1);
        }
        Compiler::mainFile = String(argv[argc - 1]);
    }

	if (!parseArgs(argv + 3, argc - 3)) {
	    return -1;
	}

	switch(command) {
		case CMD_CHECK: {
    		Compiler::command = Compiler::BC_VALIDATE;
    		break;
		}

		case CMD_TRANSLATE: {
            Compiler::command = Compiler::BC_TRANSLATE;
            break;
		}

	    case CMD_BUILD: {
			Compiler::command = Compiler::BC_BUILD;
			break;
		}

		case CMD_RUN: {
            Compiler::command = Compiler::BC_RUN;
            break;
		}

		case CMD_HELP: {
		    printHelp();
            return 0;
		}

		case CMD_VERSION: {
		    printVersion();
            return 0;
		}
	}

	if (Compiler::outFile.len == 0) {
		int i = 0;
		int lastDotIdx = -1;
		int lastSlashIdx = -1;
		while (1) {
			const char ch = Compiler::mainFile.buff[i];
			if (ch == '\0') break;
			if (ch == '.') lastDotIdx = i;
			if (ch == '\\' || ch == '/') lastSlashIdx = i;
			i++;
		}

		const int idx = lastSlashIdx < 0 ? 0 : lastSlashIdx + 1;
		const int len = lastDotIdx <= lastSlashIdx ? i - idx : lastDotIdx - idx;

		Compiler::outFile.buff = (char*) malloc(len + 1);
		Compiler::outFile.len = len;

		memcpy(Compiler::outFile.buff, Compiler::mainFile.buff + idx, len);
	} else {
		const int len = strlen(Compiler::outFile);
		char* tmp = (char*) malloc(len + 1 + 4);
		memcpy(tmp, Compiler::outFile, len);

		Compiler::outFile.buff = tmp;
		Compiler::outFile.len = len;
	}

	auto cmpStartTime = std::chrono::high_resolution_clock::now();
	if (Compiler::compile() < 0) return -1;
	std::chrono::duration<double, std::milli> cmpElapsedTime = std::chrono::high_resolution_clock::now() - cmpStartTime;

	Logger::log(logPln, "\nCompilation time was: %.2f ms.", NULL, cmpElapsedTime.count());

	if (Compiler::command == Compiler::BC_RUN) {
	    if (Compiler::targets[0]->kind == Compiler::TK_VM) {
			return 0;
		}

		Logger::log(logPln, "Running the executable...\n");

		if (calledFromBat) {
			return 14;
		}

		if(run() < 0) {
			Logger::log(logErr, "Creation of new process failed!\n");
			return -1;
		}
	}

	return 0;
}



#ifdef _WIN32
	#include <iostream>
	#include <windows.h>
#else
	#include <unistd.h>
#endif

int run () {
	#ifdef _WIN32

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
    	ZeroMemory(&pi, sizeof(pi));

		//HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    	//DWORD mode;
    	//GetConsoleMode(hConsole, &mode);
    	//SetConsoleMode(hConsole, mode & ~ENABLE_PROCESSED_OUTPUT);

		if (CreateProcessA(
			NULL,//Compiler::outFile, // Program to execute
			(LPSTR)(std::string("cmd.exe /k ") + std::string(Compiler::outFile)).c_str(),              // Command-line arguments
			NULL,              // Process security attributes
			NULL,              // Thread security attributes
			FALSE,             // Inherit handles
			CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE,			   //CREATE_NEW_CONSOLE, // Create a new console window
			NULL,              // Environment variables
			NULL,              // Working directory
			&si,               // STARTUPINFO
			&pi                // PROCESS_INFORMATION
		)) {

			//AttachConsole(pi.dwProcessId);

			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);

			ExitProcess(0);

		} else {

			std::cout << "CreateProcess failed. Error: " << GetLastError() << std::endl;
			return -1;
		}
	#else
		execl(Compiler::outFile, Compiler::outFile, NULL);
		return -1;
	#endif
}


// Usage: compiler <command> <input_file> [options]
//
// Commands:
//   check        Only run semantic analysis (no output generated).
//   translate    Generate intermediate source files (e.g., .c, .bc).
//   build        Compile to a final executable/binary.
//   run          Compile and immediately execute the program.
//
// Options:
//   -t, --target <list>   Comma-separated list of targets. Default: c
//                         Available: c, vm, debug
//
//   -o, --output <name>   Base name for output files (without extension).
//                         Default: same as input file name.
//
//   -d, --outdir <path>   Directory where output files will be saved.
//                         Default: current working directory (./).
//
//   -g, --debug           Generate debug information (verbose source output, symbols).
//   -O<level>             Optimization level: 0 (default), 1, 2, 3.
//
//   -h, --help            Print this help message.
//   -b, --bat             Indicate the program is called from a batch script
//
