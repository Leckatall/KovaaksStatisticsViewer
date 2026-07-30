
#target_link_libraries(ksv_ui PUBLIC
#        Qt6::Core
#        Qt6::Gui
#)
#

qt_policy(SET QTP0004 NEW)
qt_policy(SET QTP0001 NEW)

add_library(ksv_ui STATIC)

target_link_libraries(ksv_ui PRIVATE Qt6::Core Qt6::Quick Qt6::Qml Qt6::Graphs)

qt_add_qml_module(ksv_ui
        URI KovaaksStatsViewer
        VERSION 1.0
        NO_PLUGIN
        OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer
        QML_FILES
            qml/Main.qml
        SOURCES
            presentation/graph_vm.h
            presentation/graph_vm.cpp
)

#set(QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR}/KovaaksStatsViewer)

target_include_directories(ksv_ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/presentation
)