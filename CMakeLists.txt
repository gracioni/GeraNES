cmake_minimum_required(VERSION 3.26)

#message("geraldo")
#message(${CMAKE_RC_COMPILER})


#cmake -DCMAKE_BUILD_TYPE=Release
#cmake -DCMAKE_BUILD_TYPE=Debug

#message("geraldo")

# Check for Windows
if(WIN32)
    set(OPENGL_LIBRARY "opengl32")
else()
    set(OPENGL_LIBRARY "GL")
endif()

set(PKG_CONFIG_EXECUTABLE "C:/Users/geral/Downloads/pkg-config-lite-0.28-1_bin-win32/pkg-config-lite-0.28-1/bin/pkg-config.exe")

project(sdl_test)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.26.5
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SDL2)

INCLUDE_DIRECTORIES(${SDL2_SOURCE_DIR}/include)

# FetchContent_Declare(
#         SDL2_image
#         GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
#         GIT_TAG release-2.6.3
#         GIT_SHALLOW TRUE
#         GIT_PROGRESS TRUE
# )

# set(SDL2IMAGE_INSTALL OFF)
# set(BUILD_SHARED_LIBS FALSE)

# FetchContent_MakeAvailable(SDL2_image)

# FetchContent_Declare(
#         freetype
#         GIT_REPOSITORY https://github.com/freetype/freetype
#         GIT_TAG VER-2-10-3
#         GIT_SHALLOW TRUE
#         GIT_PROGRESS TRUE
# )

# set(BUILD_SHARED_LIBS FALSE)

# FetchContent_MakeAvailable(freetype)

# IF(CMAKE_BUILD_TYPE MATCHES Debug)
#         set(FREETYPE_LIBRARY ${freetype_BINARY_DIR}/libfreetyped.a)
# ELSE()
#         set(FREETYPE_LIBRARY ${freetype_BINARY_DIR}/libfreetype.a)
# ENDIF()

# set(FREETYPE_INCLUDE_DIRS ${freetype_SOURCE_DIR}/include)

# INCLUDE_DIRECTORIES(${freetype_SOURCE_DIR}/include)

# FetchContent_Declare(
#         SDL2_ttf
#         GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
#         GIT_TAG release-2.20.2
#         GIT_SHALLOW TRUE
#         GIT_PROGRESS TRUE
# )

# set(SDL3TTF_SAMPLES_INSTALL OFF)
# set(BUILD_SHARED_LIBS FALSE)

# FetchContent_MakeAvailable(SDL2_ttf)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.89.4
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
    ${IMGUI_PATH}/backends/imgui_impl_opengl3_loader.cpp)
add_library("imgui" STATIC ${IMGUI_GLOB})

target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH}/backends)

FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.0.2
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(nfd)

INCLUDE_DIRECTORIES(${nfd_SOURCE_DIR}/src/include)

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


FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8

        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(BUILD_SHARED_LIBS FALSE)

FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG v3.11.2

        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(BUILD_SHARED_LIBS FALSE)

FetchContent_MakeAvailable(json)



INCLUDE_DIRECTORIES(src)

set(CPP_FILES src/main.cpp src/GeraNes/RomFile.cpp src/sha1/sha1.cpp src/zip/zip.c src/signal/SigSlot.cpp src/GeraNesUI/ConfigFile.cpp)
#SDL2_image::SDL2_image-static SDL2_ttf::SDL2_ttf-static freetype
set(LIBS -static-libgcc -static-libstdc++ stdc++fs SDL2::SDL2main SDL2::SDL2-static imgui nfd libglew_static glm nlohmann_json::nlohmann_json ${OPENGL_LIBRARY})


if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS ${CMAKE_CURRENT_SOURCE_DIR}/src/resources.rc)
    set(CPP_FILES ${CPP_FILES} ${APP_ICON_RESOURCE_WINDOWS})
endif()




#add_executable(sdl_test WIN32 ${CPP_FILES})
add_executable(${CMAKE_PROJECT_NAME} ${CPP_FILES})
target_compile_features(${CMAKE_PROJECT_NAME} PUBLIC cxx_std_17)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBS})