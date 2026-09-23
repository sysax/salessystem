cmake_minimum_required(VERSION 3.28)

find_package(Qt6 REQUIRED COMPONENTS Test)

# Ruta a tests/fixtures (seed demo solo-tests) para TestDb.h
add_compile_definitions(TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

function(add_qt_test name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE sales_core Qt6::Test Qt6::Sql Qt6::Core)
    add_test(NAME ${name} COMMAND ${name})
    # QTEST_MAIN instancia QApplication: forzar offscreen en CI/headless
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_qt_test(tst_totp tst_totp.cpp)
add_qt_test(tst_auth tst_auth.cpp)
add_qt_test(tst_money tst_money.cpp)
add_qt_test(tst_repositories tst_repositories.cpp)
add_qt_test(tst_sales_service tst_sales_service.cpp)
add_qt_test(tst_inventory_service tst_inventory_service.cpp)
add_qt_test(tst_report tst_report.cpp)
add_qt_test(tst_sync tst_sync.cpp)
add_qt_test(tst_ticket tst_ticket.cpp)
add_qt_test(tst_controllers tst_controllers.cpp)
add_qt_test(tst_mgmt tst_mgmt.cpp)
add_qt_test(tst_settings tst_settings.cpp)
add_qt_test(tst_tax tst_tax.cpp)
add_qt_test(tst_catalog tst_catalog.cpp)
add_qt_test(tst_vertical tst_vertical.cpp)

add_executable(tst_sidebar tst_sidebar.cpp)
target_link_libraries(tst_sidebar PRIVATE sales_core Qt6::Test Qt6::Sql Qt6::Core
                                          Qt6::Gui Qt6::Qml Qt6::Quick)
target_compile_definitions(tst_sidebar PRIVATE QML_DIR="${CMAKE_SOURCE_DIR}/qml")
add_test(NAME tst_sidebar COMMAND tst_sidebar)
set_tests_properties(tst_sidebar PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
