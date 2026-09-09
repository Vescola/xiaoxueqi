QT += core gui widgets sql network

CONFIG += c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ChargingUserClient

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    databasemanager.cpp \
    chargingworker.cpp \
    clientprotocol.cpp \
    techpage.cpp \
    mousetrailwidget.cpp \
    serverapiclient.cpp

HEADERS += \
    mainwindow.h \
    databasemanager.h \
    chargingworker.h \
    clientprotocol.h \
    models.h \
    techpage.h \
    mousetrailwidget.h \
    serverapiclient.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc
