cmake_minimum_required(VERSION 3.20)
project(cs2-internal)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT /O2 /GS- /EHsc")

file(GLOB IMGUI_SOURCES
    dependencies/imgui/imgui.cpp
    dependencies/imgui/imgui_draw.cpp
    dependencies/imgui/imgui_widgets.cpp
    dependencies/imgui/imgui_tables.cpp
    dependencies/imgui/backends/imgui_impl_win32.cpp
    dependencies/imgui/backends/imgui_impl_dx11.cpp
)

add_library(cs2-internal SHARED
    main.cpp
    menu.cpp
    visuals.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(cs2-internal PRIVATE
    ${CMAKE_SOURCE_DIR}/dependencies/imgui
    ${CMAKE_SOURCE_DIR}/dependencies/imgui/backends
)

target_link_libraries(cs2-internal PRIVATE
    d3d11
    dxgi
)

set_target_properties(cs2-internal PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
