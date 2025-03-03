cmake_minimum_required(VERSION 3.13)

project(examGame)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE EXAM_GAME_SOURCES1 . ./*.[ch]pp)
file(GLOB_RECURSE EXAM_GAME_SOURCES2 . ./*.[ch])

add_executable(examGame ${EXAM_GAME_SOURCES1} ${EXAM_GAME_SOURCES2})
target_link_libraries(examGame PUBLIC project_options project_warnings)
target_link_libraries(examGame PUBLIC raylib flecs_static)

