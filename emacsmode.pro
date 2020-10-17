# CONFIG += single
include(../../qtcreatorplugin.pri)

QT += gui
SOURCES += emacsmodehandler.cpp \
    emacsmodeplugin.cpp \
    emacsmodesettings.cpp \
    shortcut.cpp \
    emacsmodeoptionpage.cpp \ 
    minibuffer.cpp \
    pluginstate.cpp \
    range.cpp \

HEADERS += emacsmodehandler.h \
    emacsmodeplugin.h \
    emacsmodesettings.h \
    shortcut.hpp \
    emacsmodeoptionpage.h \
    minibuffer.hpp \
    pluginstate.hpp \
    range.hpp \

FORMS += emacsmodeoptions.ui

CONFIG +=c++11

RESOURCES += \
    emacsmode.qrc

