#include "emacsmodeminibuffer.h"

#include <QLabel>
#include <QLineEdit>

namespace EmacsMode{
namespace Internal {

MiniBuffer::MiniBuffer()
  : m_label(new QLabel(this))
  , m_edit(new QLineEdit(this))
  , m_eventFilter(0)
  , m_lastMessageLevel(MessageMode)
{
  connect(m_edit, SIGNAL(textEdited(QString)), SLOT(changed()));
  connect(m_edit, SIGNAL(cursorPositionChanged(int,int)), SLOT(changed()));
  connect(m_edit, SIGNAL(selectionChanged()), SLOT(changed()));
  m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

  addWidget(m_label);
  addWidget(m_edit);

  m_hideTimer.setSingleShot(true);
  m_hideTimer.setInterval(8000);
  connect(&m_hideTimer, SIGNAL(timeout()), SLOT(hide()));
}

void MiniBuffer::setContents(const QString &contents, int cursorPos, int anchorPos,
                     int messageLevel, QObject *eventFilter)
{
  if (cursorPos != -1) {
    m_edit->blockSignals(true);
    m_label->clear();
    m_edit->setText(contents);
    if (anchorPos != -1 && anchorPos != cursorPos)
      m_edit->setSelection(anchorPos, cursorPos - anchorPos);
    else
      m_edit->setCursorPosition(cursorPos);
    m_edit->blockSignals(false);
    setCurrentWidget(m_edit);
    m_edit->setFocus();
  } else {
    if (contents.isEmpty()) {
      if (m_lastMessageLevel == MessageMode)
        hide();
                else
        m_hideTimer.start();
    } else {
      m_hideTimer.stop();
      show();

      m_label->setText(contents);

      QString css;
      if (messageLevel == MessageError) {
        css = QLatin1String("border:1px solid rgba(255,255,255,150);"
                "background-color:rgba(255,0,0,100);");
                } else if (messageLevel == MessageWarning) {
        css = QLatin1String("border:1px solid rgba(255,255,255,120);"
                "background-color:rgba(255,255,0,20);");
      } else if (messageLevel == MessageShowCmd) {
        css = QLatin1String("border:1px solid rgba(255,255,255,120);"
                "background-color:rgba(100,255,100,30);");
      }
      m_label->setStyleSheet(QString::fromLatin1(
                    "*{border-radius:2px;padding-left:4px;padding-right:4px;%1}").arg(css));
    }

    if (m_edit->hasFocus())
      emit edited(QString(), -1, -1);

    setCurrentWidget(m_label);
  }

  if (m_eventFilter != eventFilter) {
    if (m_eventFilter != 0) {
      m_edit->removeEventFilter(m_eventFilter);
      disconnect(SIGNAL(edited(QString,int,int)));
    }
    if (eventFilter != 0) {
      m_edit->installEventFilter(eventFilter);
      connect(this, SIGNAL(edited(QString,int,int)),
              eventFilter, SLOT(miniBufferTextEdited(QString,int,int)));
    }
    m_eventFilter = eventFilter;
  }

  m_lastMessageLevel = messageLevel;
}

QSize MiniBuffer::sizeHint() const
{
  QSize size = QWidget::sizeHint();
  // reserve maximal width for line edit widget
  return currentWidget() == m_edit ? QSize(maximumWidth(), size.height()) : size;
}

void MiniBuffer::changed()
{
  const int cursorPos = m_edit->cursorPosition();
  int anchorPos = m_edit->selectionStart();
  if (anchorPos == cursorPos)
    anchorPos = cursorPos + m_edit->selectedText().length();
  emit edited(m_edit->text(), cursorPos, anchorPos);
}

}
}

