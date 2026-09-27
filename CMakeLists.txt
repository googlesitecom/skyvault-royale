# ============================================================================
#  SKYVAULT Royale - Battle Royale 3D con motor propio (C++ / OpenGL)
#  Build system: CMake >= 3.16 (probado con CMake 4.4, GCC 14, Linux)
#
#  Targets:
#    sv_engine  -> motor estatico (engine/)
#    skyvault   -> el juego (game/)
#    sv_tests   -> tests unitarios CPU (tests/)
#    texgen     -> herramienta offline de horneado de texturas (tools/)
#
#  Uso (Linux):
#    cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#          -DCMAKE_INCLUDE_PATH=<x11 includes> -DCMAKE_LIBRARY_PATH=<x11 libs>
#    cmake --build build -j
#
#  Uso (Windows / MSVC):
#    cmake -B build -G "Visual Studio 17 2022"
#    cmake --build build --config Release
#
#  Uso (Web / Emscripten -> WebAssembly + WebGL2):
#    emcmake cmake -B build-web -DSKYVAULT_WEB=ON
#    cmake --build build-web -j
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(SkyVault VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# CMake 4.x elimino la compatibilidad con proyectos que piden < 3.5 (GLFW/GLM)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

option(SKYVAULT_BUILD_TESTS "Compilar tests unitarios" ON)
option(SKYVAULT_BUILD_TOOLS "Compilar herramientas offline" ON)
option(SKYVAULT_WEB "Target Emscripten/WebAssembly (WebGL2)" OFF)
option(SKYVAULT_WARNINGS_AS_ERRORS "Tratar warnings como errores" OFF)

include(cmake/CompilerWarnings.cmake)

# ---------------------------------------------------------------------------
# Dependencias de terceros (solo auxiliares: ventana, input, audio, math)
# GLFW 3.4  - ventana / input / contexto GL
# GLM 1.0.1 - matematicas (permitida explicitamente por el diseno)
# miniaudio - audio (header unico vendido en third_party/)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_COCOA    OFF CACHE BOOL "" FORCE) # Linux/Windows only en este entorno

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE
)

if(SKYVAULT_WEB)
  # Emscripten: GLFW propio del SDK (implementacion JS/WebGL para el canvas)
  # y GLM desde fuente igual que en escritorio.
  set(SKYVAULT_WEB_DEF 1)
  FetchContent_MakeAvailable(glm)
  add_compile_definitions(SKYVAULT_WEB=1)
  add_compile_options(-sUSE_GLFW=3)
else()
  set(SKYVAULT_WEB_DEF 0)
  FetchContent_MakeAvailable(glfw glm)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Motor (engine/) - libreria estatica
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SV_ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp")
if(NOT SKYVAULT_WEB)
  list(APPEND SV_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio/miniaudio.c")
endif()

add_library(sv_engine STATIC ${SV_ENGINE_SOURCES})
target_include_directories(sv_engine PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/engine"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party"   # stb, font8x8
)
target_link_libraries(sv_engine PUBLIC glm::glm Threads::Threads ${CMAKE_DL_LIBS})
if(NOT SKYVAULT_WEB)
  target_link_libraries(sv_engine PUBLIC glfw)
endif()
sv_set_target_warnings(sv_engine)

# miniaudio: deshabilita backends cuyo header no exista (builds hermeticos)
include(CheckIncludeFileCXX)
check_include_file_cxx("alsa/asoundlib.h" SV_HAS_ALSA)
if(NOT SV_HAS_ALSA)
  target_compile_definitions(sv_engine PUBLIC MA_NO_ALSA=1)
endif()
target_compile_definitions(sv_engine PUBLIC MA_NO_PULSEAUDIO=1 MA_NO_JACK=1 MA_NO_RUNTIME_LINKING=1)
if(SKYVAULT_WEB)
  target_compile_definitions(sv_engine PUBLIC MA_USE_WEBAUDIO=1)
endif()

# ---------------------------------------------------------------------------
# Juego (game/)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SV_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp")
add_executable(skyvault ${SV_GAME_SOURCES})
target_include_directories(skyvault PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(skyvault PRIVATE sv_engine)
sv_set_target_warnings(skyvault)

# Nota: todos los modelos/texturas del juego se generan por codigo
# (requisito de originalidad), asi que no hay assets que copiar.

if(SKYVAULT_WEB)
  set_target_properties(skyvault PROPERTIES
    SUFFIX ".html"
    LINK_FLAGS "-sUSE_GLFW=3 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB -sMAXIMUM_MEMORY=1GB -sEXIT_RUNTIME=0 -sGL_DEBUG=0 -sEXPORTED_FUNCTIONS=_main,_svDebugLand,_svDebugState -sEXPORTED_RUNTIME_METHODS=ccall,cwrap --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
  )
  # shaders embebidos en C++ (no se necesita copiarlos)
  set_target_properties(skyvault PROPERTIES RULE_LAUNCH_LINK "")
endif()

# ---------------------------------------------------------------------------
# Tests unitarios (tests/) - solo CPU, sin GPU ni ventana
# ---------------------------------------------------------------------------
if(SKYVAULT_BUILD_TESTS AND NOT SKYVAULT_WEB)
  file(GLOB SV_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
  add_executable(sv_tests ${SV_TEST_SOURCES})
  target_link_libraries(sv_tests PRIVATE sv_engine)
  sv_set_target_warnings(sv_tests)
  enable_testing()
  add_test(NAME unit COMMAND sv_tests)
endif()

# ---------------------------------------------------------------------------
# Herramientas (tools/)
# ---------------------------------------------------------------------------
if(SKYVAULT_BUILD_TOOLS AND NOT SKYVAULT_WEB)
  file(GLOB SV_TOOL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp")
  foreach(tool_src ${SV_TOOL_SOURCES})
    get_filename_component(tool_name "${tool_src}" NAME_WE)
    add_executable(${tool_name} "${tool_src}")
    target_link_libraries(${tool_name} PRIVATE sv_engine)
    sv_set_target_warnings(${tool_name})
  endforeach()
endif()
