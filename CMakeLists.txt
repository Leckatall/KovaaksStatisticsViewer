cmake_minimum_required(VERSION 3.21)
project(KovaaksStatsViewer VERSION 0.0.1)

# C++ Standard Configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Qt Configuration
set(ENV{QTFRAMEWORK_BYPASS_LICENSE_CHECK} 1)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Network
        Svg
        Test  # Add for testing
)
# Find Protobuf
find_package(Protobuf REQUIRED)

# Configure Protobuf

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)

# Add subdirectories for modular libraries
add_subdirectory(src/domain)
#add_subdirectory(src/infrastructure)
#add_subdirectory(src/presentation)
add_subdirectory(src/ui)
add_subdirectory(src/data)
#add_subdirectory(src/controllers)

# Main executable
add_executable(myapp
        src/main.cpp
        src/app/app.cpp
        src/app/app.h
        #        resources.qrc
)


# Link libraries to main executable
target_link_libraries(myapp PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        ksv_ui
        ksv_data
        ksv_domain
)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
#target_include_directories(myapp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

