#include <windows.h>
#include <shellapi.h>
#include <string>
#include <fstream>
#include <sstream>
#include <regex>
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
    if(GetFileAttributesW(L"no-health.flag")!=INVALID_FILE_ATTRIBUTES){Sleep(200);return 4;}
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(int i=1;i+1<argc;++i)if(std::wstring(argv[i])==L"--health-file"){
        HANDLE f=CreateFileW(argv[i+1],GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f!=INVALID_HANDLE_VALUE){std::ifstream receipt("._teamhjd-receipt.json");std::stringstream bytes;bytes<<receipt.rdbuf();const auto raw=bytes.str();std::smatch match;std::string version;if(std::regex_search(raw,match,std::regex("\"version\"\\s*:\\s*\"([0-9A-Za-z._-]+)\"")))version=match[1];auto text=std::string("{\"pid\":")+std::to_string(GetCurrentProcessId())+",\"version\":\""+version+"\"}";DWORD written=0;WriteFile(f,text.data(),DWORD(text.size()),&written,nullptr);CloseHandle(f);}
    }
    LocalFree(argv);Sleep(2500);return 0;
}
