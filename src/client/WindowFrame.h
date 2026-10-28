#pragma once
#include <QAbstractNativeEventFilter>
class QQuickWindow;
class WindowFrame : public QAbstractNativeEventFilter {
public:
    explicit WindowFrame(QQuickWindow *window);
    ~WindowFrame() override;
    bool nativeEventFilter(const QByteArray &type,void *message,qintptr *result) override;
private:
    QQuickWindow *m_window;
    quintptr m_handle=0;
};
