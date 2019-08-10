cmake_minimum_required (VERSION 2.8)
project (wave-share)

option(USE_FINDSDL2 "Use the FindSDL2.cmake script" OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -W -Wall -Wno-long-long -pedantic")

#
## Dependencies
find_package(Threads REQUIRED)
find_package(FFTW REQUIRED)
find_package(SDL2)

if (NOT USE_FINDSDL2 AND NOT SDL2_FOUND)
    message(WARNING "Unable to find SDL2 library. It is either not installed or CMake cannot find it."
        " In the latter case, setting the USE_FINDSDL2 variable might help:\n"
        "   $ cmake -D USE_FINDSDL2 .."
        )

    message(FATAL_ERROR "Aborting")
endif()

string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)

add_executable(wave-share main.cpp)
target_include_directories(wave-share PUBLIC ${FFTW_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_libraries(wave-share PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${FFTW_LIBRARIES} ${SDL2_LIBRARIES})
