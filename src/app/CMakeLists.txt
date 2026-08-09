
add_library(ksv_app STATIC
        app.h
        app.cpp
        usecases/i_graph_use_case.h
        usecases/graph_use_case.h
        usecases/graph_column.h
        usecases/perf_column_builder.h
        usecases/perf_column_builder.cpp
        usecases/i_playtime_graph_use_case.h
        usecases/playtime_graph_use_case.h
        session_controller.cpp
        session_controller.h
        usecases/i_session_controller.h
        usecases/run_summary.h
)

target_link_libraries(ksv_app PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        ksv_ui
        ksv_qt_data
        ksv_data
        ksv_domain
)

target_include_directories(ksv_app PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)