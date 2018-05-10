TEMPLATE = app
TARGET = test

QT -= gui

CONFIG += console

QT += core testlib xml network

SOURCES += \
    TestParser.cpp


include(../../common.pri)

include(../starfish-lib/src.pri)

INCLUDEPATH += ../starfish-lib
