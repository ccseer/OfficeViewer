QT       += core gui widgets

SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/oitviewer.cpp

HEADERS += \
    $$PWD/src/oitviewer.h

FORMS +=  $$PWD/src/oitviewer.ui

INCLUDEPATH+= $$PWD/sdk/common


HEADERS += $$PWD/Seer-sdk/seer/embedplugin.h
INCLUDEPATH += $$PWD/Seer-sdk/



VERSION = 1.4.0
QMAKE_TARGET_COMPANY = "1218.io"
QMAKE_TARGET_PRODUCT = "Seer"
QMAKE_TARGET_DESCRIPTION = "Seer - A Windows Quick Look Tool"
QMAKE_TARGET_COPYRIGHT = "Corey"
