cmake_minimum_required(VERSION 3.16)
project(ACUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================
# vcpkg 工具链（自动检测环境变量和常见安装路径）
# ========================
if(NOT CMAKE_TOOLCHAIN_FILE)
    # 优先使用环境变量 VCPKG_ROOT
    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_tc "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${_vcpkg_tc}")
            set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_tc}" CACHE STRING "")
            message(STATUS "vcpkg 工具链 (VCPKG_ROOT): ${_vcpkg_tc}")
        endif()
    endif()

    # 回退到常见安装路径
    if(NOT CMAKE_TOOLCHAIN_FILE)
        foreach(_candidate
            "E:/apps/vcpkg"
            "C:/vcpkg"
            "C:/dev/vcpkg"
            "$ENV{USERPROFILE}/vcpkg"
        )
            set(_vcpkg_tc "${_candidate}/scripts/buildsystems/vcpkg.cmake")
            if(EXISTS "${_vcpkg_tc}")
                set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_tc}" CACHE STRING "")
                message(STATUS "vcpkg 工具链 (自动检测): ${_vcpkg_tc}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT CMAKE_TOOLCHAIN_FILE)
        message(WARNING "未找到 vcpkg 工具链。请设置 VCPKG_ROOT 环境变量或通过 -DCMAKE_TOOLCHAIN_FILE=... 指定路径。")
    endif()
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
# WIN32 关键字：Windows GUI 子系统，避免启动时弹出黑色控制台窗口。
# 同时用 /ENTRY:mainCRTStartup 保留 main() 入口（GUI 子系统默认要求 WinMain）。
set(_ACUI_TARGET_KIND "")
if(WIN32)
    set(_ACUI_TARGET_KIND WIN32)
endif()
add_executable(${PROJECT_NAME} ${_ACUI_TARGET_KIND} ${ACUI_SOURCES} ${ACUI_HEADERS}
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)
if(WIN32)
    target_link_options(${PROJECT_NAME} PRIVATE /ENTRY:mainCRTStartup)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${OPENGL_INCLUDE_DIRS}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    /utf-8          # UTF-8 源码编码
    /W4             # 高警告级别
    /wd4100         # 忽略未引用形参警告
    /wd4127         # 忽略条件表达式常量警告 (ImGui)
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS   # 使用标准 C 函数 (fopen/sscanf 等)
    NOMINMAX                  # 避免 Windows.h 定义 min/max 宏
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    ${OPENGL_LIBRARIES}
)

# Windows 子系统依赖
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm ws2_32)
endif()

# ========================
# 安装规则
# ========================
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# 拷贝运行时配置文件到构建输出目录
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/acui.ini")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/acui.ini"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/acui.ini"
        COMMENT "Copying acui.ini to output directory"
    )
endif()