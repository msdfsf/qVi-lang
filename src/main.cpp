// #pragma once
#include <stdio.h>
#include <string.h>
#include <chrono>

#include "compiler.h"
#include "logger.h"

#define OPTION_SYMBOL '-'

Logger::Type logErr = { .level = Logger::ERROR, .tag = "main" };
Logger::Type logInf = { .level = Logger::INFO, .tag = NULL };
Logger::Type logPln = { .level = Logger::PLAIN, .tag = NULL };

Logger::FlushStream flushStreams[] = {
    {
        .kind = Logger::FlushStream::FS_C_STREAM,
        .cstream = stdout
    }
};

int calledFromBat = 0;

int run ();

int parseArgs(char* argv[], int argc) {

	int atLeastOneLangSet = 0;
	int wasImplicitOption = 0;

	for (int i = 0; i < argc; i++) {

		if (argv[i][0] != OPTION_SYMBOL) {
			if (wasImplicitOption) {
				Logger::log(logErr, "Implicit option was already set!\n");
				return -1;
			}
			wasImplicitOption = 1;

			Compiler::mainFile = argv[i];
			continue;
		}

		char* const option = argv[i] + 1;

		if (!strcmp(option, "ol")) {
			// output lang

			if (i + 1 > argc) {
				Logger::log(logErr, "Argument requaried for option -ol!\nFollowing options are avaliable: c, asm, itself_console\n");
				return -1;
			}

			i++;

			atLeastOneLangSet = 1;
			char* const option = argv[i];

			if (!strcmp(option, "c")) {
				Compiler::outLangs = Compiler::outLangs | Compiler::C_LANG;
				continue;
			}

			if (!strcmp(option, "itself_console")) {
				Compiler::outLangs = Compiler::outLangs | Compiler::ITSELF_CONSOLE_LANG;
				continue;
			}

			Logger::log(logErr, "Unknown language!\nFollowing options are avaliable: c, asm, itself_console\n");

			return -1; // TODO : add error

		} else if (!strcmp(option, "of")) {
			// output file

			if (i + 1 > argc) {
				Logger::log(logErr, "Argument requaried for option -of!\nFile has to be without the extension!\n");
				return -1;
			}

			i++;

			Compiler::outFile = argv[i];


		} else if (!strcmp(option, "od")) {
			// output file

			if (i + 1 > argc) {
				Logger::log(logErr, "Argument requaried for option -od!\n");
				return -1;
			}

			i++;

			Compiler::outDir = argv[i];

		} else if (!strcmp(option, "gd")) {
			// generate debug info

			Compiler::debugInfo = 1;

		} else if (!strcmp(option, "h")) {
			// help

			Logger::log(logInf, "Compiler options:\n");
			Logger::log(logInf, "-ol (output lang):\n");
			Logger::log(logInf, "\toptions: c\n");
			Logger::log(logInf, "\tdefault: c\n");
			Logger::log(logInf, "-of (output file):\n");
			Logger::log(logInf, "\tfilename without the extension the executable will be saved to\n");
			Logger::log(logInf, "-od (output directory):\n");
			Logger::log(logInf, "\tdirname translated files of the language specified in -ol will be saved to\n");
			Logger::log(logInf, "-h (help):\n");
			Logger::log(logInf, "\tprints help\n");

			return -1;

		} else if (!strcmp(option, "b")) {

			calledFromBat = 1;

		} else {

			Logger::log(logErr, "Unknown option!\n");
			return -1;

		}

	}

	if (!atLeastOneLangSet) Compiler::outLangs = Compiler::outLangs | Compiler::C_LANG;

	return 0;

}

int main(int argc, char* argv[]) {

    Logger::flushStreams = flushStreams;
    Logger::flushStreamCount = sizeof(flushStreams) / sizeof(Logger::FlushStream);

	int atLeastOneLangSet = 0;

	if (argc < 2) {
		Logger::log(logPln, "You need to specify command!\n");
		Logger::log(logPln, "There are following commands available: run, build.\n");
		Logger::log(logPln, "To get more information about each command use -h option as follows:\t run -h\n");
		Logger::log(logPln, "To just compile and run the file 'file_name' use:\t run file_name\n");
		return -1;
	}

	char* const command = argv[1];
	if (!strcmp(command, "build")) {
		Compiler::command = Compiler::BUILD;
	} else if (!strcmp(command, "run")) {
		Compiler::command = Compiler::RUN;
	} else if (!strcmp(command, "translate")) {
		Compiler::command = Compiler::TRANSLATE;
	} else {
		Logger::log(logErr, "Unknown command!\n");
		return -1;
	}

	if (parseArgs(argv + 2, argc - 2) < 0) return -1;

	if (!Compiler::outFile) {

		int i = 0;
		int lastDotIdx = -1;
		int lastSlashIdx = -1;
		while (1) {
			const char ch = Compiler::mainFile[i];
			if (ch == '\0') break;
			if (ch == '.') lastDotIdx = i;
			if (ch == '\\' || ch == '/') lastSlashIdx = i;
			i++;
		}

		const int idx = lastSlashIdx < 0 ? 0 : lastSlashIdx + 1;
		const int len = lastDotIdx <= lastSlashIdx ? i - idx : lastDotIdx - idx;

		Compiler::outFile = (char*) malloc(len + 1 + 4);
		memcpy(Compiler::outFile, Compiler::mainFile + idx, len);
		Compiler::outFile[len] = '.';
		Compiler::outFile[len + 1] = 'e';
		Compiler::outFile[len + 2] = 'x';
		Compiler::outFile[len + 3] = 'e';
		Compiler::outFile[len + 4] = '\0';

	} else {

		const int len = strlen(Compiler::outFile);
		char* tmp = (char*) malloc(len + 1 + 4);
		memcpy(tmp, Compiler::outFile, len);
		Compiler::outFile = tmp;
		Compiler::outFile[len] = '.';
		Compiler::outFile[len + 1] = 'e';
		Compiler::outFile[len + 2] = 'x';
		Compiler::outFile[len + 3] = 'e';
		Compiler::outFile[len + 4] = '\0';

	}

	auto cmpStartTime = std::chrono::high_resolution_clock::now();
	if (Compiler::compile() < 0) return -1;
	std::chrono::duration<double, std::milli> cmpElapsedTime = std::chrono::high_resolution_clock::now() - cmpStartTime;

	printf("\nCompilation time was: %.2f ms.\n", cmpElapsedTime.count());

	if (Compiler::command == Compiler::RUN) {

		Logger::log(logInf, "Running the executable...\n");

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
			(LPSTR)(std::string("cmd.exe /k ") + Compiler::outFile).c_str(),              // Command-line arguments
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

// compiler [command] [filename] [args]
// commands:
// 	build
// 	run
//
// args:
// 	-ol (output lang):
//	 options: c
//	 default: c
//
//	-of (output file):
//	 filename without the extension the executable will be saved to
//
// 	-od (output directory):
//	 dirname translated files of the language specified in -ol will be saved to
//
//	-gd (generate debug info):
//	 as tiny C compiler is used, DWARF will be generated
//
// 	-h (help):
//	 prints help
//
//  -b (bat)
//	 indicates that program is called run from the bat file
