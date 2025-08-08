cmake_minimum_required(VERSION 3.16)

project(Client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network Sql Core5Compat)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appClient
    main.cpp
    src/DatabaseManager.h
    src/DatabaseManager.cpp
    src/auth/AuthManager.h
    src/auth/AuthManager.cpp
    src/auth/NetworkClient.h
    src/auth/NetworkClient.cpp
    src/auth/SessionManager.h
    src/auth/SessionManager.cpp
    src/models/User.h
    src/models/User.cpp
    src/models/AuthResponse.h
    src/models/AuthResponse.cpp
    src/utils/Validator.h
    src/utils/Validator.cpp
    src/utils/Crypto.h
    src/utils/Crypto.cpp
    src/utils/Logger.h
    src/utils/Logger.cpp
    src/utils/NetworkQualityMonitor.h
    src/utils/NetworkQualityMonitor.cpp
    src/utils/SmartErrorHandler.h
    src/utils/SmartErrorHandler.cpp
)

qt_add_qml_module(appClient
    URI Client
    VERSION 1.0
    QML_FILES
        Main.qml
        qml/LoginPage.qml
        qml/RegisterPage.qml
        qml/pages/MainPage.qml
        qml/components/ThemeManager.qml
        qml/components/LoadingDialog.qml
        qml/components/MessageDialog.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appClient PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appClient
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appClient
    PRIVATE Qt6::Quick Qt6::Network Qt6::Sql Qt6::Core5Compat
)

include(GNUInstallDirs)
install(TARGETS appClient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
