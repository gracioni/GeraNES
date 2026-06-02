cmake_minimum_required(VERSION 3.26)
# CMake 4 removed implicit compatibility with very old policy versions used by
# some vendored dependencies (e.g. glew-cmake, plog).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

#message(${CMAKE_RC_COMPILER})

#cmake -DCMAKE_BUILD_TYPE=Release
#cmake -DCMAKE_BUILD_TYPE=Debug

project(GeraNES)

include(CTest)
enable_testing()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_definitions(
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
)

add_compile_options(
    $<$<CONFIG:Release>:-O3>
)

option(GERANES_ENABLE_WARNINGS_ALL "Enable broad compiler warnings for all targets" ON)
option(GERANES_WEB_PTHREADS "Use Emscripten pthreads and the threaded emulation host in web builds" OFF)
set(GERANES_WEB_PTHREADS_INITIAL_MEMORY_MB "512" CACHE STRING "Initial shared wasm memory for Emscripten pthread builds, in MiB")
add_library(geranes_warnings INTERFACE)
if(GERANES_ENABLE_WARNINGS_ALL)
    if(MSVC)
        target_compile_options(geranes_warnings INTERFACE
            /W4
            /permissive-
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(geranes_warnings INTERFACE
            $<$<COMPILE_LANGUAGE:C>:-Wall>
            $<$<COMPILE_LANGUAGE:C>:-Wextra>
            $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
            $<$<COMPILE_LANGUAGE:C>:-Wno-unused-function>
            $<$<COMPILE_LANGUAGE:CXX>:-Wall>
            $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
            $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-function>
        )
    endif()
endif()

if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")    
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/template.html")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s STACK_SIZE=8MB -s WASM=1 -s EXPORTED_RUNTIME_METHODS=[ccall,cwrap] -s EXPORTED_FUNCTIONS=[_main,_malloc,_free]")

    if(GERANES_WEB_PTHREADS)
        add_compile_definitions(GERANES_WEB_PTHREADS=1)
        math(EXPR GERANES_WEB_PTHREADS_INITIAL_MEMORY_BYTES "${GERANES_WEB_PTHREADS_INITIAL_MEMORY_MB} * 1024 * 1024")
        add_compile_definitions(GERANES_WEB_PTHREADS_INITIAL_MEMORY_MB=${GERANES_WEB_PTHREADS_INITIAL_MEMORY_MB})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=2 -s INITIAL_MEMORY=${GERANES_WEB_PTHREADS_INITIAL_MEMORY_BYTES}")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(CMakeRC)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

if(NOT EMSCRIPTEN)
    FetchContent_Declare(
            SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-2.32.10
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )

    set(SDL_VULKAN OFF)
    set(SDL_TEST OFF)
    set(SDL_STATIC ON)
    set(SDL_SHARED OFF)
    set(SDL_RENDER_D3D OFF)

    FetchContent_MakeAvailable(SDL2)

    INCLUDE_DIRECTORIES(${SDL2_SOURCE_DIR}/include)
endif()

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(stb)

INCLUDE_DIRECTORIES(${stb_SOURCE_DIR})


FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.5-docking
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui)

INCLUDE_DIRECTORIES(PUBLIC ${imgui_SOURCE_DIR})

set(IMGUI_PATH ${imgui_SOURCE_DIR})
file(GLOB IMGUI_GLOB
    ${IMGUI_PATH}/imgui.h
    ${IMGUI_PATH}/imgui.cpp
    ${IMGUI_PATH}/imconfig.h
    ${IMGUI_PATH}/imgui_demo.cpp
    ${IMGUI_PATH}/imgui_draw.cpp
    ${IMGUI_PATH}/imgui_internal.h
    ${IMGUI_PATH}/imstb_rectpack.h
    ${IMGUI_PATH}/imstb_textedit.h
    ${IMGUI_PATH}/imstb_truetype.h
    ${IMGUI_PATH}/imgui_tables.cpp
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/misc/cpp/imgui_stdlib.h
    ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp

    # specific bindings...
    ${IMGUI_PATH}/backends/imgui_impl_sdl2.h
    ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.h
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3_loader.cpp
)
add_library("imgui" STATIC ${IMGUI_GLOB})

