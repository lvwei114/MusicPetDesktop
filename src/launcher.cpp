#include "launcher.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <wrl/client.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <set>
#pragma comment(lib,"advapi32.lib")
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace {
std::wstring Lower(std::wstring s){for(auto& c:s)c=(wchar_t)towlower(c);return s;}
bool MusicName(const std::wstring& name){
    const auto s=Lower(name);
    const wchar_t* names[]={L"cloudmusic",L"qqmusic",L"kwmusic",L"kugou",L"musicfree",L"spotify",L"applemusic",L"网易云音乐",L"qq音乐",L"qq 音乐",L"酷我音乐",L"酷狗音乐",L"apple music"};
    for(auto name:names)if(s.find(name)!=std::wstring::npos)return true;return false;
}
std::wstring RegText(HKEY key,const wchar_t* value){
    DWORD bytes=0,type=0;
    if(RegQueryValueExW(key,value,nullptr,&type,nullptr,&bytes)!=ERROR_SUCCESS||bytes>65536||(type!=REG_SZ&&type!=REG_EXPAND_SZ))return {};
    std::wstring s(bytes/sizeof(wchar_t)+1,L'\0');
    if(RegQueryValueExW(key,value,nullptr,&type,(BYTE*)s.data(),&bytes)!=ERROR_SUCCESS)return {};
    s.resize(wcslen(s.c_str()));
    if(type==REG_EXPAND_SZ){wchar_t expanded[32768]{};auto count=ExpandEnvironmentStringsW(s.c_str(),expanded,32768);if(count&&count<=32768)s=expanded;}
    return s;
}
std::wstring LinkTarget(const fs::path& path){
    ComPtr<IShellLinkW> link;ComPtr<IPersistFile> file;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))||FAILED(link.As(&file))||FAILED(file->Load(path.c_str(),STGM_READ)))return {};
    wchar_t target[32768]{};WIN32_FIND_DATAW data{};
    // Do not Resolve: discovery must not launch installers, show UI or access networks.
    if(FAILED(link->GetPath(target,32768,&data,SLGP_RAWPATH)))return {};
    return target;
}
}
bool IsLaunchTarget(const std::wstring& path){
    std::error_code ec;const fs::path p(path);const auto ext=Lower(p.extension().wstring());
    return (ext==L".exe"||ext==L".lnk")&&fs::is_regular_file(p,ec);
}
std::vector<LaunchTarget> FindMusicApps(){
    const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    std::vector<LaunchTarget> found;std::set<std::wstring> seen;
    const ULONGLONG deadline=GetTickCount64()+3500;size_t visited=0;
    auto add=[&](const fs::path& path){
        if(!IsLaunchTarget(path.wstring()))return;
        std::wstring target=path.wstring();if(Lower(path.extension().wstring())==L".lnk")target=LinkTarget(path);
        if(target.empty()||Lower(fs::path(target).extension().wstring())!=L".exe")return;
        const auto stem=Lower(fs::path(target).stem().wstring());
        const wchar_t* clients[]={L"cloudmusic",L"qqmusic",L"kwmusic",L"kugou",L"musicfree",L"spotify",L"applemusic"};
        bool client=false;for(auto name:clients)if(stem==name)client=true;
        if(!client)return; // Exclude similarly named updaters, services and uninstallers.
        // Deduplicate executable paths while preserving the shortcut's launch arguments.
        if(seen.insert(Lower(fs::path(target).lexically_normal().wstring())).second)
            found.push_back({path.stem().wstring(),path.wstring()});
    };
    auto scan=[&](const fs::path& root,int depth,bool shortcuts){
        std::error_code ec;
        fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
        while(!ec&&it!=end&&GetTickCount64()<deadline&&visited++<6000){
            const auto entry=*it;const auto ext=Lower(entry.path().extension().wstring());
            if(it.depth()>=depth||entry.is_symlink(ec))it.disable_recursion_pending();
            if((shortcuts?ext==L".lnk":ext==L".exe")&&MusicName(entry.path().stem().wstring()))add(entry.path());
            it.increment(ec);
        }
    };
    // Running clients, including portable installations.
    HANDLE processes=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(processes!=INVALID_HANDLE_VALUE){PROCESSENTRY32W entry{sizeof(entry)};
        if(Process32FirstW(processes,&entry))do{
            if(!MusicName(entry.szExeFile))continue;
            HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
            if(process){wchar_t path[32768]{};DWORD size=32768;if(QueryFullProcessImageNameW(process,0,path,&size))add(path);CloseHandle(process);}
        }while(Process32NextW(processes,&entry));CloseHandle(processes);
    }
    const wchar_t* executables[]={L"cloudmusic.exe",L"QQMusic.exe",L"KwMusic.exe",L"KuGou.exe",L"MusicFree.exe",L"Spotify.exe",L"AppleMusic.exe"};
    for(auto hive:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE})for(REGSAM view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY}){
        for(auto exe:executables){HKEY key{};auto name=std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")+exe;
            if(RegOpenKeyExW(hive,name.c_str(),0,KEY_READ|view,&key)==ERROR_SUCCESS){auto path=RegText(key,nullptr);if(!path.empty()){if(path.front()==L'"'&&path.back()==L'"')path=path.substr(1,path.size()-2);add(path);}RegCloseKey(key);}}
        HKEY uninstall{};
        if(RegOpenKeyExW(hive,L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",0,KEY_READ|view,&uninstall)==ERROR_SUCCESS){
            wchar_t name[256];DWORD length;
            for(DWORD i=0;GetTickCount64()<deadline;++i){length=256;auto status=RegEnumKeyExW(uninstall,i,name,&length,nullptr,nullptr,nullptr,nullptr);if(status==ERROR_NO_MORE_ITEMS)break;if(status!=ERROR_SUCCESS)continue;
                HKEY key{};if(RegOpenKeyExW(uninstall,name,0,KEY_READ|view,&key)!=ERROR_SUCCESS)continue;
                if(MusicName(RegText(key,L"DisplayName"))){auto location=RegText(key,L"InstallLocation");if(!location.empty())scan(location,2,false);}
                RegCloseKey(key);
            }RegCloseKey(uninstall);
        }
    }
    for(const auto* id:{&FOLDERID_Programs,&FOLDERID_CommonPrograms}){PWSTR path{};
        if(SUCCEEDED(SHGetKnownFolderPath(*id,0,nullptr,&path))){scan(path,4,true);CoTaskMemFree(path);}}
    for(const wchar_t* env:{L"ProgramFiles",L"ProgramFiles(x86)",L"LOCALAPPDATA",L"APPDATA"}){
        wchar_t base[32768]{};const auto size=GetEnvironmentVariableW(env,base,32768);if(!size||size>=32768)continue;
        for(const wchar_t* folder:{L"NetEase\\CloudMusic",L"CloudMusic",L"Tencent\\QQMusic",L"QQMusic",L"KuGou",L"KuGou\\KGMusic",L"kuwo\\kuwomusic",L"MusicFree",L"Programs\\MusicFree",L"Spotify"})
            if(GetTickCount64()<deadline)scan(fs::path(base)/folder,2,false);
    }
    if(SUCCEEDED(com))CoUninitialize();return found;
}
bool OpenLaunchTarget(const std::wstring& path,void* owner){
    if(!IsLaunchTarget(path))return false;
    SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_FLAG_NO_UI;info.hwnd=(HWND)owner;
    info.lpVerb=L"open";info.lpFile=path.c_str();
    const auto directory=fs::path(path).parent_path().wstring();info.lpDirectory=directory.c_str();info.nShow=SW_SHOWNORMAL;
    return ShellExecuteExW(&info)!=FALSE;
}
