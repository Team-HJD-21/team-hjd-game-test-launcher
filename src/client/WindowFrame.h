#pragma once
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
class QQuickWindow;
class WindowFrame : public QAbstractNativeEventFilter {
public:
    explicit WindowFrame(QQuickWindow *window);
    ~WindowFrame() override;
    bool nativeEventFilter(const QByteArray &type,void *message,qintptr *result) override;
private:
    QPointer<QQuickWindow> m_window;
    QObject m_callbacks;
    quintptr m_handle=0;
    bool m_maximizeQueued=false;
};
