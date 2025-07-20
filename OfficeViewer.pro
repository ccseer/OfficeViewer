QT       += core gui widgets
TARGET  = officeviewer
CONFIG += c++17

# TEMPLATE = lib
# CONFIG += plugin
# TARGET_EXT = .dll
SOURCES += src/test.cpp

# src
SOURCES += \
    $$PWD/src/officeviewer.cpp
HEADERS += \
    $$PWD/src/officeviewer.h
DISTFILES += $$PWD/bin/plugin.json

# oit
INCLUDEPATH+= $$PWD/sdk/common

# seer
INCLUDEPATH += $$PWD/Seer-sdk/
HEADERS += \
    Seer-sdk/seer/viewerbase.h \
    Seer-sdk/seer/viewer_helper.h \
    Seer-sdk/seer/viewoption.h


VERSION = 1.4.1
QMAKE_TARGET_COMPANY = "1218.io"
QMAKE_TARGET_PRODUCT = "Seer"
QMAKE_TARGET_DESCRIPTION = "Seer - A Windows Quick Look Tool"
QMAKE_TARGET_COPYRIGHT = "Corey"


