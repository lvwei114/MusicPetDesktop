#include <windows.h>
#include <shellapi.h>
#include <fstream>
int main(){
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 std::ofstream f("qa/tray-probe-report.txt");
 WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"MusicPet.TrayProbe";RegisterClassW(&wc);
 HWND h=CreateWindowW(wc.lpszClassName,L"Probe",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
 NOTIFYICONDATAW n{sizeof(n)};n.hWnd=h;n.uID=71;n.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);n.uCallbackMessage=WM_APP+11;wcscpy_s(n.szTip,L"MusicPet tray probe");
 ShowWindow(h,SW_SHOWNOACTIVATE);
 SetLastError(0);auto ok=Shell_NotifyIconW(NIM_ADD,&n);auto err=GetLastError();f<<"regular window ADD="<<ok<<" error="<<err<<" sizeof="<<sizeof(n)<<" hwnd="<<h<<"\n";
 if(ok){Shell_NotifyIconW(NIM_DELETE,&n);}
 for(DWORD size:{(DWORD)NOTIFYICONDATAW_V1_SIZE,(DWORD)NOTIFYICONDATAW_V2_SIZE,(DWORD)NOTIFYICONDATAW_V3_SIZE}){
 n.cbSize=size;SetLastError(0);auto value=Shell_NotifyIconW(NIM_ADD,&n);f<<"compat size="<<size<<" ADD="<<value<<" error="<<GetLastError()<<"\n";if(value)Shell_NotifyIconW(NIM_DELETE,&n);
 }
 DestroyWindow(h);CoUninitialize();
 return ok?0:1;
}
