#-------------------------------------------------------------
# 东软电动汽车充电桩应用管理平台 - PC 服务器端
#
# 目标环境 : Qt Creator 6.0.2 / Qt 5.15.3 (GCC 11.2.0, 64 bit)
# 构建系统 : qmake
# 依赖模块 : widgets + sql + network
#            图表由 QPainter 手绘, 不依赖 Qt Charts 模块
#
# 打开方式 : Qt Creator -> 打开工程 -> 选择本 .pro -> 选 5.15.3 Kit
#-------------------------------------------------------------

QT       += core gui widgets sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET    = EVChargingServer
TEMPLATE  = app

CONFIG   += c++14
CONFIG   += thread

# 源码树按 src 下的子目录引用, 例如 #include "db/ServerDb.h"
INCLUDEPATH += src

# 关闭 Qt3 兼容警告, 保持输出干净
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/common/AppConfig.cpp \
    src/common/PasswordUtil.cpp \
    src/common/Protocol.cpp \
    src/db/ServerDb.cpp \
    src/net/ClientSession.cpp \
    src/net/SessionManager.cpp \
    src/net/ServerCore.cpp \
    src/ui/LauncherWindow.cpp \
    src/ui/LoginDialog.cpp \
    src/ui/MainWindow.cpp \
    src/ui/pages/TrendChartWidget.cpp \
    src/ui/pages/SalesPage.cpp \
    src/ui/pages/StationPage.cpp \
    src/ui/pages/PilePage.cpp \
    src/ui/pages/UserPage.cpp \
    src/ui/pages/OrderPage.cpp

HEADERS += \
    src/common/AppConfig.h \
    src/common/PasswordUtil.h \
    src/common/Protocol.h \
    src/db/DbTypes.h \
    src/db/ServerDb.h \
    src/net/ClientSession.h \
    src/net/SessionManager.h \
    src/net/ServerCore.h \
    src/ui/LauncherWindow.h \
    src/ui/LoginDialog.h \
    src/ui/MainWindow.h \
    src/ui/pages/TrendChartWidget.h \
    src/ui/pages/SalesPage.h \
    src/ui/pages/StationPage.h \
    src/ui/pages/PilePage.h \
    src/ui/pages/UserPage.h \
    src/ui/pages/OrderPage.h

# Qt Designer 界面文件(双击即可在 Qt Creator 设计模式可视化编辑)
FORMS += \
    src/ui/LauncherWindow.ui \
    src/ui/LoginDialog.ui \
    src/ui/MainWindow.ui \
    src/ui/pages/SalesPage.ui \
    src/ui/pages/StationPage.ui \
    src/ui/pages/PilePage.ui \
    src/ui/pages/UserPage.ui \
    src/ui/pages/OrderPage.ui

# uic 生成的 ui_*.h 需要能找到提升控件的头文件
INCLUDEPATH += src/ui/pages

unix:!macx {
    QMAKE_CXXFLAGS += -fPIC
}

DISTFILES += \
    README.md \
    config.ini.example
