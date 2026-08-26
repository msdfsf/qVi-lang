:: Builds the project for windows
:: Expetcs the compiler being in the PATH
::
:: Note: as GNU assembler was chosen to provide syntax
:: consistency and cross-platform compatibility, support
:: for the 'cl' compiler has been dropped. Using 'cl' would
:: require a separate tool for assembly, contradicting the
:: whole point of using one prefered compiler as now there
:: are, in a way, two. Instead, 'clang-cl' is supported
:: from the 'microsoft family', allowing a single tool
:: to do the job.
::
:: $1 choose the project ['compiler', 'lsp', 'test']
::    default: 'compiler'
:: $2 choose the compiler ['clang-cl', 'clang++', 'g++']
::    default: any thats avaliable in the PATH
::    in given order
:: $3 choose the mode ['release', 'debug']
::    default: 'release'


@echo off
for /F %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
setlocal EnableDelayedExpansion

:: =============================================
:: CONFIGURATION
:: =============================================

set "TARGET_PROJECT=%~1"
set "TARGET_COMPILER=%~2"
set "TARGET_MODE=%~3"

set "SRC_DIRS=src"
set "LIB_DIR=lib"
set "BUILD_DIR=build"

if "%TARGET_PROJECT%"=="" set "TARGET_PROJECT=compiler"
if "%TARGET_MODE%"=="" set "TARGET_MODE=release"

if /i "%TARGET_PROJECT%"=="compiler" (
    set "OUT_BIN=compiler.exe"
    set "SRC_DIRS=src"
) else if /i "%TARGET_PROJECT%"=="lsp" (
    set "OUT_BIN=qVi-lsp.exe"
    set "SRC_DIRS=lsp/src src"
) else if /i "%TARGET_PROJECT%"=="test" (
    set "OUT_BIN=test.exe"
    set "SRC_DIRS=test src"
) else (
    call :print_error "Invalid target project: '%TARGET_PROJECT%'"
    echo         Available: 'compiler', 'lsp', 'test'
    exit /b 1
)

if /i "%TARGET_MODE%" NEQ "debug" (
    if /i "%TARGET_MODE%" NEQ "release" (
        call :print_error "Invalid target mode, 'debug' or 'release' are available."
        exit /b 1
    )
)

if /i "%TARGET_MODE%"=="debug" (
    set "BUILD_DIR=%BUILD_DIR%\debug"
    set "LIB_DIR=..\%LIB_DIR%"
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"



:: =============================================
:: AUTO-DETECTION (DEFAULTS)
:: =============================================

set "KNOWN_COMPILERS=clang-cl g++ clang++"

if "%TARGET_MODE%"=="" set "TARGET_MODE=release"

if "%TARGET_COMPILER%"=="" (
    for %%c in (%KNOWN_COMPILERS%) do (
        where /q %%c
        if !errorlevel! equ 0 (
            set "TARGET_COMPILER=%%c"
            goto :compiler_found
        )
    )

    call :print_error "No supported compiler found in PATH."
    echo         Checked: %KNOWN_COMPILERS%
    exit /b 1
) else (
    set "FOUND=false"
    for %%c in (%KNOWN_COMPILERS%) do (
        if /i "%TARGET_COMPILER%"=="%%c" set "FOUND=true"
    )

    if "!FOUND!"=="false" (
        call :print_error "Unknown compiler: '%TARGET_COMPILER%'"
        echo         Available: %KNOWN_COMPILERS%
        exit /b 1
    )
)

:compiler_found
call :print_info "Project:  %TARGET_PROJECT%"
call :print_info "Compiler: %TARGET_COMPILER%"
call :print_info "Mode:     %TARGET_MODE%"



:: =============================================
:: FIND ALL SOURCES
:: =============================================
set "SOURCES="
for %%d in (%SRC_DIRS%) do (
    PUSHD "%%d"
    if not errorlevel 1 (

        for /r . %%f in (*.cpp *.c *.s *.asm) do (
            set "SKIP=false"

            :: Skip if a file with the same name was already included
            if defined INCLUDED_FILE[%%~nxf] set "SKIP=true"

            :: Exclude compiler's main files when building test or lsp targets
            if /i "%%~dpf"=="%CD%\src\" (
                if /i "%TARGET_PROJECT%"=="test" set "IS_TARGET=1"
                if /i "%TARGET_PROJECT%"=="lsp"  set "IS_TARGET=1"

                if defined IS_TARGET (
                    if /i "%%~nxf"=="main.cpp"     set "SKIP=true"
                    if /i "%%~nxf"=="compiler.cpp" set "SKIP=true"
                    set "IS_TARGET="
                )
            )

            if "!SKIP!"=="false" (
                set "INCLUDED_FILE[%%~nxf]=1"
                set "SOURCES=!SOURCES! "%%f""
            )
        )

        POPD

    )
)

