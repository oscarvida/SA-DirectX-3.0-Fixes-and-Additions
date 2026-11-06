@echo off
rem Compila los 8 plugins (LucesNocturnasENB solo si estan sus archivos de lluvia: ver src\LucesNocturnasENB\FALTAN-ARCHIVOS-LLUVIA.md)
rem ASI (x86, MSVC, CRT estatico) en la carpeta "salida".
rem Requisitos: Visual Studio 2026 (o 2022) con C++ x86, y las cabeceras D3DX9 del
rem DirectX SDK (junio 2010) para zzENBParametros. Indica su carpeta "include" en DXSDK_INCLUDE.
rem Despues, opcional: python tools\agregar_version.py salida <carpeta-temporal>
rem   (anade la informacion de version y el checksum, como en los binarios de la r2).
setlocal
set "VSVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"
if not exist "%VSVARS%" set "VSVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
call "%VSVARS%" >nul || exit /b 1
if "%DXSDK_INCLUDE%"=="" if not "%DXSDK_DIR%"=="" set "DXSDK_INCLUDE=%DXSDK_DIR%Include"
set "R=%~dp0"
set "O=%R%salida"
set "T=%R%salida\obj"
if not exist "%T%" mkdir "%T%"
cd /d "%T%"
set CL=/nologo /O2 /MT /LD /Fo"%T%\\" /D_CRT_SECURE_NO_WARNINGS

cl /EHsc /W3 "%R%src\enbseries_guardia\ENBGuardiaEfectos.cpp" /link /OUT:"%O%\enbseries_guardia.asi" user32.lib || exit /b 1
cl /EHsc /W4 "%R%src\enbseries_interiores\ENBInteriorLighting.cpp" /link /OUT:"%O%\enbseries_interiores.asi" || exit /b 1
cl /EHsc /W3 "%R%src\ENBShaderCache\ENBShaderCache.cpp" /link /OUT:"%O%\ENBShaderCache.asi" user32.lib || exit /b 1
if exist "%R%src\LucesNocturnasENB\lluvia_tex.h" cl /EHa /W3 "%R%src\LucesNocturnasENB\LucesNocturnasENB.cpp" /link /OUT:"%O%\LucesNocturnasENB.SA.asi" user32.lib d3d9.lib uuid.lib || exit /b 1
cl /std:c++17 /EHa /W4 /WX "%R%src\HumoLucesENB\HumoLucesENB.cpp" /link /OUT:"%O%\HumoLucesENB.SA.asi" user32.lib || exit /b 1
cl /std:c++17 /EHsc /W4 /I"%DXSDK_INCLUDE%" "%R%src\zzENBParametros\ENBParametros.cpp" /link /OUT:"%O%\enbseries_parametros.asi" || exit /b 1
rem El original se enlazo como enbseries_parametros.asi (nombre interno de sus exportaciones) y se distribuye renombrado.
move /y "%O%\enbseries_parametros.asi" "%O%\zzENBParametros.asi" >nul
cl /EHsc /W3 "%R%src\ShadowCasterFix\ShadowCasterFix.cpp" /link /OUT:"%O%\ShadowCasterFix.asi" user32.lib || exit /b 1
cl /std:c++20 /EHsc /W4 "%R%src\RendimientoHiloPrincipal\RendimientoHiloPrincipal.cpp" /link /OUT:"%O%\RendimientoHiloPrincipal.SA.asi" user32.lib psapi.lib || exit /b 1
cd /d "%R%"
echo Listo: %O%
