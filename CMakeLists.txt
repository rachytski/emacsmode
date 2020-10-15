if (WITH_TESTS)
  set(TST_COMPONENT CppEditor CppTools)
endif()

add_qtc_plugin(EmacsMode
  PLUGIN_DEPENDS Core TextEditor ${TST_COMPONENT}
  SOURCES ${TEST_SOURCES}
    emacsmode.qrc
    emacsmodeplugin.cpp emacsmodeplugin.h
    emacsmodesettings.cpp emacsmodesettings.h
    emacsmodeshortcut.cpp emacsmodeshortcut.h
    emacsmodehandler.cpp emacsmodehandler.h
    emacsmodeoptionpage.cpp emacsmodeoptionpage.h
    minibuffer.cpp minibuffer.hpp
    pluginstate.cpp pluginstate.hpp
    range.cpp range.hpp
    emacsmodeoptions.ui
)

extend_qtc_plugin(EmacsMode
  CONDITION WITH_TESTS
  SOURCES emacsmode_test.cpp
)
