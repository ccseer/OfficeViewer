QT       += core gui widgets

SOURCES += \
    main.cpp \
    oitviewer.cpp

HEADERS += \
    oitviewer.h

FORMS += oitviewer.ui

INCLUDEPATH+= $$PWD/../sdk/common

VERSION = 1.3.0
include(../../../common.pri)

HEADERS += ../../../../github/FontViewer/oitvar.h
INCLUDEPATH += ../../../../github/FontViewer/


