cmake_minimum_required(VERSION 3.2.2)

set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3")

find_package(LLVM)

# Define add_llvm_* macro's.
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

include_directories(../include)
add_subdirectory(SILVIAAdd)
add_subdirectory(SILVIAMuladd)
add_subdirectory(SILVIAMul)
add_subdirectory(BBToDot)

