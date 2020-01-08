for _,arg in ipairs(_ARGS) do
    if arg == '--wall' then wall = true
    elseif arg == '--emscripten' then emscripten = true
    else error('Unknown argument: ' .. arg) end
end

version = os.getenv('VXT_VERSION')
if not version then
    version = '0.0.1'
end

sdl2_path = os.getenv('SDL2')

function create_project(k)
    kind(k)
    language 'C'
    targetdir ''
    includedirs { 'include' }
    if wall then flags { 'ExtraWarnings'} end

    if os.is('macosx') then
        platforms { 'native', 'x64' }
    else
        platforms { 'native', 'x32', 'x64' }
    end
    
    if k == 'ConsoleApp' then
        files { 'src/virtualxt.c' }

        if emscripten then
            files { 'src/vxt.c' }
        else
            links { 'libvxt' }
        end

        if os.is('windows') then
            links { 'comdlg32' }
            files { 'tools/package/itch/icon.rc' }
        elseif os.is('macosx') then
            links { 'Foundation.framework', 'AppKit.framework' }
            files { 'src/nfd/nfd_common.c', 'src/nfd/nfd_cocoa.m' }
            includedirs { 'src/nfd' }
        end

        links { 'SDL2' }
    else
        files { 'src/vxt.c' }
        targetname 'vxt'
    end
end

function write_version()
    local fp = io.open('src/version.h', 'w')
    fp:write(string.format('#define VERSION_STRING "%s"', version))
    io.close(fp)
end

function create_rc()
    local fp = io.open('tools/package/itch/icon.rc', 'w')
    fp:write('1 ICON "doc/icon/icon.ico"')
    io.close(fp)
end

function create_app()
    local plist = [[<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
    <dict>
        <key>CFBundleDevelopmentRegion</key>
        <string>English</string>
        <key>CFBundleExecutable</key>
        <string>virtualxt</string>
        <key>CFBundleGetInfoString</key>
        <string>%s, Copyright © 2019-2020 Andreas T Jonsson</string>
        <key>CFBundleIconFile</key>
        <string>icon.icns</string>
        <key>CFBundleIdentifier</key>
        <string>org.virtualxt.VirtualXT</string>
        <key>CFBundleInfoDictionaryVersion</key>
        <string>6.0</string>
        <key>CFBundleName</key>
        <string>VirtualXT</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <!-- <key>CFBundleShortVersionString</key><string>0.0</string> -->
        <!-- <key>CFBundleSignature</key><string>????</string> -->
        <key>CFBundleVersion</key>
        <string>%s</string>
        <key>NSHumanReadableCopyright</key>
        <string>Copyright © 2019-2020 Andreas T Jonsson, GNU General Public License.</string>
        <key>LSMinimumSystemVersion</key>
        <string>10.4</string>
    </dict>
</plist>]]

    local fp = io.open('tools/package/itch/Info.plist', 'w')
    fp:write(string.format(plist, version, version))
    io.close(fp)
end

write_version()
if os.is('windows') then
    create_rc()
elseif os.is('macosx') then
    create_app()
end

solution 'VirtualXT'
    configurations { 'Release', 'Debug' }

    configuration 'Release'
        flags { 'Optimize' }
        defines { 'NDEBUG' }

    configuration 'Debug'
        flags { 'Symbols' }

    configuration 'vs*'
        defines { '_CRT_SECURE_NO_WARNINGS' }
        links { 'SDL2main' }
        
        if sdl2_path then
            includedirs { sdl2_path .. '/include' }
            libdirs { sdl2_path .. '/lib/x64' }
        end

    configuration 'gmake'
        buildoptions { '-fsigned-char -std=gnu99 -Wno-unused-result -Wno-unused-value -fno-strict-aliasing' }

    if emscripten then
        buildoptions { '-s USE_SDL=2' }
        linkoptions { '--embed-file tools/floppies/freedos.img@boot.img' }
    end

    project 'virtualxt'
        create_project 'ConsoleApp'
        
    if emscripten then
        targetname 'index.html'
    else
        project 'libvxt'
            create_project 'StaticLib'
    end