target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH}/backends)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    message(STATUS "Compiling for Emscripten: Ignoring nfd libraries")
    # Defina uma variável ou faça ajustes conforme necessário para Emscripten
else()

    FetchContent_Declare(
            nfd
            GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
            GIT_TAG v1.2.1
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(nfd)

    INCLUDE_DIRECTORIES(${nfd_SOURCE_DIR}/src/include)

endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    message(STATUS "Compiling for Emscripten: Ignoring glew libraries")
    # Defina uma variável ou faça ajustes conforme necessário para Emscripten
else()

    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    FetchContent_Declare(
            GLEW
            GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
            GIT_TAG glew-cmake-2.2.0
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )

    set(glew-cmake_BUILD_SHARED OFF)
    set(glew-cmake_BUILD_STATIC ON)
    set(ONLY_LIBS ON)
    set(BUILD_SHARED_LIBS FALSE)

    FetchContent_MakeAvailable(GLEW)

endif()


FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(BUILD_SHARED_LIBS FALSE)

FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(BUILD_SHARED_LIBS FALSE)

FetchContent_MakeAvailable(json)

FetchContent_Declare(
  yoga
  GIT_REPOSITORY https://github.com/facebook/yoga.git
  GIT_TAG v3.2.1
  GIT_SHALLOW TRUE
)

set(BUILD_FUZZ_TESTS OFF CACHE BOOL "Disable fuzz tests for yoga" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable third-party tests in this superbuild" FORCE)
set(YOGA_BUILD_TESTS OFF CACHE BOOL "Disable yoga tests" FORCE)
set(gmock_build_tests OFF CACHE BOOL "Disable gmock tests" FORCE)
set(gtest_build_tests OFF CACHE BOOL "Disable gtest tests" FORCE)
set(gtest_build_samples OFF CACHE BOOL "Disable gtest samples" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Do not install gtest" FORCE)

FetchContent_GetProperties(yoga)
if(NOT yoga_POPULATED)
    FetchContent_Populate(yoga)
endif()

# We only need the Yoga core library. Pulling in the top-level project adds
# vendored gtest targets and broken placeholder CTest entries such as
# `yogatests_NOT_BUILT`, which pollute the VS Code Testing view.
add_subdirectory("${yoga_SOURCE_DIR}/yoga" "${yoga_BINARY_DIR}/yoga")

# Some Clang-based toolchains can promote gtest char8_t conversion warnings
# to errors. Keep this scoped to vendored gtest targets and avoid passing
# unsupported warning flags to GCC/MinGW.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if (TARGET gtest)
        target_compile_options(gtest PRIVATE -Wno-character-conversion -Wno-error=character-conversion)
    endif()
    if (TARGET gtest_main)
        target_compile_options(gtest_main PRIVATE -Wno-character-conversion -Wno-error=character-conversion)
    endif()
endif()

FetchContent_Declare(
    flips
    GIT_REPOSITORY https://github.com/Alcaro/Flips.git
    GIT_TAG        master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(flips)

INCLUDE_DIRECTORIES(PUBLIC ${flips_SOURCE_DIR})

FetchContent_Declare(
    pffft
    GIT_REPOSITORY https://github.com/marton78/pffft.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_GetProperties(pffft)
if(NOT pffft_POPULATED)
    FetchContent_Populate(pffft)
endif()

add_library(PFFFT STATIC
    ${pffft_SOURCE_DIR}/src/pffft.c
    ${pffft_SOURCE_DIR}/src/pffft_common.c
)
add_library(PFFFT::PFFFT ALIAS PFFFT)
target_include_directories(PFFFT
    PUBLIC
        ${pffft_SOURCE_DIR}/include
    PRIVATE
        ${pffft_SOURCE_DIR}/src
        ${pffft_SOURCE_DIR}/include/pffft
)
target_compile_definitions(PFFFT PUBLIC PFFFT_STATIC_DEFINE)
if(NOT MSVC)
    target_link_libraries(PFFFT PUBLIC m)
endif()

if(NOT EMSCRIPTEN)
    FetchContent_Declare(
        enet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(enet)
    INCLUDE_DIRECTORIES(${enet_SOURCE_DIR}/include)

    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-2
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(asio)
    add_library(asio_headers INTERFACE)
    target_include_directories(asio_headers SYSTEM INTERFACE
        ${asio_SOURCE_DIR}/asio/include
    )

    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG v3.6.2
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedtls programs" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "Disable mbedtls tests" FORCE)
    set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "Use static mbedtls libraries" FORCE)
    set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "Build static mbedtls libraries" FORCE)

    FetchContent_MakeAvailable(mbedtls)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        foreach(_mbedtls_target mbedtls mbedx509 mbedcrypto)
            if(TARGET ${_mbedtls_target})
                target_compile_options(${_mbedtls_target} PRIVATE
                    -Wno-error=unterminated-string-initialization
                    -Wno-unterminated-string-initialization
                )
            endif()
        endforeach()
    endif()
    foreach(_mbedtls_target mbedtls mbedx509 mbedcrypto)
        if(TARGET ${_mbedtls_target})
            target_compile_definitions(${_mbedtls_target} PUBLIC MBEDTLS_USER_CONFIG_FILE=\"mbedtls_user_config.h\")
            target_include_directories(${_mbedtls_target} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/cmake)
        endif()
    endforeach()
    get_target_property(_geranes_mbedtls_include_dirs mbedtls INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _geranes_mbedtls_include_dirs)
        get_target_property(_geranes_mbedtls_include_dirs mbedtls INCLUDE_DIRECTORIES)
    endif()
    set(MBEDTLS_FOUND TRUE CACHE INTERNAL "GeraNES bundled MbedTLS available")
    set(MBEDTLS_INCLUDE_DIRS "${_geranes_mbedtls_include_dirs}" CACHE STRING "Bundled MbedTLS includes" FORCE)
    set(MBEDTLS_VERSION_GREATER_THAN_3 "${_geranes_mbedtls_include_dirs}" CACHE STRING "Bundled MbedTLS v3 marker" FORCE)
    set(MBEDTLS_LIBRARY mbedtls CACHE STRING "Bundled MbedTLS library" FORCE)
    set(MBEDX509_LIBRARY mbedx509 CACHE STRING "Bundled MbedX509 library" FORCE)
    set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "Bundled MbedCrypto library" FORCE)
    set(MBEDTLS_LIBRARIES "mbedtls;mbedx509;mbedcrypto" CACHE STRING "Bundled MbedTLS libraries" FORCE)

    set(NO_TESTS ON CACHE BOOL "Disable libdatachannel tests" FORCE)
    set(NO_EXAMPLES ON CACHE BOOL "Disable libdatachannel examples" FORCE)
    set(NO_MEDIA ON CACHE BOOL "Disable libdatachannel media" FORCE)
    set(USE_NICE OFF CACHE BOOL "Disable libnice backend" FORCE)
    set(USE_GNUTLS OFF CACHE BOOL "Disable GnuTLS backend for libdatachannel" FORCE)
    set(USE_MBEDTLS ON CACHE BOOL "Use Mbed TLS backend for libdatachannel" FORCE)

    FetchContent_Declare(
        libdatachannel
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG v0.22.4
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(libdatachannel)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        foreach(_datachannel_target datachannel datachannel-static)
            if(TARGET ${_datachannel_target})
                target_compile_options(${_datachannel_target} PRIVATE
                    -Wno-cast-function-type
                )
            endif()
        endforeach()
        foreach(_libjuice_target juice juice-static)
            if(TARGET ${_libjuice_target})
                target_compile_options(${_libjuice_target} PRIVATE
                    -Wno-cast-function-type
                )
            endif()
        endforeach()
    endif()

    set(USE_TLS ON CACHE BOOL "Enable IXWebSocket TLS support" FORCE)
    set(USE_MBED_TLS ON CACHE BOOL "Use MbedTLS for IXWebSocket TLS" FORCE)
    set(USE_OPEN_SSL OFF CACHE BOOL "Disable OpenSSL for IXWebSocket TLS" FORCE)
    set(USE_ZLIB OFF CACHE BOOL "Disable IXWebSocket zlib support" FORCE)
    set(BUILD_DEMO OFF CACHE BOOL "Disable IXWebSocket demo build" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "Disable IXWebSocket install rules" FORCE)

    FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG v11.4.6
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(ixwebsocket)
    if(TARGET ixwebsocket)
        target_compile_definitions(ixwebsocket PRIVATE IXWEBSOCKET_USE_MBED_TLS_MIN_VERSION_3)
    endif()
endif()

set(FLIPS_PATH ${flips_SOURCE_DIR})
file(GLOB FLIPS_GLOB
    ${FLIPS_PATH}/libips.h
    ${FLIPS_PATH}/libips.cpp
    ${FLIPS_PATH}/libbps.h
    ${FLIPS_PATH}/libbps.cpp
    ${FLIPS_PATH}/libups.h
    ${FLIPS_PATH}/libups.cpp
    ${FLIPS_PATH}/crc32.h
    ${FLIPS_PATH}/crc32.cpp
)

add_library("flips" STATIC ${FLIPS_GLOB})
set_target_properties("flips" PROPERTIES POSITION_INDEPENDENT_CODE ON)

INCLUDE_DIRECTORIES(src)

# Gets the list of files in the resources folder
file(GLOB_RECURSE RESOURCES_LIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")

# Add the resource files using the list obtained
cmrc_add_resource_library(resources ${RESOURCES_LIST})
cmrc_add_resource_library(libretro_db_assets "${CMAKE_CURRENT_SOURCE_DIR}/data/db.txt")
set_target_properties(resources PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(libretro_db_assets PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GERANES_DOCS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/user-guide/mkdocs.yml")
set(GERANES_DOCS_SITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/docs/user-guide/site")

add_custom_target(generate_user_docs
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${GERANES_DOCS_SITE_DIR}"
    COMMAND ${Python3_EXECUTABLE} -m mkdocs build --clean
            --config-file "${GERANES_DOCS_CONFIG_FILE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating user documentation"
    VERBATIM
)

# Copy data folder to build
if(EMSCRIPTEN)
    set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file ${DATA_DIR}@/")

    add_custom_target(copy_web_docs ALL
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/docs"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/docs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GERANES_DOCS_SITE_DIR}"
                "${CMAKE_BINARY_DIR}/docs"
        COMMENT "Copying generated user documentation to web build directory"
        VERBATIM
    )

    add_dependencies(copy_web_docs generate_user_docs)
else()
    set(GERANES_RUNTIME_DATA_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/runtime_data")
    set(GERANES_RUNTIME_DATA_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/generated/runtime_data_manifest.txt")

    add_custom_target(copy_runtime_data ALL
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${GERANES_RUNTIME_DATA_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GERANES_RUNTIME_DATA_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/data"
                "${GERANES_RUNTIME_DATA_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${GERANES_RUNTIME_DATA_STAGE_DIR}/docs/site"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GERANES_RUNTIME_DATA_STAGE_DIR}/docs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GERANES_DOCS_SITE_DIR}"
                "${GERANES_RUNTIME_DATA_STAGE_DIR}/docs"
        COMMAND ${CMAKE_COMMAND}
                -DSOURCE_DIR="${GERANES_RUNTIME_DATA_STAGE_DIR}"
                -DDEST_DIR="${CMAKE_BINARY_DIR}"
                -DMANIFEST_FILE="${GERANES_RUNTIME_DATA_MANIFEST}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SyncManagedTree.cmake"
        COMMENT "Copying runtime data to build directory"
        VERBATIM
    )

    add_dependencies(copy_runtime_data generate_user_docs)

    if(FALSE)
    file(GLOB source_files_and_dirs ${CMAKE_CURRENT_SOURCE_DIR}/data/*)

    # Copia cada item (diretório ou arquivo) para o destino
    foreach(item IN LISTS source_files_and_dirs)
        file(COPY ${item} DESTINATION ${CMAKE_BINARY_DIR})
    endforeach()
    endif()

endif()


if(WIN32)
    set(OPENGL_LIBRARY "opengl32")
else()
    set(OPENGL_LIBRARY "GL")
endif()

#set(CPP_FILES src/main.cpp src/zip/zip.c src/signal/SigSlot.cpp src/GeraNESApp/SDLOpenGLWindow.cpp src/util/stb_image_impl.cpp src/util/sdl_util.cpp)

file(GLOB_RECURSE CPP_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CPP_FILES EXCLUDE REGEX ".*/src/libretro/.*")
if(NOT EMSCRIPTEN)
    list(FILTER CPP_FILES EXCLUDE REGEX ".*/src/GeraNESApp/EmscriptenBindings\\.cpp$")
    list(FILTER CPP_FILES EXCLUDE REGEX ".*/src/GeraNESApp/EmscriptenUtil\\.cpp$")
    list(FILTER CPP_FILES EXCLUDE REGEX ".*/src/GeraNESApp/OpenALAudioOutput\\.cpp$")
endif()

set(GERANES_LIBRARY_SOURCES ${CPP_FILES})
list(FILTER GERANES_LIBRARY_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")
list(FILTER GERANES_LIBRARY_SOURCES EXCLUDE REGEX ".*/src/resources\\.rc$")

set(GERANES_APP_LIBRARY_SOURCES ${GERANES_LIBRARY_SOURCES})
list(FILTER GERANES_APP_LIBRARY_SOURCES INCLUDE REGEX ".*/src/GeraNESApp/.*\\.(c|cpp)$")
list(APPEND GERANES_APP_LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/GeraNESInputFrameAdapter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/GeraNESNetplayAdapters.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/GeraNESNetplayConsole.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/GeraNESNetplayRuntimeDriver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/GeraNESNetplayMenuHelpers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/GeraNESNetplayAssignmentHelpers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESNetplay/NetplayWindowUI.cpp"
)

set(CONSOLE_NETPLAY_LIBRARY_SOURCES ${GERANES_LIBRARY_SOURCES})
list(FILTER CONSOLE_NETPLAY_LIBRARY_SOURCES INCLUDE REGEX ".*/src/ConsoleNetplay/.*\\.(c|cpp)$")
set(GERANES_CORE_LIBRARY_SOURCES ${GERANES_LIBRARY_SOURCES})
list(FILTER GERANES_CORE_LIBRARY_SOURCES EXCLUDE REGEX ".*/src/GeraNESApp/.*\\.(c|cpp)$")
list(FILTER GERANES_CORE_LIBRARY_SOURCES EXCLUDE REGEX ".*/src/GeraNESNetplay/.*\\.(c|cpp)$")
list(FILTER GERANES_CORE_LIBRARY_SOURCES EXCLUDE REGEX ".*/src/ConsoleNetplay/.*\\.(c|cpp)$")

set(GERANES_EXE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
if(EMSCRIPTEN)
    list(APPEND GERANES_EXE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/GeraNESApp/EmscriptenBindings.cpp"
    )
endif()

set(LIBS -static-libgcc -static-libstdc++ imgui flips PFFFT::PFFFT glm nlohmann_json::nlohmann_json yogacore resources ${OPENGL_LIBRARY})
set(CONSOLE_NETPLAY_LIBS nlohmann_json::nlohmann_json)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(LIBS ${LIBS} SDL2::SDL2-static libglew_static nfd stdc++fs enet LibDataChannel::LibDataChannelStatic ixwebsocket::ixwebsocket)
    set(CONSOLE_NETPLAY_LIBS ${CONSOLE_NETPLAY_LIBS} stdc++fs enet LibDataChannel::LibDataChannelStatic ixwebsocket::ixwebsocket)
else()
set(LIBS ${LIBS} -lopenal)
endif()


if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS ${CMAKE_CURRENT_SOURCE_DIR}/src/resources.rc)
    list(APPEND GERANES_EXE_SOURCES ${APP_ICON_RESOURCE_WINDOWS})
    set(LIBS ${LIBS} -ldwmapi -lmswsock -lwinhttp)
    set(CONSOLE_NETPLAY_LIBS ${CONSOLE_NETPLAY_LIBS} -lmswsock -lwinhttp)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(GeraNESLib STATIC ${GERANES_CORE_LIBRARY_SOURCES})
target_compile_features(GeraNESLib PUBLIC cxx_std_20)
target_compile_definitions(GeraNESLib PUBLIC ENABLE_NSF_PLAYER=1)
if(NOT EMSCRIPTEN)
    target_compile_definitions(GeraNESLib PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_ RTC_STATIC)
endif()
if(MINGW)
    target_compile_options(GeraNESLib PRIVATE -Wa,-mbig-obj)
endif()
target_link_libraries(GeraNESLib PUBLIC ${LIBS})
target_link_libraries(GeraNESLib PRIVATE geranes_warnings)
if(NOT EMSCRIPTEN)
    target_link_libraries(GeraNESLib PRIVATE asio_headers)
endif()
if(MINGW)
    target_link_options(GeraNESLib PRIVATE -static-libgcc -static-libstdc++)
    target_link_libraries(GeraNESLib PRIVATE
        -Wl,-Bstatic
        -lstdc++
        -lgcc
        -lwinpthread
        -Wl,-Bdynamic
    )
endif()

add_library(ConsoleNetplay STATIC ${CONSOLE_NETPLAY_LIBRARY_SOURCES})
target_compile_features(ConsoleNetplay PUBLIC cxx_std_20)
target_compile_definitions(ConsoleNetplay PUBLIC ENABLE_NSF_PLAYER=1)
if(NOT EMSCRIPTEN)
    target_compile_definitions(ConsoleNetplay PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_ RTC_STATIC)
endif()
if(MINGW)
    target_compile_options(ConsoleNetplay PRIVATE -Wa,-mbig-obj)
endif()
target_link_libraries(ConsoleNetplay PUBLIC ${CONSOLE_NETPLAY_LIBS})
target_link_libraries(ConsoleNetplay PRIVATE geranes_warnings)
if(NOT EMSCRIPTEN)
    target_link_libraries(ConsoleNetplay PRIVATE asio_headers)
endif()
if(MINGW)
    target_link_options(ConsoleNetplay PRIVATE -static-libgcc -static-libstdc++)
endif()

add_library(GeraNESAppLib STATIC ${GERANES_APP_LIBRARY_SOURCES})
target_compile_features(GeraNESAppLib PUBLIC cxx_std_20)
target_compile_definitions(GeraNESAppLib PUBLIC ENABLE_NSF_PLAYER=1)
if(NOT EMSCRIPTEN)
    target_compile_definitions(GeraNESAppLib PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_ RTC_STATIC)
endif()
if(MINGW)
    target_compile_options(GeraNESAppLib PRIVATE -Wa,-mbig-obj)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GeraNESAppLib PRIVATE -Wno-template-body)
endif()
target_link_libraries(GeraNESAppLib PUBLIC ConsoleNetplay GeraNESLib)
target_link_libraries(GeraNESAppLib PRIVATE geranes_warnings)
if(NOT EMSCRIPTEN)
    target_link_libraries(GeraNESAppLib PRIVATE asio_headers)
endif()
if(MINGW)
    target_link_options(GeraNESAppLib PRIVATE -static-libgcc -static-libstdc++)
endif()

add_executable(${CMAKE_PROJECT_NAME} ${GERANES_EXE_SOURCES})
target_compile_features(${CMAKE_PROJECT_NAME} PUBLIC cxx_std_20)
if(NOT EMSCRIPTEN)
    add_dependencies(${CMAKE_PROJECT_NAME} copy_runtime_data)
endif()
if(MINGW)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wa,-mbig-obj)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wno-template-body)
endif()
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE GeraNESAppLib ConsoleNetplay GeraNESLib)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE geranes_warnings)
if(NOT EMSCRIPTEN)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE SDL2::SDL2main)
endif()
if(WIN32)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    )
endif()
if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_WINPTHREAD_DLL "${MINGW_BIN_DIR}/libwinpthread-1.dll")
    if(EXISTS "${MINGW_WINPTHREAD_DLL}")
        add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_WINPTHREAD_DLL}"
                "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/libwinpthread-1.dll"
        )
    endif()
endif()

set(GERANES_DEFAULT_TEST_ROM "" CACHE FILEPATH "Required ROM fixture used by Catch2 tests. Set this path or define GERANES_TEST_ROM before running GeraNESTests." FORCE)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/TestConfig.h"
    @ONLY
)

add_executable(GeraNESTests
    tests/StateReplayTests.cpp
    tests/NetplayTests.cpp
)
target_compile_features(GeraNESTests PUBLIC cxx_std_20)
target_include_directories(GeraNESTests PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_precompile_headers(GeraNESTests PRIVATE
    <catch2/catch_test_macros.hpp>
    <algorithm>
    <chrono>
    <cstdint>
    <optional>
    <random>
    <string>
    <thread>
    <vector>
)
if(MINGW)
    target_compile_options(GeraNESTests PRIVATE -Wa,-mbig-obj)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GeraNESTests PRIVATE -Wno-template-body)
endif()
if(NOT EMSCRIPTEN)
    target_compile_definitions(GeraNESTests PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_ RTC_STATIC)
endif()
target_link_libraries(GeraNESTests PRIVATE GeraNESAppLib ConsoleNetplay GeraNESLib Catch2::Catch2WithMain)
target_link_libraries(GeraNESTests PRIVATE geranes_warnings)
if(NOT EMSCRIPTEN)
    target_link_libraries(GeraNESTests PRIVATE asio_headers)
endif()
if(WIN32)
    target_link_libraries(GeraNESTests PRIVATE mswsock)
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(GeraNESTests
    PROPERTIES
        RUN_SERIAL TRUE
)

add_executable(GeraNESStateReplayTests
    tests/StateReplayTests.cpp
)
target_compile_features(GeraNESStateReplayTests PUBLIC cxx_std_20)
target_include_directories(GeraNESStateReplayTests PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_precompile_headers(GeraNESStateReplayTests PRIVATE
    <catch2/catch_test_macros.hpp>
    <algorithm>
    <chrono>
    <cstdint>
    <optional>
    <random>
    <string>
    <thread>
    <vector>
)
if(MINGW)
    target_compile_options(GeraNESStateReplayTests PRIVATE -Wa,-mbig-obj)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GeraNESStateReplayTests PRIVATE -Wno-template-body)
endif()
if(NOT EMSCRIPTEN)
    target_compile_definitions(GeraNESStateReplayTests PRIVATE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_ _WEBSOCKETPP_CPP11_THREAD_ RTC_STATIC)
endif()
target_link_libraries(GeraNESStateReplayTests PRIVATE GeraNESAppLib ConsoleNetplay GeraNESLib Catch2::Catch2WithMain)
target_link_libraries(GeraNESStateReplayTests PRIVATE geranes_warnings)
if(NOT EMSCRIPTEN)
    target_link_libraries(GeraNESStateReplayTests PRIVATE asio_headers)
endif()
if(WIN32)
    target_link_libraries(GeraNESStateReplayTests PRIVATE mswsock)
endif()

catch_discover_tests(GeraNESStateReplayTests
    PROPERTIES
        RUN_SERIAL TRUE
)

set(LIBRETRO_CORE_NAME "geranes_libretro")

add_library(${LIBRETRO_CORE_NAME} SHARED
    src/libretro/GeraNESLibretro.cpp
    src/GeraNESApp/AudioOutputBase.cpp
    src/GeraNES/ThirdParty/emu2413.cpp
    src/signal/signal.cpp
    src/zip/zip.c
)
target_compile_features(${LIBRETRO_CORE_NAME} PUBLIC cxx_std_20)
target_include_directories(${LIBRETRO_CORE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(${LIBRETRO_CORE_NAME} PRIVATE GERANES_LIBRETRO_USE_CMRC_DB=1)
if(MINGW)
    target_compile_options(${LIBRETRO_CORE_NAME} PRIVATE -Wa,-mbig-obj)
endif()
target_link_libraries(${LIBRETRO_CORE_NAME} PRIVATE libretro_db_assets flips nlohmann_json::nlohmann_json geranes_warnings)
if(MINGW)
    # Make the core self-contained for RetroArch on Windows.
    target_link_options(${LIBRETRO_CORE_NAME} PRIVATE -static-libgcc -static-libstdc++)
    target_link_libraries(${LIBRETRO_CORE_NAME} PRIVATE
        -Wl,-Bstatic
        -lstdc++
        -lgcc
        -lwinpthread
        -Wl,-Bdynamic
    )
endif()
set_target_properties(${LIBRETRO_CORE_NAME} PROPERTIES PREFIX "")

# Copy the canonical libretro .info file.
set(LIBRETRO_INFO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/libretro/geranes_libretro.info")
set(LIBRETRO_INFO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${LIBRETRO_CORE_NAME}.info")
configure_file("${LIBRETRO_INFO_SOURCE}" "${LIBRETRO_INFO_OUTPUT}" COPYONLY)
