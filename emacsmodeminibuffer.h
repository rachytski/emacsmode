#pragma once

#include <QStackedWidget>
#include <QTimer>

#include "emacsmodehandler.h"

class QLabel;
class QLineEdit;
class QObject;

namespace EmacsMode {
namespace Internal {

class MiniBuffer : public QStackedWidget
{
    Q_OBJECT

public:
    MiniBuffer();

    void setContents(const QString &contents, int cursorPos, int anchorPos,
                     int messageLevel, QObject *eventFilter);

    QSize sizeHint() const;

signals:
    void edited(const QString &text, int cursorPos, int anchorPos);

private slots:
    void changed();

private:
    QLabel *m_label;
    QLineEdit *m_edit;
    QObject *m_eventFilter;
    QTimer m_hideTimer;
    int m_lastMessageLevel;
};

}
}
