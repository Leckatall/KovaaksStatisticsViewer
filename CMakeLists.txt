cmake_minimum_required(VERSION 3.21)
project(KovaaksStatsViewer VERSION 0.1.0)

# C++ Standard Configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Qt Configuration
set(Qt6_DIR C:/Qt/6.11.1/mingw_64/lib/cmake/Qt6)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Quick
        Qml
        QuickControls2
        Graphs
        Test  # Add for testing
)
qt_policy(SET QTP0004 NEW)


qt_standard_project_setup()



# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)

# Add subdirectories for modular libraries
add_subdirectory(src/app)
add_subdirectory(src/domain)
#add_subdirectory(src/presentation)
add_subdirectory(src/ui)
add_subdirectory(src/data)
add_subdirectory(src/qt_data)
#add_subdirectory(src/controllers)

# Main executable
add_executable(ksv
        src/main.cpp
        #        resources.qrc
)


# Link libraries to main executable
target_link_libraries(ksv PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Graphs
        ksv_app
        ksv_ui
        ksv_data
        ksv_qt_data
        ksv_domain
)

# The MinGW toolchain's libwinpthread-1.dll is required at runtime by
# vcpkg's libabseil_dll.dll, but only the compiler's own copy exports the
# pthread_*64 symbols abseil links against. Copy it next to the exe so the
# app runs standalone instead of only inside CLion (which puts the
# toolchain's bin/ on PATH itself).
get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
add_custom_command(TARGET ksv POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_BIN_DIR}/libwinpthread-1.dll"
        "$<TARGET_FILE_DIR:ksv>"
)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
#target_include_directories(myapp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

