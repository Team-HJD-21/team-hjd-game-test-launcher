#include "client/WindowFrame.h"
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QQuickWindow>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <windows.h>

namespace {
QRect nativeClientGeometry(HWND handle) {
    RECT client{};
    POINT origin{};
    if(!IsWindow(handle)||!GetClientRect(handle,&client)||!ClientToScreen(handle,&origin))return {};
    return {origin.x,origin.y,client.right-client.left,client.bottom-client.top};
}
bool sameRect(const QRect &actual,const QRect &expected) {
    return qAbs(actual.x()-expected.x())<=1&&qAbs(actual.y()-expected.y())<=1&&
           qAbs(actual.width()-expected.width())<=1&&qAbs(actual.height()-expected.height())<=1;
}
QString rectText(const QRect &rect) {
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}
}

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);
    QQuickWindow window;
    // Exercise the same top-level window type as the product. A Tool/no-focus
    // window has different native show/maximize behavior on Windows.
    window.setFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowTransparentForInput);
    window.setOpacity(0.0);
    window.resize(900,650);
    window.show();
    WindowFrame frame(&window);
    MSG message{};
    message.hwnd=reinterpret_cast<HWND>(window.winId());
    message.wParam=HTMAXBUTTON;
    for(const auto event:{WM_NCLBUTTONDOWN,WM_NCLBUTTONUP,WM_NCCALCSIZE}) {
        message.message=event;
        if(frame.nativeEventFilter("windows_dispatcher_MSG",&message,nullptr)) {
            QTextStream(stderr)<<"FAIL unrelated dispatcher messages were consumed\n";
            return 1;
        }
    }
    for(const auto event:{WM_NCCALCSIZE,WM_NCHITTEST}) {
        message.message=event;
        if(frame.nativeEventFilter("windows_generic_MSG",&message,nullptr)) {
            QTextStream(stderr)<<"FAIL geometry query without a result was consumed\n";
            return 1;
        }
    }
    qintptr result=123;
    message.message=WM_NCLBUTTONUP;
    if(frame.nativeEventFilter("windows_dispatcher_MSG",&message,&result)||result!=123) {
        QTextStream(stderr)<<"FAIL non-window dispatcher message was changed\n";
        return 2;
    }
    int transitions=0;
    QElapsedTimer transitionWait;
    QRect normalGeometry,normalNativeGeometry;
    auto toggle=[&] {
        const auto before=window.visibility();
        // Exercise both maximize and restore through Qt's real null-result
        // input contract, as well as the valid-result callback contract.
        const bool noResult=transitions%4<2;
        for(const auto event:{WM_NCLBUTTONDOWN,WM_NCLBUTTONUP}) {
            message.message=event;
            result=123;
            if(!frame.nativeEventFilter("windows_generic_MSG",&message,noResult?nullptr:&result)||(!noResult&&result!=0)) {
                QTextStream(stderr)<<"FAIL native maximize button was not handled\n";
                app.exit(3);
                return false;
            }
        }
        if(window.visibility()!=before) {
            QTextStream(stderr)<<"FAIL window state changed inside the native callback\n";
            app.exit(4);
            return false;
        }
        ++transitions;
        transitionWait.start();
        return true;
    };
    QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,&app,[&] {
        const auto currentHandle=reinterpret_cast<HWND>(window.winId());
        if(currentHandle!=message.hwnd||!IsWindow(currentHandle)) {
            QTextStream(stderr)<<"FAIL native window handle changed or became invalid\n";
            app.exit(5);
            return;
        }
        const auto nativeGeometry=nativeClientGeometry(currentHandle);
        if(!transitions) {
            normalGeometry=window.geometry();
            normalNativeGeometry=nativeGeometry;
        }
        if(transitions) {
            const bool maximized=transitions%2==1;
            MONITORINFO monitor{sizeof(monitor)};
            if(!GetMonitorInfoW(MonitorFromWindow(currentHandle,MONITOR_DEFAULTTONEAREST),&monitor)) {
                QTextStream(stderr)<<"FAIL native monitor work area unavailable\n";
                app.exit(5);
                return;
            }
            const QRect workArea(monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right-monitor.rcWork.left,monitor.rcWork.bottom-monitor.rcWork.top);
            const auto expectedQt=maximized?window.screen()->availableGeometry():normalGeometry;
            const auto expectedNative=maximized?workArea:normalNativeGeometry;
            // Frameless Qt windows can maximize by setting work-area geometry
            // without setting WS_MAXIMIZE. Check actual client bounds, not that
            // optional native style bit reported by IsZoomed().
            if(window.visibility()!=(maximized?QWindow::Maximized:QWindow::Windowed)||
               !sameRect(window.geometry(),expectedQt)||!sameRect(nativeGeometry,expectedNative)) {
                if(transitionWait.elapsed()<2000)return;
                QTextStream(stderr)<<"FAIL queued native window state at transition "<<transitions
                                   <<"; visibility="<<int(window.visibility())<<"; IsZoomed="<<IsZoomed(currentHandle)
                                   <<"; Qt="<<rectText(window.geometry())<<" expected="<<rectText(expectedQt)
                                   <<"; native="<<rectText(nativeGeometry)<<" expected="<<rectText(expectedNative)<<'\n';
                app.exit(5);
                return;
            }
        }
        if(transitions==20) {
            QTextStream(stdout)<<"PASS null-result input handling and 10 queued maximize/restore cycles with Qt/native work-area geometry\n";
            window.hide();
            app.quit();
        } else toggle();
    });
    timer.start(60);
    QTimer::singleShot(8000,&app,[&] {
        QTextStream(stderr)<<"FAIL native window frame test timeout\n";
        app.exit(6);
    });
    return app.exec();
}
