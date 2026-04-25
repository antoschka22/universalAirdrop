@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d %~dp0
mkdir build 2>nul
cd build
"C:\Program Files\CMake\bin\cmake.exe" .. -DBUILD_GUI=ON -DBUILD_TESTS=OFF -DCMAKE_PREFIX_PATH=C:/Qt/6.8.2/msvc2022_64 -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64" -G "Visual Studio 17 2022"
if %errorlevel% neq 0 exit /b %errorlevel%
"C:\Program Files\CMake\bin\cmake.exe" --build . --config Release
