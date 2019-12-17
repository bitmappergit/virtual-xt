@echo off

set PACKAGE_DEST=%TRAVIS_BUILD_DIR%\package\virtualxt
rem if exist %PACKAGE_DEST%\ (
rem     rmdir /q /s %PACKAGE_DEST%
rem )
mkdir %PACKAGE_DEST%

copy virtualxt.exe %PACKAGE_DEST%
xcopy /S /Y /I doc\manual %PACKAGE_DEST%\manual
copy tools\floppies\freedos_itch.img %PACKAGE_DEST%\freedos.img
copy tools\package\itch\windows.itch.toml %PACKAGE_DEST%\.itch.toml

copy SDL2-2.0.10\x86_64-w64-mingw32\bin\SDL2.dll %PACKAGE_DEST%