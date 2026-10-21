set(env{TEST_FILE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/examples)

# One CTest test per GoogleTest executable. Each writes its XML report here, and the build runner
# expands a failing executable into its failing cases from that report.
set(KSV_GTEST_REPORT_DIR ${CMAKE_BINARY_DIR}/tests/gtest-reports)
file(MAKE_DIRECTORY ${KSV_GTEST_REPORT_DIR})

function(ksv_add_gtest_test target)
    add_test(NAME ${target}
        COMMAND ${target} --gtest_brief=1 --gtest_output=xml:${KSV_GTEST_REPORT_DIR}/${target}.xml
    )
endfunction()

# Test targets that carry a target_precompile_headers keep their own PCH rather than sharing one
# via REUSE_FROM: their compile definitions (TEST_FILES_DIR, KSV_GTEST_GUI_APP, KSV_UI_QML_DIR)
# and Qt module sets differ, and GCC rejects a PCH built under different ones.
add_subdirectory(support)
add_subdirectory(domain)
add_subdirectory(data)
add_subdirectory(qt_data)
add_subdirectory(app)
add_subdirectory(ui)
add_subdirectory(integration)

