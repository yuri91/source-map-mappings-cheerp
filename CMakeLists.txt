CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
# Rename as appropiate
PROJECT (MAPPINGS)

# It is convenient to add game sources one directory at the time using *.cpp
#FILE(GLOB PYTHON_SOURCES
#)
#INCLUDE_DIRECTORIES(
#)

ADD_EXECUTABLE(mappings mappings.cpp)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -cheerp-mode=wasm -std=c++14")

# TODO: Add PreExecuter pass near the end of project
# NOTE: Some short symbols in the minimized version are reserved for FB code, also the name of the game object itself
#       (by convention same name as the class, but lowercase), should also be reserved
#SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -cheerp-no-math-imul -cheerp-preexecute -cheerp-reserved-names=fbq,_fbq,FB,ima,gamename")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -O2  -cheerp-linear-heap-size=512 -cheerp-wasm-loader=loader.js -cheerp-make-module=commonjs")
