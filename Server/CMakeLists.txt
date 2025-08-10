cmake_minimum_required(VERSION 3.16)

project(Server VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Sql Core Qml)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Sql Core Qml)

# 设置OpenSSL路径（优先使用项目内的OpenSSL）
if(WIN32)
    # 优先使用项目内的OpenSSL
    set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/../third-libraries/OpenSSL")
    if(EXISTS "${OPENSSL_ROOT_DIR}")
        set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
        set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD/libcrypto.lib")
        set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/VC/x64/MD/libssl.lib")
        message(STATUS "Using project OpenSSL at: ${OPENSSL_ROOT_DIR}")
        
        # 验证库文件是否存在
        if(NOT EXISTS "${OPENSSL_CRYPTO_LIBRARY}")
            message(FATAL_ERROR "OpenSSL crypto library not found at: ${OPENSSL_CRYPTO_LIBRARY}")
        endif()
        if(NOT EXISTS "${OPENSSL_SSL_LIBRARY}")
            message(FATAL_ERROR "OpenSSL SSL library not found at: ${OPENSSL_SSL_LIBRARY}")
        endif()
        
        # 手动设置OpenSSL组件
        set(OpenSSL_FOUND TRUE)
        set(OpenSSL_INCLUDE_DIR "${OPENSSL_INCLUDE_DIR}")
        set(OpenSSL_CRYPTO_LIBRARY "${OPENSSL_CRYPTO_LIBRARY}")
        set(OpenSSL_SSL_LIBRARY "${OPENSSL_SSL_LIBRARY}")
        set(OpenSSL_LIBRARIES "${OpenSSL_SSL_LIBRARY};${OpenSSL_CRYPTO_LIBRARY}")
        set(OpenSSL_VERSION "3.5.1")
        
        message(STATUS "OpenSSL manually configured:")
        message(STATUS "  Include dir: ${OpenSSL_INCLUDE_DIR}")
        message(STATUS "  Crypto lib: ${OpenSSL_CRYPTO_LIBRARY}")
        message(STATUS "  SSL lib: ${OpenSSL_SSL_LIBRARY}")
    else()
        # 如果项目内没有OpenSSL，尝试使用系统安装的
        set(OPENSSL_ROOT_DIR "D:/Program Files/OpenSSL-Win64")
        set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
        set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib")
        set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.lib")
        message(STATUS "Using system OpenSSL at: ${OPENSSL_ROOT_DIR}")
        
        # 查找OpenSSL
        find_package(OpenSSL REQUIRED)
    endif()
else()
    # 非Windows系统，使用标准的find_package
    find_package(OpenSSL REQUIRED)
endif()

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui

        # 数据库模块
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/database/RedisClient.h
        src/database/RedisClient.cpp

        # 认证模块
        src/auth/UserService.h
        src/auth/UserService.cpp
        src/auth/EmailService.h
        src/auth/EmailService.cpp
        src/auth/SmtpClient.h
        src/auth/SmtpClient.cpp
        src/auth/VerificationCodeManager.h
        src/auth/VerificationCodeManager.cpp

        # 网络模块
        src/network/TcpServer.h
        src/network/TcpServer.cpp
        src/network/ClientHandler.h
        src/network/ClientHandler.cpp
        src/network/ProtocolHandler.h
        src/network/ProtocolHandler.cpp

        # 模型类
        src/models/User.h
        src/models/User.cpp

        # 工具类
        src/utils/Logger.h
        src/utils/Logger.cpp
        src/utils/Crypto.h
        src/utils/Crypto.cpp
        src/utils/Validator.h
        src/utils/Validator.cpp

        # 服务器管理器
        src/ServerManager.h
        src/ServerManager.cpp

        # 配置管理
        src/config/ConfigManager.h
        src/config/ConfigManager.cpp

        # 安全模块
        src/security/CertificateManager.h
        src/security/CertificateManager.cpp
        src/security/OpenSSLHelper.h
        src/security/OpenSSLHelper.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Server
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Server APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Server SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Server
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Server PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Qml
    ${OpenSSL_LIBRARIES}
)

# 包含OpenSSL头文件目录
target_include_directories(Server PRIVATE ${OPENSSL_INCLUDE_DIR})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Server)
endif()
set_target_properties(Server PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# 自动复制配置文件到构建目录
add_custom_command(TARGET Server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Server>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config/server.json
        $<TARGET_FILE_DIR:Server>/config/server.json
    COMMENT "Copying configuration files to build directory"
)

include(GNUInstallDirs)
install(TARGETS Server
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 安装配置文件
install(FILES config/server.json
    DESTINATION ${CMAKE_INSTALL_BINDIR}/config
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Server)
endif()
