@echo off
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (echo VCVARS_FAILED & exit /b 1)
echo --- clang-cl version ---
"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" --version
echo --- compile ---
"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe" /nologo /std:c++20 /EHsc /W4 /WX /D_CRT_SECURE_NO_WARNINGS "%~dp0depth_bounds_check.cpp" /Fe:"%~dp0depth_bounds_check.exe" /Fo:"%~dp0depth_bounds_check.obj"
echo COMPILE_EXIT=%ERRORLEVEL%
echo --- run ---
"%~dp0depth_bounds_check.exe"
echo RUN_EXIT=%ERRORLEVEL%
