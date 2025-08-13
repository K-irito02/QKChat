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
    src/models/FriendGroupManager.h
    src/models/FriendGroupManager.cpp
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
    src/chat/ChatNetworkClient.h
    src/chat/ChatNetworkClient.cpp
)

# 添加QML资源文件
qt_add_resources(appClient "qml"
    PREFIX "/"
    FILES
        Main.qml
        qml/LoginPage.qml
        qml/RegisterPage.qml
        qml/pages/MainPage.qml
        qml/windows/LoginWindow.qml
        qml/windows/ChatWindow.qml
        qml/components/ThemeManager.qml
        qml/components/CustomTextField.qml
        qml/components/CustomButton.qml
        qml/components/CustomDropdownButton.qml
        qml/components/LoadingDialog.qml
        qml/components/MessageDialog.qml
        qml/components/SettingsManager.qml
        qml/components/PageTransition.qml
        qml/components/FriendListView.qml
        qml/components/FriendListItem.qml
        qml/components/ChatView.qml
        qml/components/MessageBubble.qml
        qml/components/UserSearchDialog.qml
        qml/components/UserSearchResultItem.qml
        qml/components/GroupHeader.qml
        qml/components/GroupManagementDialog.qml
        qml/components/GroupedListView.qml
        qml/windows/UserDetailWindow.qml
        qml/windows/AddFriendWindow.qml
        qml/components/qmldir
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
