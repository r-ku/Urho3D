#
# Copyright (c) 2008-2020 the Urho3D project.
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

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    # Enable parallel builds
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif ()

# When crosscompiling, we need several tools built for host system so various build tasks can be completed.
if ((CMAKE_CROSSCOMPILING OR UWP) AND (URHO3D_PACKAGING OR URHO3D_CSHARP))
    include (ExternalProject)
    if (IOS OR TVOS)
        # When cross-compiling for iOS/tvOS the host environment has been altered by xcodebuild for the said platform, the following fix is required to reset the host environment before spawning another process to configure/generate project file for external project
        # Also workaround a known CMake/Xcode generator bug which prevents it from installing native tool binaries correctly
        set (ALTERNATE_COMMAND /usr/bin/env -i PATH=$ENV{PATH} CC=${SAVED_CC} CXX=${SAVED_CXX} CI=$ENV{CI} ${CMAKE_COMMAND} BUILD_COMMAND bash -c "sed -i '' 's/\$$\(EFFECTIVE_PLATFORM_NAME\)//g' CMakeScripts/install_postBuildPhase.make*")
    else ()
        set (ALTERNATE_COMMAND ${CMAKE_COMMAND} -E env CC=${SAVED_CC} CXX=${SAVED_CXX} CI=$ENV{CI} ${CMAKE_COMMAND})
    endif ()
    # When we are building SDK, we need these tools in it, so macros from UrhoCommon.cmake work.
    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set (URHO3D_NATIVE_PREFIX ${CMAKE_BINARY_DIR}/native)
    else ()
        set (URHO3D_NATIVE_PREFIX ${CMAKE_INSTALL_PREFIX})
    endif ()
	set (_CMAKE_GENERATOR_PLATFORM ${CMAKE_GENERATOR_PLATFORM})
	if (MSVC_IDE)
		if ("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "AMD64")
			set (CMAKE_GENERATOR_PLATFORM x64)
		elseif ("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "X86")	# TODO: Needs to be verified.
			set (CMAKE_GENERATOR_PLATFORM Win32)
		else ()
			set (CMAKE_GENERATOR_PLATFORM "${CMAKE_HOST_SYSTEM_PROCESSOR}")
		endif ()
	endif ()
    ExternalProject_Add (Urho3D-Native
        SOURCE_DIR ${rbfx_SOURCE_DIR}
        CMAKE_COMMAND ${ALTERNATE_COMMAND}
        CMAKE_ARGS -DURHO3D_ENABLE_ALL=OFF -DMINI_URHO=ON -DURHO3D_TOOLS=ON -DBUILD_SHARED_LIBS=OFF
                   -DURHO3D_SYSTEMUI=OFF -DURHO3D_CSHARP=${URHO3D_CSHARP} -DCMAKE_INSTALL_PREFIX=${URHO3D_NATIVE_PREFIX}
				   ${EXTRA_GENERATOR_FLAGS}
    )
	set (CMAKE_GENERATOR_PLATFORM "${_CMAKE_GENERATOR_PLATFORM}")
	unset (_CMAKE_GENERATOR_PLATFORM)
    set (PACKAGE_TOOL    "${URHO3D_NATIVE_PREFIX}/${DEST_BIN_DIR_CONFIG}/PackageTool" CACHE STRING "" FORCE)
    set (SWIG_EXECUTABLE "${URHO3D_NATIVE_PREFIX}/${DEST_BIN_DIR_CONFIG}/swig"        CACHE STRING "" FORCE)
else ()
    set (PACKAGE_TOOL    "$<TARGET_FILE:PackageTool>"                                 CACHE STRING "" FORCE)
    set (SWIG_EXECUTABLE "$<TARGET_FILE:swig>"                                        CACHE STRING "" FORCE)
endif ()

add_subdirectory (ThirdParty)
add_subdirectory (Urho3D)
add_subdirectory (Tools)
add_subdirectory (Extras)
add_subdirectory (Samples)
add_subdirectory (Player)

# Urho3D tests
if (URHO3D_UNITTESTS)
    add_subdirectory (Tests)
endif ()

if (NOT MINI_URHO)
    if (URHO3D_CSHARP AND NOT CMAKE_VS_MSBUILD_COMMAND)
        # Generate a VS solution for C# projects when outside of visual studio.
        vs_generate_sln(${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln ${RBFX_CSPROJ_LIST})

        if (LINUX)
            # On linux instance of dotnet may hang indefinitely which prevents CMake from detecting build completion and hangs the build.
            set (DOTNET_LINUX_MSBUILD_WORKAROUND COMMAND ${rbfx_SOURCE_DIR}/script/.kill_dangling_dotnet.sh ${DOTNET})
        elseif (MACOS)
            # On MacOS Xcode pollutes environment which causes MSBuild to compile all managed targets into Urho3DNet assembly and compilation fails because file contents is not what next build step expects.
            set (DOTNET_MACOS_MSBUILD_WORKAROUND env -u TARGET_NAME -u TARGETNAME)
        endif ()
        add_custom_target(Urho3DNet ALL
            COMMAND ${DOTNET_MACOS_MSBUILD_WORKAROUND} ${DOTNET} build ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.sln
                -restore -noLogo -interactive:false -nr:false -p:CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}/
                -consoleLoggerParameters:ErrorsOnly -p:Platform=${URHO3D_PLATFORM} -p:Configuration=${URHO3D_CONFIG}
                ${DOTNET_LINUX_MSBUILD_WORKAROUND}
            DEPENDS Urho3D)
        foreach (TARGET libEditor libPlayer)
            if (TARGET ${TARGET})
                add_dependencies(Urho3DNet ${TARGET})
            endif ()
        endforeach ()
    endif ()
    install(EXPORT Urho3D DESTINATION ${DEST_SHARE_DIR}/CMake)
endif ()
