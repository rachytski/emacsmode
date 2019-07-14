# CONFIG += single
include(../../qtcreatorplugin.pri)

QT += gui
SOURCES += emacsmodehandler.cpp \
    emacsmodeplugin.cpp \
    emacsmodesettings.cpp \
    emacsmodeshortcut.cpp \
    emacsmodeoptionpage.cpp \ 
    minibuffer.cpp \
    pluginstate.cpp \
    range.cpp \

HEADERS += emacsmodehandler.h \
    emacsmodeplugin.h \
    emacsmodesettings.h \
    emacsmodeshortcut.h \
    emacsmodeoptionpage.h \
    minibuffer.hpp \
    pluginstate.hpp \
    range.hpp \

FORMS += emacsmodeoptions.ui

macx {
    QMAKE_CXXFLAGS = -mmacosx-version-min=10.7 -std=gnu0x -stdlib=libc+
}

CONFIG +=c++11

RESOURCES += \
    emacsmode.qrc

