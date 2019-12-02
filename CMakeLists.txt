cmake_minimum_required(VERSION 3.0)

project(dx-tut CXX)
set(CMAKE_CXX_STANDARD 17)


add_executable(dx_tut main.cpp)

target_link_libraries(dx_tut d3d11 d3dcompiler)

add_custom_command(
  TARGET dx_tut POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
          	${CMAKE_SOURCE_DIR}/shader.fx
            ${CMAKE_CURRENT_BINARY_DIR}/shader.fx
)


