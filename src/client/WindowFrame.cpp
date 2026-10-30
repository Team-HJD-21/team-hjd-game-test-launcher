#include "WindowFrame.h"
#include <QGuiApplication>
#include <QQuickWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

WindowFrame::WindowFrame(QQuickWindow *window):m_window(window){
    HWND handle=reinterpret_cast<HWND>(window->winId());
    m_handle=reinterpret_cast<quintptr>(handle);
    qGuiApp->installNativeEventFilter(this);
    auto style=GetWindowLongPtrW(handle,GWL_STYLE);
    SetWindowLongPtrW(handle,GWL_STYLE,style|WS_THICKFRAME|WS_CAPTION|WS_MAXIMIZEBOX|WS_MINIMIZEBOX|WS_SYSMENU);
    BOOL dark=TRUE;DwmSetWindowAttribute(handle,20,&dark,sizeof(dark));
    DWORD corner=2;DwmSetWindowAttribute(handle,33,&corner,sizeof(corner));
    COLORREF border=RGB(48,55,61);DwmSetWindowAttribute(handle,34,&border,sizeof(border));
    SetWindowPos(handle,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
}
WindowFrame::~WindowFrame(){qGuiApp->removeNativeEventFilter(this);}
bool WindowFrame::nativeEventFilter(const QByteArray &type,void *message,qintptr *result){
    if(type!="windows_generic_MSG"||!message)return false;
    auto *msg=static_cast<MSG*>(message);auto handle=reinterpret_cast<HWND>(m_handle);if(!handle||msg->hwnd!=handle)return false;
    if(msg->message==WM_NCDESTROY){m_handle=0;return false;}
    if(!m_window)return false;
    // Qt delivers queued native input with a null result, and deliberately does
    // not repeat input in the window-proc global filter. Consume max-button
    // input here, but only write an LRESULT when the caller supplies one.
    if((msg->message==WM_NCLBUTTONDOWN||msg->message==WM_NCLBUTTONUP)&&msg->wParam==HTMAXBUTTON){
        if(msg->message==WM_NCLBUTTONUP&&!m_maximizeQueued){
            m_maximizeQueued=true;
            // Leave the native callback before changing Qt window state.
            QMetaObject::invokeMethod(&m_callbacks,[this]{
                m_maximizeQueued=false;
                if(!m_window||!m_handle)return;
                if(m_window->visibility()==QWindow::Maximized)m_window->showNormal();else m_window->showMaximized();
            },Qt::QueuedConnection);
        }
        if(result)*result=0;
        return true;
    }
    if(msg->message==WM_NCMOUSEMOVE){m_window->setProperty("nativeMaxHover",msg->wParam==HTMAXBUTTON);TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE|TME_NONCLIENT,handle,0};TrackMouseEvent(&track);}
    if(msg->message==WM_NCMOUSELEAVE)m_window->setProperty("nativeMaxHover",false);
    // Geometry queries need a real result, unlike the queued mouse input above.
    if(!result)return false;
    if(msg->message==WM_NCCALCSIZE&&msg->wParam){
        if(IsZoomed(handle)){auto *parameters=reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromWindow(handle,MONITOR_DEFAULTTONEAREST),&info);parameters->rgrc[0]=info.rcWork;}
        *result=0;return true;
    }
    if(msg->message==WM_NCHITTEST){
        POINT p{GET_X_LPARAM(msg->lParam),GET_Y_LPARAM(msg->lParam)};ScreenToClient(handle,&p);RECT r{};GetClientRect(handle,&r);
        double ratio=m_window->devicePixelRatio();int edge=int(6*ratio);bool left=p.x<edge,right=p.x>=r.right-edge,top=p.y<edge,bottom=p.y>=r.bottom-edge;
        if(!IsZoomed(handle)){
            if(top&&left)*result=HTTOPLEFT;else if(top&&right)*result=HTTOPRIGHT;else if(bottom&&left)*result=HTBOTTOMLEFT;else if(bottom&&right)*result=HTBOTTOMRIGHT;else if(left)*result=HTLEFT;else if(right)*result=HTRIGHT;else if(top)*result=HTTOP;else if(bottom)*result=HTBOTTOM;else *result=HTCLIENT;
            if(*result!=HTCLIENT)return true;
        }
        if(p.y>=0&&p.y<int(40*ratio)&&p.x>=r.right-int(92*ratio)&&p.x<r.right-int(46*ratio)){*result=HTMAXBUTTON;return true;}
    }
    return false;
}
#else
WindowFrame::WindowFrame(QQuickWindow *window):m_window(window){}
WindowFrame::~WindowFrame()=default;
bool WindowFrame::nativeEventFilter(const QByteArray &,void *,qintptr *){return false;}
#endif
