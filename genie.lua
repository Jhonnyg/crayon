local host_platform = os.get()

function linux_toolchain()
    premake.gcc.cc  = "clang-6.0"
    premake.gcc.cxx = "clang++-6.0"
    premake.gcc.ar  = "ar"

    buildoptions{ "-Wno-unused-parameter" }
end

solution "crayon"
    language "C++"

    configurations { "Debug", "Release" }

    if host_platform == "linux" then
        platforms {"x64"}
        linux_toolchain()
    end

    location ( "build" )

    flags {
        "FatalWarnings",
        "NoPCH"
    }

    includedirs { path.join("src"), path.join("libs","include") }
    libdirs     { path.join("libs","x86_64-linux") }

    configuration "Debug"
        defines { "DEBUG" }
        flags   { "Symbols" }

    configuration "Release"
        defines { "NDEBUG" }
        flags   { "Optimize" }

project ("crayon_simple")
    kind        ( "ConsoleApp" )
    files       { path.join("simple","main.cpp") }
    includedirs { path.join("simple"), path.join("deps") }
    targetdir   ( path.join("build") )
