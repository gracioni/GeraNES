cmake_minimum_required(VERSION 3.26)

#message(${CMAKE_RC_COMPILER})

#cmake -DCMAKE_BUILD_TYPE=Release
#cmake -DCMAKE_BUILD_TYPE=Debug

project(GeraNES)

if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")    
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/template.html")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s STACK_SIZE=8MB -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=[ccall,cwrap] -s EXPORTED_FUNCTIONS=[_main,_malloc,_free]")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(CMakeRC)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

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
        GIT_TAG v1.92.5
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
set(gmock_build_tests OFF CACHE BOOL "Disable gmock tests" FORCE)
set(gtest_build_tests OFF CACHE BOOL "Disable gtest tests" FORCE)
set(gtest_build_samples OFF CACHE BOOL "Disable gtest samples" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Do not install gtest" FORCE)

FetchContent_MakeAvailable(yoga)

FetchContent_Declare(
    flips
    GIT_REPOSITORY https://github.com/Alcaro/Flips.git
    GIT_TAG        master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(flips)

INCLUDE_DIRECTORIES(PUBLIC ${flips_SOURCE_DIR})

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

INCLUDE_DIRECTORIES(src)

# Gets the list of files in the resources folder
file(GLOB_RECURSE RESOURCES_LIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")

# Add the resource files using the list obtained
cmrc_add_resource_library(resources ${RESOURCES_LIST})

# Copy data folder to build
if(EMSCRIPTEN)
    set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --preload-file ${DATA_DIR}@/")
else()

    file(GLOB source_files_and_dirs ${CMAKE_CURRENT_SOURCE_DIR}/data/*)

    # Copia cada item (diretório ou arquivo) para o destino
    foreach(item IN LISTS source_files_and_dirs)
        file(COPY ${item} DESTINATION ${CMAKE_BINARY_DIR})
    endforeach()

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

set(LIBS -static-libgcc -static-libstdc++ SDL2::SDL2main SDL2::SDL2-static imgui flips glm nlohmann_json::nlohmann_json yogacore resources ${OPENGL_LIBRARY})

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(LIBS ${LIBS} libglew_static nfd stdc++fs)
else()
set(LIBS ${LIBS} -lopenal)
endif()


if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS ${CMAKE_CURRENT_SOURCE_DIR}/src/resources.rc)
    set(CPP_FILES ${CPP_FILES} ${APP_ICON_RESOURCE_WINDOWS})
    set(LIBS ${LIBS} -ldwmapi)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${CMAKE_PROJECT_NAME} ${CPP_FILES})
target_compile_features(${CMAKE_PROJECT_NAME} PUBLIC cxx_std_20)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBS})