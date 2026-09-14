cmake_minimum_required(VERSION 3.20)
project(moku_host CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Platform-agnostic engine: everything in src/go, src/ai, plus the Butano-free parts of src/game.
file(GLOB GO_SRC ${ROOT}/src/go/*.cpp)
file(GLOB AI_SRC ${ROOT}/src/ai/*.cpp)
# The Butano-free half of src/game: rules-side game code that the host can build and test.
# Anything that includes a bn_*.h header (scenes, renderers, audio, save) stays out.
set(GAME_HOST_SRC
    ${ROOT}/src/game/missions.cpp
    ${ROOT}/src/game/missions_data.cpp
    ${ROOT}/src/game/strings.cpp
    ${ROOT}/src/game/session.cpp)
add_library(moku_engine STATIC ${GO_SRC} ${AI_SRC} ${GAME_HOST_SRC})
target_include_directories(moku_engine PUBLIC ${ROOT}/src ${ROOT}/include)
target_compile_definitions(moku_engine PUBLIC MOKU_HOST=1)

add_library(doctest_main STATIC doctest_main.cpp)
target_include_directories(doctest_main PUBLIC third_party)

enable_testing()
file(GLOB TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
foreach(t ${TEST_SRC})
  get_filename_component(name ${t} NAME_WE)
  add_executable(${name} ${t})
  target_link_libraries(${name} moku_engine doctest_main)
  add_test(NAME ${name} COMMAND ${name})
endforeach()

# Tools (not tests): AI arena for the strength ladder and fuzzers.
file(GLOB TOOL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/tool_*.cpp)
foreach(t ${TOOL_SRC})
  get_filename_component(name ${t} NAME_WE)
  add_executable(${name} ${t})
  target_link_libraries(${name} moku_engine)
endforeach()
