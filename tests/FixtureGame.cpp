#include <windows.h>
#include <shellapi.h>
#include <string>
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
    if(GetFileAttributesW(L"no-health.flag")!=INVALID_FILE_ATTRIBUTES){Sleep(200);return 4;}
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(int i=1;i+1<argc;++i)if(std::wstring(argv[i])==L"--health-file"){
        HANDLE f=CreateFileW(argv[i+1],GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f!=INVALID_HANDLE_VALUE){auto text=std::string("{\"pid\":")+std::to_string(GetCurrentProcessId())+"}";DWORD written=0;WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr);CloseHandle(f);}
    }
    LocalFree(argv);Sleep(2500);return 0;
}
