cmake_minimum_required(VERSION 3.25)

project(DirectionalMelee VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# CommonLibSSE-NG (SE + AE; VR off to keep the build smaller)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "" FORCE)
add_subdirectory(extern/CommonLibSSE-NG CommonLibSSE EXCLUDE_FROM_ALL)
if(NOT COMMAND add_commonlibsse_plugin)
	include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)
endif()

file(GLOB_RECURSE DM_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)

add_commonlibsse_plugin(
	DirectionalMelee
	NAME "DirectionalMelee"
	USE_ADDRESS_LIBRARY
	SOURCES ${DM_SOURCES}
)

target_include_directories(DirectionalMelee PRIVATE src)
find_package(directxtk CONFIG REQUIRED)
target_link_libraries(DirectionalMelee PRIVATE Microsoft::DirectXTK d3d11.lib d3dcompiler.lib dxgi.lib)
target_precompile_headers(DirectionalMelee PRIVATE src/PCH.h)
target_compile_definitions(DirectionalMelee PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
	target_compile_options(DirectionalMelee PRIVATE /W4 /permissive- /Zc:preprocessor /utf-8 /MP /wd4200)
endif()

# The built plugin, copied to build/dist where the release tools take it from (the file name is from the mod's first versions;
# it is shipped as Data/SKSE/Plugins/MordhauSkyrim.dll).
add_custom_command(TARGET DirectionalMelee POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:DirectionalMelee>" "${CMAKE_BINARY_DIR}/dist/DirectionalMelee.asi"
	VERBATIM)

# Dev: the plugin's own baker (src/bake: reads MORDHAU's files, writes the local cache) as a small test program, to
# compare its output with what the Python tools wrote. Not shipped.
add_executable(mordhau_bake_test tools/bake_test.cpp src/bake/Bake.cpp src/bake/Spec.cpp src/bake/Cache.cpp)
target_link_libraries(mordhau_bake_test PRIVATE advapi32.lib)
target_include_directories(mordhau_bake_test PRIVATE src)
target_compile_definitions(mordhau_bake_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
	target_compile_options(mordhau_bake_test PRIVATE /W4 /permissive- /utf-8)
endif()

# MordhauBones.dll: the MORDHAU-side script's native helper (reads bone transforms; mordhau_side/bones). Built with the
# plugin, copied next to it in build/dist; the release tools put it into the script's folder.
add_library(MordhauBones SHARED ${CMAKE_CURRENT_SOURCE_DIR}/../mordhau_side/bones/MordhauBones.cpp)
target_compile_definitions(MordhauBones PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
	target_compile_options(MordhauBones PRIVATE /W4 /permissive- /utf-8)
endif()
add_custom_command(TARGET MordhauBones POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:MordhauBones>" "${CMAKE_BINARY_DIR}/dist/MordhauBones.dll"
	VERBATIM)
