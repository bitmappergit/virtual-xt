for _,arg in ipairs(_ARGS) do
    if arg == 'nogfx' then nogfx = true
    elseif arg == 'nosfx' then nosfx = true
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
    else
        files { 'src/libvxt.c' }
        targetname 'vxt'
    end

    if nogfx then defines { 'NO_GRAPHICS' } end
    if nosfx then defines { 'NO_AUDIO' } end

    if not nogfx or not nosfx then
        links { 'SDL2' }
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
