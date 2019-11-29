@echo off

set PACKAGE_DEST=package\virtualxt
mkdir %PACKAGE_DEST%

copy virtualxt.exe %PACKAGE_DEST%
copy doc\manual %PACKAGE_DEST%
copy SDL2-2.0.10\x86_64-w64-mingw32\bin\SDL2.dll %PACKAGE_DEST%
copy tools\package\itch\freedos.img %PACKAGE_DEST%
copy tools\package\itch\windows.itch.toml %PACKAGE_DEST%\.itch.toml