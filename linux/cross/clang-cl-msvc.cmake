# Cross-compiling the SKSE plugin on Linux: clang-cl and lld-link against Microsoft's SDK and C runtime as xwin lays them
# out (xwin --accept-license splat --output ~/dev/tools/xwin-sdk; XWIN_SDK to put it elsewhere). Used by build-cross.sh,
# and by vcpkg through triplets/x64-windows-static-md.cmake.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(DEFINED ENV{XWIN_SDK})
	set(XWIN "$ENV{XWIN_SDK}")
else()
	set(XWIN "$ENV{HOME}/dev/tools/xwin-sdk")
endif()

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

set(_xwin_includes "/imsvc\"${XWIN}/crt/include\" /imsvc\"${XWIN}/sdk/include/ucrt\" /imsvc\"${XWIN}/sdk/include/um\" /imsvc\"${XWIN}/sdk/include/shared\" /imsvc\"${XWIN}/sdk/include/winrt\"")
set(CMAKE_C_FLAGS_INIT "-fms-compatibility-version=19.40 ${_xwin_includes}")
set(CMAKE_CXX_FLAGS_INIT "-fms-compatibility-version=19.40 ${_xwin_includes}")
set(CMAKE_RC_FLAGS_INIT "/I \"${XWIN}/sdk/include/um\" /I \"${XWIN}/sdk/include/shared\" /I \"${XWIN}/crt/include\" /I \"${XWIN}/sdk/include/ucrt\"")
set(_xwin_libs "/libpath:\"${XWIN}/crt/lib/x86_64\" /libpath:\"${XWIN}/sdk/lib/um/x86_64\" /libpath:\"${XWIN}/sdk/lib/ucrt/x86_64\"")
foreach(_kind EXE SHARED MODULE)
	set(CMAKE_${_kind}_LINKER_FLAGS_INIT "${_xwin_libs}")
endforeach()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# xwin ships no debug C runtime (msvcrtd.lib): the release one (/MD, as CI builds) for every configuration and compiler test
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
