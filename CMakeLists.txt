CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
# Rename as appropiate
PROJECT (MAPPINGS)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

ADD_EXECUTABLE(mappings-cheerp mappings.cpp raw_mappings.cpp utils.cpp)

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -cheerp-linear-heap-size=1024 -cheerp-make-module=commonjs -cheerp-preexecute -cheerp-pretty-code")
