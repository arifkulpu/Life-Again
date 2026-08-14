@echo off
echo === Life Again SKSE Plugin Build Script ===
echo.

:: Visual Studio 2022 gelistirici ortamini bul ve yukle
set VSINSTALL=C:\Program Files\Microsoft Visual Studio\18\Community
if not exist "%VSINSTALL%" (
    set VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Community
)
if not exist "%VSINSTALL%" (
    set VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\BuildTools
)

if not exist "%VSINSTALL%" (
    echo HATA: Visual Studio bulunamadi!
    echo Lutfen C:\Program Files\Microsoft Visual Studio\2022\Community yolunu kontrol edin.
    pause
    exit /b 1
)

echo Visual Studio bulundu: %VSINSTALL%
echo.

:: MSVC gelistirici ortamini aktiflestirir
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo --- CMake Configure (Release) ---
cmake --preset release
if errorlevel 1 (
    echo.
    echo HATA: CMake configure basarisiz!
    pause
    exit /b 1
)

echo.
echo --- Build ---
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo HATA: Build basarisiz!
    pause
    exit /b 1
)

echo.
echo ======================================
echo  Build BASARILI!
echo  DLL: build\Release\LifeAgain.dll
echo  ya da build\LifeAgain.dll
echo ======================================
echo.
echo Kopyalamak icin: Data\SKSE\Plugins\LifeAgain.dll
pause
