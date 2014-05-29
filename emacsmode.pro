# CONFIG += single
include(../../qtcreatorplugin.pri)

QT += gui
SOURCES += emacsmodehandler.cpp \
    emacsmodeplugin.cpp \
    emacsmodeactions.cpp \
    emacsmodeshortcut.cpp

HEADERS += emacsmodehandler.h \
    emacsmodeplugin.h \
    emacsmodeactions.h \
    emacsmodeshortcut.h

FORMS += emacsmodeoptions.ui
QMAKE_CXXFLAGS = -mmacosx-version-min=10.7 -std=gnu0x -stdlib=libc+

CONFIG +=c++11

