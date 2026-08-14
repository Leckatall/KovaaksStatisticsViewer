
add_library(ksv_app STATIC
        app.h
        app.cpp
        usecases/graph_use_case.h
        usecases/graph_column_preferences.h
        usecases/graph_column_preferences.cpp
        usecases/perf_column_builder.h
        usecases/perf_column_builder.cpp
        usecases/playtime_graph_use_case.h
        usecases/completion_history_use_case.h
        profile_build_worker.cpp
        profile_build_worker.h
        session_controller.cpp
        session_controller.h
        usecases/i_session_controller.h
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
        ksv_contracts
)

target_include_directories(ksv_app PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
