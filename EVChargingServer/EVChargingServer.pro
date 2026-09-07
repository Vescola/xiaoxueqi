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

# 源码树按 src 下的子目录引用, 例如 #include "db/databasemanager.h"
INCLUDEPATH += src

# 关闭 Qt3 兼容警告, 保持输出干净
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    src/main.cpp \
    src/common/appconfig.cpp \
    src/common/passwordutil.cpp \
    src/common/protocol.cpp \
    src/db/databasemanager.cpp \
    src/net/clientsession.cpp \
    src/net/sessionmanager.cpp \
    src/net/servercore.cpp \
    src/ui/launcherwindow.cpp \
    src/ui/logindialog.cpp \
    src/ui/mainwindow.cpp \
    src/ui/pages/trendchartwidget.cpp \
    src/ui/pages/salespage.cpp \
    src/ui/pages/stationpage.cpp \
    src/ui/pages/stationeditdialog.cpp \
    src/ui/pages/pilepage.cpp \
    src/ui/pages/chargereditdialog.cpp \
    src/ui/pages/userpage.cpp \
    src/ui/pages/orderpage.cpp

HEADERS += \
    src/common/appconfig.h \
    src/common/passwordutil.h \
    src/common/protocol.h \
    src/db/databasemanager.h \
    src/net/clientsession.h \
    src/net/sessionmanager.h \
    src/net/servercore.h \
    src/ui/launcherwindow.h \
    src/ui/logindialog.h \
    src/ui/mainwindow.h \
    src/ui/pages/trendchartwidget.h \
    src/ui/pages/salespage.h \
    src/ui/pages/stationpage.h \
    src/ui/pages/stationeditdialog.h \
    src/ui/pages/pilepage.h \
    src/ui/pages/chargereditdialog.h \
    src/ui/pages/userpage.h \
    src/ui/pages/orderpage.h

# Qt Designer 界面文件(双击即可在 Qt Creator 设计模式可视化编辑)
FORMS += \
    src/ui/launcherwindow.ui \
    src/ui/logindialog.ui \
    src/ui/mainwindow.ui \
    src/ui/pages/salespage.ui \
    src/ui/pages/stationpage.ui \
    src/ui/pages/stationeditdialog.ui \
    src/ui/pages/pilepage.ui \
    src/ui/pages/chargereditdialog.ui \
    src/ui/pages/userpage.ui \
    src/ui/pages/orderpage.ui

# uic 生成的 ui_*.h 需要能找到提升控件的头文件
INCLUDEPATH += src/ui/pages

unix:!macx {
    QMAKE_CXXFLAGS += -fPIC
}

DISTFILES += \
    README.md \
    config.ini.example
