for _,arg in ipairs(_ARGS) do
    if arg == '--wall' then wall = true
    else error('Unknown argument: ' .. arg) end
end

version = os.getenv('VXT_VERSION')
if not version then
    version = '0.0.1'
end

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
        links { 'libvxt' }

        if os.is('windows') then
            links { 'comdlg32' }
            links { 'SDL2' }
        elseif os.is('macosx') then
            links { 'Foundation.framework', 'AppKit.framework', 'SDL2.framework' }
            files { 'src/nfd_osx/nfd_common.c', 'src/nfd_osx/nfd_cocoa.m' }
            includedirs { 'src/nfd_osx' }
        else
            links { 'SDL2' }
        end
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
        <string>%s, Copyright © 2019 Andreas T Jonsson</string>
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
        <string>Copyright © 2019 Andreas T Jonsson, GNU General Public License.</string>
        <key>LSMinimumSystemVersion</key>
        <string>10.4</string>
    </dict>
</plist>]]

    local fp = io.open('tools/package/itch/Info.plist', 'w')
    fp:write(string.format(plist, version, version))
    io.close(fp)
end

write_version()
if os.is('macosx') then
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

    configuration 'gmake'
        buildoptions { '-fsigned-char -std=gnu99 -Wno-unused-result -Wno-unused-value -fno-strict-aliasing' }

    project 'virtualxt'
        create_project 'ConsoleApp'
        
    project 'libvxt'
        create_project 'StaticLib'
