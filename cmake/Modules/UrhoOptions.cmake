#
# Copyright (c) 2008-2017 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

if (NOT MINI_URHO)
    # Source environment
    if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
        execute_process(COMMAND cmd /c set OUTPUT_VARIABLE ENVIRONMENT)
    else ()
        execute_process(COMMAND env OUTPUT_VARIABLE ENVIRONMENT)
    endif ()
    string(REGEX REPLACE "=[^\n]*\n?" ";" ENVIRONMENT "${ENVIRONMENT}")
    set(IMPORT_URHO3D_VARIABLES_FROM_ENV BUILD_SHARED_LIBS MINI_URHO SWIG_EXECUTABLE SWIG_DIR)
    foreach(key ${ENVIRONMENT})
        list (FIND IMPORT_URHO3D_VARIABLES_FROM_ENV ${key} _index)
        if ("${key}" MATCHES "^(URHO3D_|CMAKE_|ANDROID_).+" OR ${_index} GREATER -1)
            if (NOT DEFINED ${key})
                set (${key} $ENV{${key}} CACHE STRING "" FORCE)
            endif ()
        endif ()
    endforeach()
endif ()

foreach (feature ${URHO3D_FEATURES})
    set (URHO3D_${feature} ON)
endforeach()

include(CMakeDependentOption)

# Set platform variables
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set (LINUX ON CACHE BOOL "" FORCE)
endif ()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsStore")
    set (UWP ON CACHE BOOL "" FORCE)
endif ()

if (ANDROID OR IOS)
    set (MOBILE ON CACHE BOOL "" FORCE)
elseif (APPLE OR "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set (APPLE ON CACHE BOOL "" FORCE)
endif ()

if (APPLE AND NOT IOS)
    set (MACOS ON CACHE BOOL "" FORCE)
endif ()

# Even though UWP can run on desktop, we do not treat it as a desktop platform, because it behavres more like a mobile app.
if ((WIN32 OR LINUX OR MACOS) AND NOT WEB AND NOT MOBILE AND NOT UWP)
    set (DESKTOP ON CACHE BOOL "" FORCE)
endif ()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten" AND NOT WEB)
    # Compatibility with old toolchain.
    set(WEB ON)
    set(EMSCRIPTEN ON)
    set(EMSCRIPTEN_EMCC_VERSION "${CMAKE_C_COMPILER_VERSION}")
    set (EMRUN ${EMSCRIPTEN_ROOT_PATH}/emrun${TOOL_EXT}    CACHE PATH "emrun")
    set (EMPACKAGER python ${EMSCRIPTEN_ROOT_PATH}/tools/file_packager.py CACHE PATH "file_packager.py")
    set (EMBUILDER python ${EMSCRIPTEN_ROOT_PATH}/embuilder.py CACHE PATH "embuilder.py")
endif ()

# Build properties
option(BUILD_SHARED_LIBS                        "Build engine as shared library."       ON)
option(URHO3D_ENABLE_ALL                        "Enable (almost) all engine features."  ON)
option(URHO3D_STATIC_RUNTIME                    "Link to static runtime"               OFF)
if (UWP)
    set (URHO3D_SSE OFF)
    set (URHO3D_GRAPHICS_API D3D11)
else ()
    set (URHO3D_SSE           SSE2 CACHE STRING "Enable SSE instructions")
endif ()
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.16)
    cmake_dependent_option(URHO3D_PCH           "Enable precompiled header"                              OFF                  "NOT WEB;NOT ANDROID"           OFF)
else ()
    set (URHO3D_PCH OFF)
endif ()

