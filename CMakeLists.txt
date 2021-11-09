cmake_minimum_required(VERSION 3.8)
project ("sample_ueye" CXX)

set(CMAKE_CXX_STANDARD 17)


if (WIN32)
    set (CMAKE_CXX_COMPILER cl.exe)
    find_package (Threads REQUIRED)
    set(WSTPLINK_PATH "C:/Program Files/Wolfram Research/Mathematica/12.3/SystemFiles/Links/WSTP/DeveloperKit/Windows-x86-64")

    set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} 
        "C:/Program Files/IDS/ids_peak/sdk/api/cmake_finder"
        "C:/Program Files/IDS/ids_peak/sdk/ipl/cmake_finder"
    )
    set (CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};C:/Qt/6.0.4/msvc2019_64")

    set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build_win32")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin_win32")
else()
    # assuming linux
    set(CMAKE_CXX_COMPILER g++)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(WSTPLINK_PATH "/opt/Wolfram/Mathematica/12.0/SystemFiles/Links/WSTP/DeveloperKit/Linux-x86-64")

    set(CMAKE_BINARY_DIR "${CMAKE_SOURCE_DIR}/build")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package (ids_peak REQUIRED)
find_package (ids_peak_ipl REQUIRED)
find_package (Qt6 COMPONENTS Widgets Core Gui REQUIRED)

include_directories(AFTER "${WSTPLINK_PATH}/CompilerAdditions"
    "${CMAKE_SOURCE_DIR}/include")


add_executable (test_wstp
    src/test_wstp.cpp
    src/MathematicaL.cpp
)

add_executable (ueye_link
    src/main.cpp
    src/MainWindow.cpp
    src/Display.cpp
    src/Worker.cpp
    src/Camera.cpp
    src/MathematicaL.cpp
    include/Display.h
    include/Worker.h
    include/MainWindow.h
    )

if (WIN32)
    link_directories("${CMAKE_SOURCE_DIR}/lib")

    target_link_libraries (ueye_link
            ids_peak ids_peak_ipl
            Qt6::Widgets Qt6::Core Qt6::Gui
            ${CMAKE_THREAD_LIBS_INIT}
            "${CMAKE_SOURCE_DIR}/lib/wstp64i4.lib"
        )
    target_link_libraries (test_wstp
            "${CMAKE_SOURCE_DIR}/lib/wstp64i4.lib"
        )

else()
    link_directories("${WSTPLINK_PATH}/CompilerAdditions")
    target_link_libraries (ueye_link
        ids_peak ids_peak_ipl
        Qt6::Widgets Qt6::Core Qt6::Gui
        rt uuid dl pthread "${WSTPLINK_PATH}/CompilerAdditions/libWSTP64i4.so"
        )

    target_link_libraries (test_wstp
        rt uuid dl pthread "${WSTPLINK_PATH}/CompilerAdditions/libWSTP64i4.so"
    )
endif()


