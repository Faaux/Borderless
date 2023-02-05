cmake_minimum_required(VERSION 3.20)

project(borderless)

include(cmake/CompilerWarnings.cmake)

set(CMAKE_CXX_STANDARD 20)

string(REPLACE "/EHsc" "/EHsc-" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable Exceptions
message(STATUS ${CMAKE_CXX_FLAGS})

# Third Party Libs
find_package(fmt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(GLEW REQUIRED)

file(GLOB BORDERLESS_SOURCES CONFIGURE_DEPENDS "src/borderless/*.cpp")
file(GLOB BORDERLESS_INCLUDES CONFIGURE_DEPENDS "include/borderless/*.h")

add_library(borderless_base OBJECT
    ${BORDERLESS_SOURCES}
    ${BORDERLESS_INCLUDES}
)
target_link_libraries(borderless_base PUBLIC fmt::fmt-header-only glfw imgui::imgui GLEW::GLEW)



add_executable(borderless WIN32 src/main.cpp src/resource.rc)
target_link_libraries(borderless PRIVATE borderless_base)
