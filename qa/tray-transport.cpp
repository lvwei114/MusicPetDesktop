#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include <fstream>
#include <vector>
#include <sddl.h>
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"psapi.lib")
std::ofstream f;
void ObjectName(HANDLE object,const char* label){wchar_t name[256]{};DWORD bytes{};GetUserObjectInformationW(object,UOI_NAME,name,sizeof(name),&bytes);f<<label<<"=";for(auto c:name){if(!c)break;f<<(char)c;}f<<"\n";}
BOOL CALLBACK Enumerate(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(wcscmp(cls,L"Shell_TrayWnd")&&wcscmp(cls,L"Shell_SecondaryTrayWnd"))return TRUE;
 DWORD pid{};auto thread=GetWindowThreadProcessId(h,&pid);f<<"taskbar hwnd="<<h<<" pid="<<pid<<" thread="<<thread<<" visible="<<IsWindowVisible(h)<<" hung="<<IsHungAppWindow(h)<<"\n";
 ObjectName(GetThreadDesktop(thread),"taskbarDesktop");
 DWORD_PTR result{};SetLastError(0);auto ok=SendMessageTimeoutW(h,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result);f<<"WM_NULL ok="<<ok<<" error="<<GetLastError()<<" reply="<<result<<"\n";
 return TRUE;}
LRESULT CALLBACK Receiver(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_COPYDATA)return 123;return DefWindowProcW(h,m,w,l);}
void DumpToken(HANDLE process,const char* label){
 HANDLE token{};if(!OpenProcessToken(process,TOKEN_QUERY,&token)){f<<label<<" tokenError="<<GetLastError()<<"\n";return;}
 f<<label<<"\n";
 for(auto type:{TokenSessionId,TokenElevationType,TokenElevation,TokenVirtualizationEnabled,TokenUIAccess,TokenIsAppContainer,TokenMandatoryPolicy}){
 DWORD value{},size{};if(GetTokenInformation(token,type,&value,sizeof(value),&size))f<<" class="<<type<<" value="<<value<<"\n";
 }
 for(auto type:{TokenUser,TokenIntegrityLevel,TokenProcessTrustLevel}){
 DWORD size{};GetTokenInformation(token,type,nullptr,0,&size);std::vector<BYTE> data(size);
 if(GetTokenInformation(token,type,data.data(),size,&size)){
 PSID sid=*(PSID*)data.data();LPWSTR text{};if(sid&&ConvertSidToStringSidW(sid,&text)){f<<" sidClass="<<type<<" value=";for(auto p=text;*p;++p)f<<(char)*p;f<<"\n";LocalFree(text);}}
 }
 f<<" restricted="<<IsTokenRestricted(token)<<"\n";CloseHandle(token);
 for(int i=0;i<19;++i){BYTE policy[32]{};if(GetProcessMitigationPolicy(process,(PROCESS_MITIGATION_POLICY)i,policy,sizeof(DWORD)))f<<" mitigation="<<i<<" flags="<<*(DWORD*)policy<<"\n";}
 BOOL job{};IsProcessInJob(process,nullptr,&job);f<<" inJob="<<job<<"\n";
}
int wmain(int argc,wchar_t**){
 if(argc>1){WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=Receiver;wc.lpszClassName=L"MusicPet.CopyDataReceiver";RegisterClassW(&wc);
 auto h=CreateWindowW(wc.lpszClassName,L"Receiver",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==WM_CLOSE)break;TranslateMessage(&msg);DispatchMessageW(&msg);}DestroyWindow(h);return 0;}

 f.open("qa/tray-transport-report.txt");
 DumpToken(GetCurrentProcess(),"selfToken");
 DWORD explorerPid{};GetWindowThreadProcessId(FindWindowW(L"Shell_TrayWnd",nullptr),&explorerPid);
 HANDLE explorer=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,explorerPid);if(explorer){DumpToken(explorer,"explorerToken");CloseHandle(explorer);}

 ObjectName(GetProcessWindowStation(),"windowStation");ObjectName(GetThreadDesktop(GetCurrentThreadId()),"ownDesktop");
 f<<"NoTrayItemsDisplay="<<SHRestricted(REST_NOTRAYITEMSDISPLAY)<<"\n";
 EnumWindows(Enumerate,0);
 WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"MusicPet.TrayTransport";RegisterClassW(&wc);
 HWND own=CreateWindowW(wc.lpszClassName,L"Tray transport probe",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
 auto tray=FindWindowW(L"Shell_TrayWnd",nullptr);COPYDATASTRUCT data{};data.dwData=0x7ffffffe;
 DWORD_PTR result{};SetLastError(0);auto ok=SendMessageTimeoutW(tray,WM_COPYDATA,(WPARAM)own,(LPARAM)&data,SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result);
 f<<"unrecognized WM_COPYDATA ok="<<ok<<" error="<<GetLastError()<<" reply="<<result<<"\n";
 wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring command=L"\""+std::wstring(executable)+L"\" --receiver";
 STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
 if(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
 HWND receiver{};for(int i=0;i<100&&!receiver;++i){receiver=FindWindowW(L"MusicPet.CopyDataReceiver",nullptr);if(!receiver)Sleep(50);}
 SetLastError(0);result=0;auto childOk=SendMessageTimeoutW(receiver,WM_COPYDATA,(WPARAM)own,(LPARAM)&data,SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result);
 f<<"own child WM_COPYDATA ok="<<childOk<<" error="<<GetLastError()<<" reply="<<result<<"\n";
 PostMessageW(receiver,WM_CLOSE,0,0);WaitForSingleObject(pi.hProcess,3000);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }
 SetLastError(0);result=0;auto zeroOk=SendMessageTimeoutW(tray,WM_COPYDATA,0,(LPARAM)&data,SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result);
 f<<"zero sender WM_COPYDATA ok="<<zeroOk<<" error="<<GetLastError()<<" reply="<<result<<"\n";
 NOTIFYICONDATAW n{sizeof(n)};n.hWnd=own;n.uID=123;n.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;n.uCallbackMessage=WM_USER+100;n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wcscpy_s(n.szTip,L"Tray transport probe");
 SetLastError(0);auto added=Shell_NotifyIconW(NIM_ADD,&n);f<<"Shell_NotifyIconW="<<added<<" error="<<GetLastError()<<"\n";if(added)Shell_NotifyIconW(NIM_DELETE,&n);
 HMODULE modules[512];DWORD bytes{};EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes);
 for(unsigned i=0;i<bytes/sizeof(HMODULE)&&i<512;++i){wchar_t path[MAX_PATH]{};GetModuleFileNameW(modules[i],path,MAX_PATH);auto output=std::wstring(path);
 if(output.find(L"Windows\\System32")==std::wstring::npos){f<<"module=";for(auto c:output)f<<(c<128?(char)c:'?');f<<"\n";}}
 DestroyWindow(own);return 0;
}

