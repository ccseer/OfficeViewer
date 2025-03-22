QT       += core gui widgets

SOURCES += \
    main.cpp \
    oitviewer.cpp

HEADERS += \
    oitviewer.h

FORMS += oitviewer.ui

INCLUDEPATH+= $$PWD/../sdk/common


HEADERS += ../Seer-sdk/seer/embedplugin.h
INCLUDEPATH +=  ../Seer-sdk/



VERSION = 1.4.0
QMAKE_TARGET_COMPANY = "1218.io"
QMAKE_TARGET_PRODUCT = "Seer"
QMAKE_TARGET_DESCRIPTION = "Seer - A Windows Quick Look Tool"
QMAKE_TARGET_COPYRIGHT = "Corey"
