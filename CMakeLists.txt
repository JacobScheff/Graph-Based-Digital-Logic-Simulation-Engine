cmake_minimum_required(VERSION 3.14)
project(LogicSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Dependencies via FetchContent ────────────────────────────────────────────
include(FetchContent)

# GLFW – disable Wayland (not available in most WSL setups; uses X11 via WSLg)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11      ON  CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ImGui (docking branch – required for the DockSpace layout)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking)
FetchContent_MakeAvailable(imgui)

# ─── OpenGL ───────────────────────────────────────────────────────────────────
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
find_package(OpenGL REQUIRED)

# ─── ImGui source list ────────────────────────────────────────────────────────
set(IMGUI_SRC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ─── Project sources (absolute paths so out-of-tree builds work) ──────────────
set(SRC ${CMAKE_CURRENT_SOURCE_DIR})
set(SIM_SRC
    ${SRC}/Net.cpp
    ${SRC}/Pin.cpp
    ${SRC}/Components.cpp
    ${SRC}/TimingWheel.cpp
    ${SRC}/IO.cpp
    ${SRC}/PowerRails.cpp
    ${SRC}/Simulator.cpp
    ${SRC}/Canvas.cpp
    ${SRC}/CanvasSerialization.cpp
    ${SRC}/CustomComponent.cpp
    ${SRC}/App.cpp
    ${SRC}/main.cpp
)

add_executable(LogicSim ${SIM_SRC} ${IMGUI_SRC})

target_include_directories(LogicSim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(LogicSim PRIVATE
    glfw
    OpenGL::GL
    ${CMAKE_DL_LIBS}
)

# On Linux / WSL we need libGL explicitly
if(UNIX AND NOT APPLE)
    target_link_libraries(LogicSim PRIVATE GL)
endif()

# ─── Headless regression tests (no GLFW window required) ─────────────────────
set(TEST_SIM_SRC
    ${SRC}/Net.cpp
    ${SRC}/Pin.cpp
    ${SRC}/Components.cpp
    ${SRC}/TimingWheel.cpp
    ${SRC}/IO.cpp
    ${SRC}/PowerRails.cpp
    ${SRC}/Simulator.cpp
    ${SRC}/Canvas.cpp
    ${SRC}/CanvasSerialization.cpp
    ${SRC}/CustomComponent.cpp
)

add_executable(test_nested_custom ${SRC}/tests/test_nested_custom.cpp ${TEST_SIM_SRC} ${IMGUI_SRC})
target_include_directories(test_nested_custom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(test_nested_custom PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(test_nested_custom PRIVATE GL)
endif()
enable_testing()
add_test(NAME nested_custom COMMAND test_nested_custom WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})