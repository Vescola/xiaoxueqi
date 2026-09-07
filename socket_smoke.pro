QT += core network gui

CONFIG += console c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = socket_smoke

INCLUDEPATH += /home/bit/ChargingUserClient_UserClient

SOURCES += \
    /home/bit/socket_smoke.cpp \
    /home/bit/ChargingUserClient_UserClient/clientprotocol.cpp \
    /home/bit/ChargingUserClient_UserClient/serverapiclient.cpp

HEADERS += \
    /home/bit/ChargingUserClient_UserClient/clientprotocol.h \
    /home/bit/ChargingUserClient_UserClient/models.h \
    /home/bit/ChargingUserClient_UserClient/serverapiclient.h
