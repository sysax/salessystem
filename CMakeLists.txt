cmake_minimum_required(VERSION 3.28)

project(QtSalesSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6.11 en ~/Qt (no está en PATH): pasar
# -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/gcc_64
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Sql Concurrent Test Network Gui)

qt_standard_project_setup()

# Lógica compartida app + tests (sin main.cpp)
add_library(sales_core STATIC
    src/core/DatabaseManager.h
    src/core/DatabaseManager.cpp
    src/core/EventBus.h
    src/core/EventBus.cpp
    src/core/Money.h
    src/core/Result.h
    src/domain/Entities.h
    src/repositories/Counters.h
    src/repositories/AuditRepository.h
    src/repositories/AuditRepository.cpp
    src/repositories/ProductRepository.h
    src/repositories/ProductRepository.cpp
    src/repositories/ClientRepository.h
    src/repositories/ClientRepository.cpp
    src/repositories/PromoRepository.h
    src/repositories/PromoRepository.cpp
    src/repositories/CajaRepository.h
    src/repositories/CajaRepository.cpp
    src/repositories/SaleRepository.h
    src/repositories/SaleRepository.cpp
    src/repositories/InventoryRepository.h
    src/repositories/InventoryRepository.cpp
    src/repositories/PurchaseRepository.h
    src/repositories/PurchaseRepository.cpp
    src/repositories/CreditRepository.h
    src/repositories/CreditRepository.cpp
    src/services/Totp.h
    src/services/Totp.cpp
    src/services/AuthService.h
    src/services/AuthService.cpp
    src/services/Dian.h
    src/services/Dian.cpp
    src/services/SalesService.h
    src/services/SalesService.cpp
    src/services/InventoryService.h
    src/services/InventoryService.cpp
    src/services/PurchaseService.h
    src/services/PurchaseService.cpp
    src/services/ReportService.h
    src/services/ReportService.cpp
    src/services/SyncService.h
    src/services/SyncService.cpp
    src/services/CreditService.h
    src/services/CreditService.cpp
    src/services/TicketPrinter.h
    src/services/TicketPrinter.cpp
    src/controllers/AuthController.h
    src/controllers/AuthController.cpp
    src/controllers/DashboardController.h
    src/controllers/DashboardController.cpp
    src/controllers/CatalogController.h
    src/controllers/CatalogController.cpp
    src/controllers/PosController.h
    src/controllers/PosController.cpp
    src/controllers/SalesController.h
    src/controllers/SalesController.cpp
    src/controllers/MgmtControllers.h
    src/controllers/MgmtControllers.cpp
    src/controllers/OpsControllers.h
    src/controllers/OpsControllers.cpp
)
target_link_libraries(sales_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Network
)
target_include_directories(sales_core PUBLIC src)

# SQL embebido en sales_core: lo usan la app y los tests por igual
# (fallback a sql/*.sql en disco para iterar sin recompilar).
qt_add_resources(sales_core "sql_files"
    PREFIX "/sql"
    BASE "sql"
    FILES
        sql/schema.sql
        sql/seed.sql
        sql/seed_min.sql
)

qt_add_executable(QtSalesSystemApp
    src/main.cpp
)

# Qt 6.5+ policy QTP0004: mantener import relativo "components" sin qmldir
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 OLD)
endif()

# Theme.qml es singleton del módulo (pragma Singleton + este flag).
# Debe ir ANTES de qt_add_qml_module: el módulo lee las propiedades al crearse.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(QtSalesSystemApp
    URI QtSalesSystem
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/AppSidebar.qml
        qml/Card.qml
        qml/LoginPage.qml
        qml/DashboardPage.qml
        qml/PosPage.qml
        qml/ProductsPage.qml
        qml/SalesPage.qml
        qml/ClientsPage.qml
        qml/SuppliersPage.qml
        qml/InventoryPage.qml
        qml/PurchasesPage.qml
        qml/ReceivablesPage.qml
        qml/PayablesPage.qml
        qml/ReportsPage.qml
        qml/PromosPage.qml
        qml/UsersPage.qml
        qml/Utils.js
        qml/components/Toast.qml
        qml/components/LoadingOverlay.qml
        qml/components/SortHeader.qml
        qml/components/Pager.qml
        qml/components/EmptyState.qml
        qml/components/ConfirmDialog.qml
)

target_link_libraries(QtSalesSystemApp PRIVATE
    sales_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Concurrent
)

# El módulo QML genera un directorio "QtSalesSystem/" en el build;
# renombrar solo el binario para evitar la colisión.
set_target_properties(QtSalesSystemApp PROPERTIES OUTPUT_NAME qtsales)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS QtSalesSystemApp
    RUNTIME DESTINATION bin
)
