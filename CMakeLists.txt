if (WITH_TESTS)
  set(TST_COMPONENT CppEditor CppTools)
endif()

add_qtc_plugin(EmacsMode
  PLUGIN_DEPENDS Core TextEditor ${TST_COMPONENT}
  SOURCES ${TEST_SOURCES}
    emacsmode.qrc
    emacsmodeplugin.cpp emacsmodeplugin.hpp
    emacsmodesettings.cpp emacsmodesettings.hpp
    shortcut.cpp shortcut.hpp
    emacsmodehandler.cpp emacsmodehandler.hpp
    emacsmodeoptionpage.cpp emacsmodeoptionpage.hpp
    killring.cpp killring.hpp
    action.cpp action.hpp
    minibuffer.cpp minibuffer.hpp
    pluginstate.cpp pluginstate.hpp
    range.cpp range.hpp
    emacsmodeoptions.ui
)

extend_qtc_plugin(EmacsMode
  CONDITION WITH_TESTS
  SOURCES emacsmode_test.cpp
)