if "%SOURCES%"=="" (
    call :print_error "No .cpp or .s files found in %SRC_DIR%"
    exit /b 1
)



:: =============================================
:: COMPILATION CONFIGURATION
:: =============================================

if /i "%TARGET_COMPILER%"=="clang-cl" (

    set "FLAGS=/std:c++20 /W0 /wd4530 /D_AMD64_ /DWIN64 /nologo /clang:-fproc-stat-report"
    set "LIBS="

    if /i "%TARGET_PROJECT%"=="test" (
        set "FLAGS=!FLAGS! /DCONFIG_DISABLE_LOGGING /DCONFIG_ERROR_RECOVERY"
    ) else if /i "%TARGET_PROJECT%"=="lsp" (
        set "FLAGS=!FLAGS! /D_CUSTOM_ALLOCATOR_ /DCONFIG_ERROR_RECOVERY"
    )

    if /i "%TARGET_MODE%"=="debug" (
        set "FLAGS=!FLAGS! /Zi /Od /Fe"%OUT_BIN%""
    ) else (
        set "FLAGS=!FLAGS! /O2 /Fe"%OUT_BIN%""
    )
) else (

    set "FLAGS=-std=c++20 -w -I"..\%LIB_DIR%""
    set "LIBS="

    if /i "%TARGET_PROJECT%"=="test" (
        set "FLAGS=!FLAGS! -DCONFIG_DISABLE_LOGGING -DCONFIG_ERROR_RECOVERY"
    ) else if /i "%TARGET_PROJECT%"=="lsp" (
        set "FLAGS=!FLAGS! -D_CUSTOM_ALLOCATOR_ -DCONFIG_ERROR_RECOVERY"
    )

    if /i "%TARGET_MODE%"=="debug" (
        set "FLAGS=!FLAGS! -DNOMINMAX -g -o "%OUT_BIN%""
    ) else (
        set "FLAGS=!FLAGS! -DNOMINMAX -O3 -o "%OUT_BIN%""
    )

)



:: ==============================================
:: EXECUTION
:: ==============================================

pushd "%BUILD_DIR%"

call :print_custom "EXEC" "1;35" "%TARGET_COMPILER% !FLAGS! !SOURCES! !LIBS!"

%TARGET_COMPILER% !FLAGS! !SOURCES! !LIBS!
if !errorlevel! neq 0 (
    popd
    call :print_error "Build failed."
    exit /b !errorlevel!
)

del *.obj >nul 2>&1

popd

call :print_custom "SUCCESS" "1;32" "Build complete:: %BUILD_DIR%\%OUT_BIN%"
exit /b



:: ================================================
:: FUNCTIONS
:: ================================================

:print_error
echo %ESC%[1;31m[ERROR]%ESC%[0m %~1
exit /b

:print_info
echo %ESC%[1;36m[INFO]%ESC%[0m %~1
exit /b

:print_custom
set "TAG=%~1"
set "COL=%~2"
set "MSG=%~3"
echo %ESC%[%COL%m[%TAG%]%ESC%[0m %MSG%
exit /b
