cmake_minimum_required(VERSION 3.25)
project(KovaaksStatsViewer VERSION 1.0.0)

# project(VERSION) accepts numeric fields only, so the pre-release suffix lives here. This file is the
# single source of the version: scripts/version.ps1 parses the project() call and the set() below out
# of it by regex, so keep their literal shape.
set(KSV_VERSION_SUFFIX "-rc.1")
set(KSV_VERSION "${PROJECT_VERSION}${KSV_VERSION_SUFFIX}")

# C++ Standard Configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Qt Configuration
if(NOT Qt6_DIR)
    set(Qt6_DIR C:/Qt/6.12.0/msvc2022_64/lib/cmake/Qt6 CACHE PATH "Qt6 package directory")
endif()
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
        Svg  # qsvg image-format plugin, for SVG button icons
        Test  # Add for testing
        QuickTest
        Network  # ksv_driver's localhost automation socket (dev tool only)
)
qt_policy(SET QTP0004 NEW)


qt_standard_project_setup()


# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)

# qDebug()/qCDebug() calls compile to nothing in Release. Must be set here, before the
# add_subdirectory() calls: the per-layer static libs (ksv_app, ksv_ui, ...) don't inherit
# compile definitions scoped PRIVATE to the ksv executable target.
add_compile_definitions($<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>)

# Add subdirectories for modular libraries
add_subdirectory(src/domain)
add_subdirectory(src/app/contracts)
add_subdirectory(src/app)
#add_subdirectory(src/presentation)
add_subdirectory(src/ui)
add_subdirectory(src/data)
add_subdirectory(src/qt_data)
#add_subdirectory(src/controllers)

# Main executable
qt_add_executable(ksv
        src/main.cpp
        #        resources.qrc
)


target_compile_definitions(ksv PRIVATE KSV_VERSION="${KSV_VERSION}")

# GUI subsystem in Release so Windows spawns no console window; console-subsystem in Debug
# so qDebug()/qWarning() keep landing in the run window.
set_target_properties(ksv PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

# Link libraries to main executable
target_link_libraries(ksv PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::Svg
        ksv_app
        ksv_ui
        ksv_data
        ksv_qt_data
        ksv_domain
)

# Dev-only tools: the component gallery (qmlpreview target) and the UI automation driver
# (screenshots + scripted input for agents). Excluded from Release and packaged builds.
option(KSV_BUILD_GALLERY "Build the dev tools (ksv_gallery, ksv_driver)" ON)
if(KSV_BUILD_GALLERY AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_subdirectory(tools/gallery)
    add_subdirectory(tools/driver)
endif()

include(CTest)

if(BUILD_TESTING)
    # CMake's generated header verifier is excluded from ordinary builds.
    add_test(NAME contracts_qt_free
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                    --target ksv_contracts_verify_interface_header_sets)
    add_subdirectory(tests)
endif()