# Subsystems
cmake_dependent_option(URHO3D_GLOW               "Lightmapping subsystem enabled"                        ${URHO3D_ENABLE_ALL} "DESKTOP;NOT MINGW"             OFF)
option                (URHO3D_IK                 "Inverse kinematics subsystem enabled"                  ${URHO3D_ENABLE_ALL})
option                (URHO3D_LOGGING            "Enable logging subsystem"                              ${URHO3D_ENABLE_ALL})
option                (URHO3D_NAVIGATION         "Navigation subsystem enabled"                          ${URHO3D_ENABLE_ALL})
cmake_dependent_option(URHO3D_NETWORK            "Networking subsystem enabled"                          ${URHO3D_ENABLE_ALL} "NOT WEB"                       OFF)
option                (URHO3D_PHYSICS            "Physics subsystem enabled"                             ${URHO3D_ENABLE_ALL})
cmake_dependent_option(URHO3D_PROFILING          "Profiler support enabled"                              ${URHO3D_ENABLE_ALL} "NOT WEB;NOT MINGW;NOT UWP"     OFF)
cmake_dependent_option(URHO3D_PROFILING_SYSTRACE "Profiler systrace support enabled"                     OFF                  "URHO3D_PROFILING"              OFF)
option                (URHO3D_SYSTEMUI           "Build SystemUI subsystem"                              ${URHO3D_ENABLE_ALL})
option                (URHO3D_URHO2D             "2D subsystem enabled"                                  ${URHO3D_ENABLE_ALL})
option                (URHO3D_RMLUI              "HTML subset UIs via RmlUI middleware"                  ${URHO3D_ENABLE_ALL})

# Features
set (URHO3D_CSHARP_TOOLS ${URHO3D_CSHARP})
cmake_dependent_option(URHO3D_CSHARP             "Enable C# support"                                     OFF                  "BUILD_SHARED_LIBS;NOT MINGW"   OFF)
if (NOT MINI_URHO)
    # Keep C# tools in minimal build if we requested them. This is a workaround for building swig as a native tool during crosscompiling.
	# Otherwise it would fail because we build tools with -DBUILD_SHARED_LIBS=OFF due to cryptic build errors, and C# is disabled in static builds.
    set (URHO3D_CSHARP_TOOLS ${URHO3D_CSHARP})
endif ()
# Valid values at https://docs.microsoft.com/en-us/dotnet/standard/frameworks
set(URHO3D_NETFX netstandard2.0 CACHE STRING "TargetFramework value for .NET libraries")
set_property(CACHE URHO3D_NETFX PROPERTY STRINGS netstandard2.0 netstandard2.1)
set(URHO3D_NETFX_RUNTIME_VERSION OFF CACHE STRING "Version of runtime to use.")
cmake_dependent_option(URHO3D_FILEWATCHER        "Watch filesystem for resource changes"                 ${URHO3D_ENABLE_ALL} "URHO3D_THREADING;NOT UWP"      OFF)
option                (URHO3D_SPHERICAL_HARMONICS "Use spherical harmonics for ambient lighting"         ON)
option                (URHO3D_HASH_DEBUG         "Enable StringHash name debugging"                      ${URHO3D_ENABLE_ALL}                                    )
option                (URHO3D_MONOLITHIC_HEADER  "Create Urho3DAll.h which includes all engine headers." OFF                                                     )
cmake_dependent_option(URHO3D_MINIDUMPS          "Enable writing minidumps on crash"                     ${URHO3D_ENABLE_ALL} "MSVC;NOT UWP"                  OFF)
cmake_dependent_option(URHO3D_PLUGINS            "Enable plugins"                                        ${URHO3D_ENABLE_ALL} "NOT WEB;NOT UWP"               OFF)
cmake_dependent_option(URHO3D_THREADING          "Enable multithreading"                                 ${URHO3D_ENABLE_ALL} "NOT WEB"                       OFF)
option                (URHO3D_WEBP               "WEBP support enabled"                                  ${URHO3D_ENABLE_ALL}                                    )
# Web
cmake_dependent_option(EMSCRIPTEN_WASM          "Use wasm instead of asm.js"                            ON                   "WEB"                           OFF)
set(EMSCRIPTEN_TOTAL_MEMORY 128 CACHE STRING  "Memory limit in megabytes. Set to 0 for dynamic growth. Must be multiple of 64KB.")

