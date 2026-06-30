QT += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = VertiCareQt
TEMPLATE = app
VERSION = 1.6.0
RC_ICONS = app.ico

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    onenetclient.cpp \
    telemetrystore.cpp

HEADERS += \
    mainwindow.h \
    onenetclient.h \
    telemetrystore.h

RESOURCES += \
    resources.qrc
