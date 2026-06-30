QT += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = VertiCareQt
TEMPLATE = app
VERSION = 1.5.0
RC_ICONS = app.ico

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    onenetclient.cpp

HEADERS += \
    mainwindow.h \
    onenetclient.h

RESOURCES += \
    resources.qrc