# Misc
cmake_dependent_option(URHO3D_PLAYER            "Build player application"                              ${URHO3D_ENABLE_ALL} "NOT WEB"                       OFF)
cmake_dependent_option(URHO3D_EXTRAS            "Build extra tools"                                     ${URHO3D_ENABLE_ALL} "NOT WEB;NOT MOBILE;NOT UWP"    OFF)
cmake_dependent_option(URHO3D_TOOLS             "Tools enabled"                                         ${URHO3D_ENABLE_ALL} "DESKTOP"                       OFF)
option(URHO3D_SAMPLES                           "Build samples"                                         OFF)
option(URHO3D_DOCS                              "Build documentation."                                  OFF)
cmake_dependent_option(URHO3D_UNITTESTS         "Build unit tests"                                      ${URHO3D_ENABLE_ALL} "NOT WEB;NOT MOBILE;NOT UWP"    OFF)
cmake_dependent_option(URHO3D_MERGE_STATIC_LIBS "Merge third party dependency libs to Urho3D.a"         OFF "NOT BUILD_SHARED_LIBS"                          OFF)
option(URHO3D_NO_EDITOR_PLAYER_EXE              "Do not build editor or player executables."            OFF)
option(URHO3D_CONTAINER_ADAPTERS                "Enable EASTL-to-Urho container adapters for easier porting of legacy code." OFF)
option(URHO3D_SSL                               "Enable OpenSSL support"                                OFF)

if (WIN32)
    set(URHO3D_GRAPHICS_API D3D11 CACHE STRING "Graphics API")
    set_property(CACHE URHO3D_GRAPHICS_API PROPERTY STRINGS D3D9 D3D11 OpenGL)
    option(URHO3D_WIN32_CONSOLE "Show log messages in win32 console"                     OFF)
elseif (IOS OR ANDROID)
    set(URHO3D_GRAPHICS_API GLES2 CACHE STRING "Graphics API")
    set_property(CACHE URHO3D_GRAPHICS_API PROPERTY STRINGS GLES2 GLES3)
else ()
    set(URHO3D_GRAPHICS_API OpenGL)
endif ()
string(TOUPPER "${URHO3D_GRAPHICS_API}" URHO3D_GRAPHICS_API)
set (URHO3D_${URHO3D_GRAPHICS_API} ON)
if (URHO3D_GLES2 OR URHO3D_GLES3)
    set (URHO3D_OPENGL ON)
endif ()

if (URHO3D_CSHARP OR URHO3D_UNITTESTS)
    set (URHO3D_MONOLITHIC_HEADER ON)   # Used by wrapper code
endif ()

# Implicit configuration
if (ANDROID OR WEB OR IOS)
    set (URHO3D_SSE OFF)
    if (NOT URHO3D_GLES3)
        set (URHO3D_SYSTEMUI OFF)
    endif ()
elseif (URHO3D_TOOLS AND NOT MINI_URHO)
    set (URHO3D_SYSTEMUI ON)
    set (URHO3D_FILEWATCHER ON)
    set (URHO3D_LOGGING ON)
    set (URHO3D_HASH_DEBUG ON)
endif ()

if (WEB)
    if (URHO3D_CSHARP)
        message(WARNING "C# is not supported in this configuration.")
        set (URHO3D_CSHARP OFF)
    endif ()
    if (BUILD_SHARED_LIBS)
        set (BUILD_SHARED_LIBS OFF)
        message(WARNING "Shared builds unsupported when compiling with emscripten")     # For now.
    endif ()
    if (NOT URHO3D_PACKAGING)
        # Web builds do not function without data packaging.
        set (URHO3D_PACKAGING ON)
    endif ()
endif ()

if (ANDROID)
    set (SDL_CPUINFO ON)
endif ()

# At the end because it depends on URHO3D_SYSTEMUI which is may be off, but implicitly enabled if URHO3D_TOOLS is enabled.
cmake_dependent_option(URHO3D_SYSTEMUI_VIEWPORTS "Use native viewports in supported applications" ON "URHO3D_SYSTEMUI" OFF)
