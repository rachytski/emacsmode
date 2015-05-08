# CONFIG += single
include(../../qtcreatorplugin.pri)

QT += gui
SOURCES += emacsmodehandler.cpp \
    emacsmodeplugin.cpp \
    emacsmodeactions.cpp \
    emacsmodeshortcut.cpp \
    emacsmodeminibuffer.cpp \
    emacsmodeoptionpage.cpp

HEADERS += emacsmodehandler.h \
    emacsmodeplugin.h \
    emacsmodeactions.h \
    emacsmodeshortcut.h \
    emacsmodeminibuffer.h \
    emacsmodeoptionpage.h

FORMS += emacsmodeoptions.ui

!win* {
    QMAKE_CXXFLAGS = -mmacosx-version-min=10.7 -std=gnu0x -stdlib=libc+
}

CONFIG +=c++11

RESOURCES += \
    emacsmode.qrc

