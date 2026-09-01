qt_add_executable(ksv_gallery
        gallery_main.cpp
        gallery_dataset.cpp
        gallery_dataset.h
)

target_link_libraries(ksv_gallery PRIVATE
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

# QML is read from the source tree at runtime. The gallery's URL interceptor
# maps the compiled module's QML URLs back to this directory so qmlpreview can
# watch and hot-reload component edits.
target_compile_definitions(ksv_gallery PRIVATE
        GALLERY_QML_FILE="${CMAKE_CURRENT_SOURCE_DIR}/Gallery.qml"
        GALLERY_QML_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/ui/qml"
        QT_QML_DEBUG
)

ksv_copy_mingw_runtime_dlls(ksv_gallery)
