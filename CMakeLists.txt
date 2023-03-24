cmake_minimum_required(VERSION 3.0.0)
project(dota_img_split VERSION 0.1.0)
include(CTest)
enable_testing()

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall --std=c++11")

add_definitions(-DLOGURU_REPLACE_GLOG)
# find_package(GDAL CONFIG REQUIRED)
# set(GDAL_DIR /path/to/gdal)

find_package(GDAL REQUIRED)
set(EXTRA_LIBS ${EXTRA_LIBS} GDAL::GDAL)

include_directories(${PROJECT_SOURCE_DIR}/include)
aux_source_directory(${PROJECT_SOURCE_DIR}/src DIR_SRCS)

add_executable(${CMAKE_PROJECT_NAME} ${DIR_SRCS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${EXTRA_LIBS} pthread dl)

add_definitions(-O0)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)