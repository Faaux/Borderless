cmake_minimum_required(VERSION 3.20)

project(borderless)

include(cmake/CompilerWarnings.cmake)

set(CMAKE_CXX_STANDARD 20)

string(REPLACE "/EHsc" "/EHsc-" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable Exceptions
message(STATUS ${CMAKE_CXX_FLAGS})

# Third Party Libs
find_package(fmt CONFIG REQUIRED)

file(GLOB TEMPLATE_SOURCES CONFIGURE_DEPENDS "src/borderless/*.cpp")
file(GLOB TEMPLATE_INCLUDES CONFIGURE_DEPENDS "include/borderless/*.h")

add_library(borderless_base OBJECT
    ${TEMPLATE_SOURCES}
    ${TEMPLATE_INCLUDES}
)
target_link_libraries(borderless_base PUBLIC fmt::fmt-header-only)



add_executable(borderless src/main.cpp)
target_link_libraries(borderless PRIVATE borderless_base)
