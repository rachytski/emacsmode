# CONFIG += single
include(../../qtcreatorplugin.pri)

QT += gui
SOURCES += emacsmodehandler.cpp \
    emacsmodeplugin.cpp \
    emacsmodesettings.cpp \
    emacsmodeshortcut.cpp \
    emacsmodeminibuffer.cpp \
    emacsmodeoptionpage.cpp \ 
    pluginstate.cpp \

HEADERS += emacsmodehandler.h \
    emacsmodeplugin.h \
    emacsmodesettings.h \
    emacsmodeshortcut.h \
    emacsmodeminibuffer.h \
    emacsmodeoptionpage.h \
    pluginstate.hpp \

FORMS += emacsmodeoptions.ui

macx {
    QMAKE_CXXFLAGS = -mmacosx-version-min=10.7 -std=gnu0x -stdlib=libc+
}

CONFIG +=c++11

RESOURCES += \
    emacsmode.qrc

