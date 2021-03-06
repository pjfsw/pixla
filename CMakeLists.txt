cmake_minimum_required(VERSION 3.10)

project(pixla C)

include(FindPkgConfig)

pkg_search_module(SDL2 REQUIRED sdl2)
pkg_search_module(SDL2IMAGE REQUIRED SDL2_image>=2.0.0)
pkg_search_module(SDL2TTF REQUIRED SDL2_ttf>=2.0.0)

set(RESOURCE_DIR "${CMAKE_INSTALL_PREFIX}/share")
configure_file(config.h.in include/config.h)

file(GLOB sources CONFIGURE_DEPENDS "src/*.c")
add_executable(${PROJECT_NAME} ${sources})
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
include_directories(${SDL2_INCLUDE_DIRS} ${SDL2IMAGE_INCLUDE_DIRS} ${SDL2TTF_INCLUDE_DIRS} ${CMAKE_BINARY_DIR}/include)
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL2IMAGE_LIBRARIES} ${SDL2TTF_LIBRARIES} m)
install(TARGETS ${PROJECT_NAME})

file(GLOB resources CONFIGURE_DEPENDS "resources/*")
install(FILES ${resources} DESTINATION share)
