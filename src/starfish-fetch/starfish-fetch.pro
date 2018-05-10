TEMPLATE = app
TARGET = starfish-fetch

QT -= gui
QT += xml network
CONFIG += console


SOURCES += starfish-fetch.cpp

target.path = /usr/bin
INSTALLS += target



include(../../common.pri)

CONFIG(standalone) {
    include(../starfish-lib/src.pri)
} else {
    LIBS += -L../starfish-lib -lstarfish
}


