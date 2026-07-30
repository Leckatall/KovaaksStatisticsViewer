
add_library(ksv_app STATIC
        app.h
        app.cpp
        usecases/i_graph_use_case.h
        usecases/graph_use_case.h
)

target_link_libraries(ksv_app PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::Graphs
        ksv_ui
        ksv_data
        ksv_domain
)

target_include_directories(ksv_app PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)