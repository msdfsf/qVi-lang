@echo off
setlocal

:: Change to repository root
cd /d "%~dp0..\.."

:: Configuration
set "EBNFGEN=D:\ebnf_test_gen\ebnfgen.exe"
set "GRAMMAR=doc\grammar.ebnf"
set "CONFIG=test\bench\ebnfgen.cfg"
set "OUT_DIR=test\bench"

set "BIG_FILE=%OUT_DIR%\big_random_file_200000.qvi"
set "NUM_FILE=%OUT_DIR%\numbers_all_32000.qvi"
set "STR_FILE=%OUT_DIR%\strings_all_120000.qvi"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

:: Clean up any old generated files
del /q "%OUT_DIR%\big_random_file_*.qvi" "%OUT_DIR%\numbers_all_*.qvi" "%OUT_DIR%\strings_all_*.qvi" 2>nul

:: Base command with common parameters
:: TODO: make config for each file
set "GEN="%EBNFGEN%" "%GRAMMAR%" "%CONFIG%" -b 200 -m 64 -i 4"

echo [bench_gen] Generating Big Random File...
%GEN% -r TopLevelItem -n 200000 -s 1337 -o "%BIG_FILE%" || goto fail

echo [bench_gen] Generating Numbers...
%GEN% -r Number -n 32000 -t 1 -s 1338 -o "%NUM_FILE%" || goto fail

echo [bench_gen] Generating Strings...
%GEN% -r StringLiteral   -n 40000 -t 1 -s 1339 -o "%OUT_DIR%\s1.tmp" || goto fail
:: Use the bare token form so strings_all.qvi still has one token per program.
%GEN% -r StringLiteralInfer -n 40000 -t 1 -s 1340 -o "%OUT_DIR%\s2.tmp" || goto fail
%GEN% -r CharLiteral     -n 40000 -t 1 -s 1341 -o "%OUT_DIR%\s3.tmp" || goto fail

:: Combine strings and cleanup temp files
copy /b "%OUT_DIR%\s1.tmp"+"%OUT_DIR%\s2.tmp"+"%OUT_DIR%\s3.tmp" "%STR_FILE%" >nul
del /q "%OUT_DIR%\s*.tmp" 2>nul

echo.
echo [bench_gen] Generation complete:
dir /b /s "%BIG_FILE%" "%NUM_FILE%" "%STR_FILE%"
exit /b 0

:fail
echo [bench_gen] Generation failed! 1>&2
del /q "%OUT_DIR%\s*.tmp" 2>nul
exit /b 1
