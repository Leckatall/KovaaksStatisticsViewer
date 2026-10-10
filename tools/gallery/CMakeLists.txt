# Synthetic profiles and the source-tree QML interceptor, shared by the dev
# tools (ksv_gallery here, ksv_driver in tools/driver).
add_library(ksv_gallery_support STATIC
        gallery_dataset.cpp
        gallery_dataset.h
        qml_source_interceptor.h
)
target_include_directories(ksv_gallery_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ksv_gallery_support PUBLIC
        Qt6::Core
        Qt6::Qml
        ksv_domain
)

qt_add_executable(ksv_gallery
        gallery_main.cpp
)

target_link_libraries(ksv_gallery PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        ksv_gallery_support
        ksv_app
        ksv_ui
        ksv_data
        ksv_qt_data
        ksv_domain
)

# QML is read from the source tree at runtime. The gallery's URL interceptor
# maps the compiled module's QML URLs back to this directory so qmlpreview can
# watch and hot-reload component edits.
target_compile_definitions(ksv_gallery PRIVATE
        GALLERY_QML_FILE="${CMAKE_CURRENT_SOURCE_DIR}/Gallery.qml"
        GALLERY_QML_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/ui/qml"
        QT_QML_DEBUG
)

ksv_copy_mingw_runtime_dlls(ksv_gallery)
