#include "core/Core.h"
#include <QCoreApplication>
#include <QTextStream>
#include <windows.h>
#include <cstring>

template<class T> T at(const QByteArray &bytes,qsizetype offset){
    if(offset<0||offset>bytes.size()||qsizetype(sizeof(T))>bytes.size()-offset)hjd::fail("truncated PE hardening metadata");
    T value{};std::memcpy(&value,bytes.constData()+offset,sizeof(T));return value;
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try{
        for(const auto *name:{"TeamHJDClient.exe","TeamHJDLauncher.exe"}){
            const auto bytes=hjd::readFile(QCoreApplication::applicationDirPath()+"/"+name);
            const auto dos=at<IMAGE_DOS_HEADER>(bytes,0);
            if(dos.e_magic!=IMAGE_DOS_SIGNATURE)hjd::fail("invalid DOS image");
            const auto nt=at<IMAGE_NT_HEADERS64>(bytes,dos.e_lfanew);
            if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.FileHeader.NumberOfSections>96)hjd::fail("invalid x64 PE image");
            const WORD expected=IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE|IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA|IMAGE_DLLCHARACTERISTICS_NX_COMPAT|IMAGE_DLLCHARACTERISTICS_GUARD_CF;
            if((nt.OptionalHeader.DllCharacteristics&expected)!=expected)hjd::fail("ASLR/high-entropy/DEP/CFG image flags missing");
            const auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
            const auto sectionOffset=dos.e_lfanew+sizeof(DWORD)+sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
            qsizetype configOffset=-1;
            for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){
                const auto section=at<IMAGE_SECTION_HEADER>(bytes,sectionOffset+i*sizeof(IMAGE_SECTION_HEADER));
                if(directory.VirtualAddress>=section.VirtualAddress&&quint64(directory.VirtualAddress)-section.VirtualAddress+directory.Size<=section.SizeOfRawData)configOffset=section.PointerToRawData+directory.VirtualAddress-section.VirtualAddress;
            }
            if(configOffset<0||directory.Size<offsetof(IMAGE_LOAD_CONFIG_DIRECTORY64,GuardFlags)+sizeof(DWORD))hjd::fail("load configuration missing");
            const auto config=at<IMAGE_LOAD_CONFIG_DIRECTORY64>(bytes,configOffset);
            if(config.DependentLoadFlags!=(LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32))hjd::fail("import DLL search must exclude current directory and PATH");
            if(!(config.GuardFlags&IMAGE_GUARD_CF_INSTRUMENTED)||!(config.GuardFlags&IMAGE_GUARD_CF_FUNCTION_TABLE_PRESENT)||!config.GuardCFFunctionCount||!config.SecurityCookie)hjd::fail("CFG instrumentation/table or stack cookie missing");
            QTextStream(stdout)<<"PASS "<<name<<" actual PE CFG/ASLR/high-entropy/DEP/stack-cookie and app-local/system-only imports\n";
        }
        return 0;
    }catch(const std::exception &error){QTextStream(stderr)<<error.what()<<'\n';return 1;}
}
