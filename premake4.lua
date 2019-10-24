for _,arg in ipairs(_ARGS) do
    if arg == 'nosdl' then nosdl = true
    else error('Unknown argument: ' .. arg) end
end

function create_project(k)
    kind(k)
    language 'C'
    targetdir ''
    platforms { 'native', 'x32', 'x64' }
    includedirs { 'src' }
    --flags { 'ExtraWarnings', 'FatalWarnings' }

    if k == 'ConsoleApp' then
        files { 'src/virtualxt.c' }
        links { 'libvxt' }
        if nosdl then
            defines { 'NO_SDL' }
        else
            links { 'SDL2' }
        end
    else
        files { 'src/libvxt.c' }
        targetname 'vxt'
    end
end

solution 'VirtualXT'
    configurations { 'Release', 'Debug' }

    configuration 'Release'
        flags { 'Optimize' }

    configuration 'Debug'
        flags { 'Symbols' }

    --configuration 'vs*'
    --    defines { '_CRT_SECURE_NO_WARNINGS' }

    configuration 'gmake'
        buildoptions { '-fsigned-char -std=c99' }

    project 'virtualxt'
        create_project 'ConsoleApp'
        
    project 'libvxt'
        create_project 'StaticLib'
