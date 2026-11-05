// TeamHJD native NSIS theme. Visual controls only; no install/auth/file actions.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <richedit.h>
#include "pluginapi.h"

namespace {
HMODULE module;
HWND parentWindow;
constexpr COLORREF background=RGB(20,24,27), surface=RGB(29,36,41);
constexpr COLORREF foreground=RGB(232,236,236), muted=RGB(149,163,171);
constexpr COLORREF gold=RGB(232,199,106), edge=RGB(50,61,68);
HBRUSH backgroundBrush, surfaceBrush;
constexpr UINT_PTR subclassId=0x484a44;

void drawButton(const DRAWITEMSTRUCT &item){
    if(item.CtlType!=ODT_BUTTON)return;
    const bool primary=GetDlgCtrlID(item.hwndItem)==IDOK;
    const bool disabled=(item.itemState&ODS_DISABLED)!=0;
    const bool pressed=(item.itemState&ODS_SELECTED)!=0;
    const auto fill=disabled?surface:(primary?(pressed?RGB(211,177,81):gold):surface);
    const auto text=disabled?muted:(primary?background:foreground);
    auto brush=CreateSolidBrush(fill);
    auto pen=CreatePen(PS_SOLID,1,primary&&!disabled?gold:edge);
    auto oldBrush=SelectObject(item.hDC,brush),oldPen=SelectObject(item.hDC,pen);
    RECT rect=item.rcItem;
    FillRect(item.hDC,&rect,backgroundBrush);
    RoundRect(item.hDC,rect.left,rect.top,rect.right,rect.bottom,10,10);
    SelectObject(item.hDC,oldBrush);SelectObject(item.hDC,oldPen);DeleteObject(brush);DeleteObject(pen);
    wchar_t title[256]{};GetWindowTextW(item.hwndItem,title,256);
    auto font=reinterpret_cast<HFONT>(SendMessageW(item.hwndItem,WM_GETFONT,0,0));
    auto oldFont=font?SelectObject(item.hDC,font):nullptr;
    SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,text);
    DrawTextW(item.hDC,title,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if((item.itemState&ODS_FOCUS)&&!(item.itemState&ODS_NOFOCUSRECT)){
        InflateRect(&rect,-4,-4);SetTextColor(item.hDC,primary?background:gold);DrawFocusRect(item.hDC,&rect);
    }
    if(oldFont)SelectObject(item.hDC,oldFont);
}

LRESULT CALLBACK themedProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(message==WM_DRAWITEM){
        auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if(item&&item->CtlType==ODT_BUTTON){drawButton(*item);return TRUE;}
        if(item&&item->CtlType==ODT_STATIC){auto brush=CreateSolidBrush(edge);FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);return TRUE;}
    }
    if(message==WM_CTLCOLORDLG)return reinterpret_cast<LRESULT>(backgroundBrush);
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX||message==WM_CTLCOLORBTN){
        auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,foreground);
        wchar_t name[64]{};GetClassNameW(reinterpret_cast<HWND>(lp),name,64);
        const bool input=lstrcmpiW(name,L"Edit")==0||lstrcmpiW(name,L"RichEdit20W")==0;
        SetBkColor(dc,input?surface:background);return reinterpret_cast<LRESULT>(input?surfaceBrush:backgroundBrush);
    }
    if(message==WM_NCDESTROY){RemoveWindowSubclass(window,themedProcedure,subclassId);if(window==parentWindow)parentWindow=nullptr;}
    return DefSubclassProc(window,message,wp,lp);
}

BOOL CALLBACK styleChild(HWND child,LPARAM){
    wchar_t name[64]{};GetClassNameW(child,name,64);
    if(lstrcmpiW(name,L"#32770")==0)SetWindowSubclass(child,themedProcedure,subclassId,0);
    if(lstrcmpiW(name,L"Static")==0&&(GetDlgCtrlID(child)==1035||GetDlgCtrlID(child)==1036)){
        auto style=GetWindowLongPtrW(child,GWL_STYLE);SetWindowLongPtrW(child,GWL_STYLE,(style&~SS_TYPEMASK)|SS_OWNERDRAW);
    }
    if(lstrcmpiW(name,L"Button")==0){
        const auto style=GetWindowLongPtrW(child,GWL_STYLE);
        const auto kind=style&BS_TYPEMASK;
        if(kind==BS_PUSHBUTTON||kind==BS_DEFPUSHBUTTON||kind==BS_OWNERDRAW){
            SetWindowLongPtrW(child,GWL_STYLE,(style&~BS_TYPEMASK)|BS_OWNERDRAW);
            SetWindowTheme(child,L"",L"");
        }else SetWindowTheme(child,L"",L"");
    }else if(lstrcmpiW(name,L"SysTreeView32")==0){
        TreeView_SetBkColor(child,surface);TreeView_SetTextColor(child,foreground);TreeView_SetLineColor(child,edge);
    }else if(lstrcmpiW(name,PROGRESS_CLASSW)==0){
        SetWindowTheme(child,L"",L"");SendMessageW(child,PBM_SETBKCOLOR,0,surface);SendMessageW(child,PBM_SETBARCOLOR,0,gold);
    }else if(lstrcmpiW(name,WC_LISTVIEWW)==0){
        ListView_SetBkColor(child,surface);ListView_SetTextBkColor(child,surface);ListView_SetTextColor(child,foreground);
    }else if(lstrcmpiW(name,L"RichEdit20W")==0){
        SendMessageW(child,EM_SETBKGNDCOLOR,0,surface);
        CHARFORMAT2W format{};format.cbSize=sizeof(format);format.dwMask=CFM_COLOR;format.crTextColor=foreground;
        SendMessageW(child,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&format));
    }
    InvalidateRect(child,nullptr,TRUE);
    return TRUE;
}

UINT_PTR callback(NSPIM event){
    if(event==NSPIM_GUIUNLOAD||event==NSPIM_UNLOAD){
        if(parentWindow&&IsWindow(parentWindow))RemoveWindowSubclass(parentWindow,themedProcedure,subclassId);
        parentWindow=nullptr;
        if(event==NSPIM_UNLOAD){
            if(backgroundBrush){DeleteObject(backgroundBrush);backgroundBrush=nullptr;}
            if(surfaceBrush){DeleteObject(surfaceBrush);surfaceBrush=nullptr;}
        }
    }
    return 0;
}
}

extern "C" __declspec(dllexport) void __cdecl Apply(HWND parent,int,wchar_t*,stack_t**,extra_parameters *extra){
    if(!parent||!IsWindow(parent)||!extra||!extra->RegisterPluginCallback)return;
    // Registering keeps our subclass code resident until NSIS destroys its GUI.
    if(extra->RegisterPluginCallback(module,callback)<0)return;
    if(!backgroundBrush)backgroundBrush=CreateSolidBrush(background);
    if(!surfaceBrush)surfaceBrush=CreateSolidBrush(surface);
    if(!backgroundBrush||!surfaceBrush)return;
    parentWindow=parent;SetWindowSubclass(parent,themedProcedure,subclassId,0);
    BOOL dark=TRUE;
    auto dwm=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(dwm){auto set=reinterpret_cast<HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD)>(GetProcAddress(dwm,"DwmSetWindowAttribute"));if(set)set(parent,20,&dark,sizeof(dark));FreeLibrary(dwm);}
    EnumChildWindows(parent,styleChild,0);InvalidateRect(parent,nullptr,TRUE);
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
