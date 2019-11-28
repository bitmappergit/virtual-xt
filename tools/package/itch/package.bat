@echo off

mkdir package\virtualxt
copy virtualxt.exe package\virtualxt\
copy SDL2-2.0.10\x86_64-w64-mingw32\bin\SDL2.dll package\virtualxt\
copy tools\package\itch\freedos.img package\virtualxt\
copy tools\package\itch\windows.itch.toml package\virtualxt\.itch.toml