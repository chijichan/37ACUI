cmake_minimum_required(VERSION 3.16)
project(ACUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================
# vcpkg 工具链（检查环境和固定路径）
# ========================
if(DEFINED ENV{VCPKG_ROOT} AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
elseif(EXISTS "E:/apps/vcpkg/scripts/buildsystems/vcpkg.cmake" AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "E:/apps/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# 查找 OpenGL 和 GLFW
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# ========================
# 第三方依赖源码 (本地 third_party 目录)
# ========================
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
set(IMPLOT_DIR "${CMAKE_SOURCE_DIR}/third_party/implot")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

# ========================
# 源文件
# ========================
set(ACUI_SOURCES
    src/main.cpp
    src/gui/app.cpp
    src/gui/console.cpp
    src/gui/lang.cpp
    src/network/http_client.cpp
)

set(ACUI_HEADERS
    src/gui/app.h
    src/gui/console.h
    src/gui/lang.h
    src/gui/panel_env.h
    src/gui/panel_server.h
    src/gui/panel_cli.h
    src/network/http_client.h
)

# ========================
# 可执行文件
# ========================
add_executable(${PROJECT_NAME} ${ACUI_SOURCES} ${ACUI_HEADERS}
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${OPENGL_INCLUDE_DIRS}
)

target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    ${OPENGL_LIBRARIES}
)

# Windows 子系统
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm ws2_32)
endif()

# ========================
# 安装规则
# ========================
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_BINARY_DIR})