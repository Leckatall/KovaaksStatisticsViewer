cmake_minimum_required(VERSION 3.21)
project(KovaaksStatsViewer VERSION 0.4.0)

# project(VERSION) accepts numeric fields only, so the pre-release suffix lives here. This file is the
# single source of the version: scripts/version.ps1 parses the project() call and the set() below out
# of it by regex, so keep their literal shape.
set(KSV_VERSION_SUFFIX "-alpha")
set(KSV_VERSION "${PROJECT_VERSION}${KSV_VERSION_SUFFIX}")

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
        Test  # Add for testing
        QuickTest
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
        ksv_app
        ksv_ui
        ksv_data
        ksv_qt_data
        ksv_domain
)

# The MinGW toolchain's runtime DLLs (libwinpthread-1, libgcc_s_seh-1,
# libstdc++-6) are required at process startup by anything that transitively
# links vcpkg's libabseil_dll.dll (protobuf), but only the compiler's own
# copy exports the ABI these were built against. Several MinGW toolchains
# can be installed side by side (Git, Qt, CLion's bundled one), and Windows'
# DLL search order can silently pick up an incompatible one from PATH
# instead, producing STATUS_ENTRY_POINT_NOT_FOUND / STATUS_DLL_NOT_FOUND at
# startup even though linking succeeded. Copy the exact DLLs from the
# compiler actually used for this build next to any target that needs them,
# so binaries run standalone regardless of what else is on PATH.
get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
function(ksv_copy_mingw_runtime_dlls target)
    foreach(_mingw_runtime_dll libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll)
        if (EXISTS "${MINGW_BIN_DIR}/${_mingw_runtime_dll}")
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MINGW_BIN_DIR}/${_mingw_runtime_dll}"
                    "$<TARGET_FILE_DIR:${target}>"
            )
        endif()
    endforeach()
endfunction()

ksv_copy_mingw_runtime_dlls(ksv)

# Dev-only component gallery (qmlpreview target). Off for packaged builds.
option(KSV_BUILD_GALLERY "Build the dev component gallery (ksv_gallery)" ON)
if(KSV_BUILD_GALLERY)
    add_subdirectory(tools/gallery)
endif()

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
#target_include_directories(myapp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

