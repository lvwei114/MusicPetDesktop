// MusicPet — native windows, static/animated artwork, GSMTC and floating notes.
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <future>
#include <chrono>
#include <string>
#include <vector>
#include "app.h"
#include "effects.h"
#include "gfx.h"
#include "media.h"
#include "settings.h"
#include "skin.h"
#include "launcher.h"
#include "motion.h"
#include "apng.h"
#include <filesystem>
#pragma comment(lib,"comdlg32.lib")
using namespace Gdiplus;
namespace {
constexpr int PanelW=344, PanelH=478;
constexpr UINT TrayMessage=WM_APP_TRAY, RecoverMessage=WM_APP+5;
// Stable identity lets Explorer retain the user's notification-area preference.
constexpr GUID TrayGuid={0xd43878c2,0x6d9a,0x471f,{0xa3,0xa1,0x79,0x67,0xd0,0x6e,0x28,0xb4}};
struct Shortcut {UINT modifiers,key;const wchar_t* label;};
constexpr Shortcut shortcuts[]={
    {MOD_CONTROL|MOD_ALT,'P',L"Ctrl + Alt + P"},
    {MOD_CONTROL|MOD_ALT|MOD_SHIFT,'P',L"Ctrl + Alt + Shift + P"},
    {MOD_CONTROL|MOD_ALT,VK_F10,L"Ctrl + Alt + F10"},
    {MOD_CONTROL|MOD_SHIFT,VK_F12,L"Ctrl + Shift + F12"}};
constexpr int CmdHotkey=230,CmdPalette=240,CmdLaunch=250,CmdLaunchChoose=251,CmdLaunchFind=252,CmdLaunchClear=253,CmdLaunchFound=3000,CmdSkinName=254,CmdTrayRetry=255;
constexpr int CmdChoose=201,CmdPreview=202,CmdThrough=203,CmdEffects=204,CmdLock=205,CmdReset=206,CmdExit=207,CmdGame=208;
constexpr int CmdAuto=220,CmdSize=300,CmdDensity=900,CmdSkin=1000,CmdSession=2000;
constexpr int CmdAutoStart=256;
constexpr int CmdMotionChoose=257,CmdMotionClear=258,CmdSizeSlider=259,CmdMode=270;
constexpr int CmdPairSave=260;
constexpr int CmdMotionSpeed=280,CmdMotionBounce=287;
constexpr int motionSpeeds[]={25,50,75,100,125,150,200};
const wchar_t* motionSpeedLabels[]={L"0.25 倍",L"0.5 倍",L"0.75 倍",L"1 倍（原速）",L"1.25 倍",L"1.5 倍",L"2 倍"};
struct State {
    HINSTANCE instance{}; HWND pet{},fx{},panel{}; HICON icon{};
    Settings settings; PetImage image,motionImage; MotionPlayer motion; NoteLayer notes; MediaWatcher media; MediaSnapshot snapshot;
    bool motionVisible{},motionFailed{};
    std::vector<SkinItem> skins; std::vector<SessionEntry> menuSessions;
    bool dragging{},animating{},busy{},hotkey{},preview{},shuttingDown{},trayAdded{};
    POINT dragOffset{}; int hover{},pressed{},panelDpi{96}; UINT dpi{96};
    ULONGLONG lastFrame{},previewUntil{}; std::wstring notice; ULONGLONG noticeUntil{};
    bool saveOnExit{true};
    bool outsideButtonDown{};
    ULONGLONG panelOpened{};
    bool dpiReloadPending{},trayV4{},ready{},recoverPending{};
    bool trayUseGuid{true};
    int activeShortcut{-1};
    std::future<std::vector<LaunchTarget>> appSearch;
    std::vector<LaunchTarget> launchTargets;
} a;
const Color ink(255,34,52,60), muted(255,112,128,132), teal(255,66,139,133), paper(255,248,250,248), line(255,225,233,229);
void Command(int id);
void ShowPanel(POINT point);
void UpdateAnimation();
void AddTray();
void SyncMotion();
void RenameSkin();
int Px(int value){return MulDiv(value,(int)a.dpi,96);}
void Trace(const wchar_t* stage){if(!a.saveOnExit)WritePrivateProfileStringW(L"startup",L"stage",stage,(ExeDir()+L"\\qa\\startup.ini").c_str());}
void RoundPath(GraphicsPath& p, RectF r,float radius) {
    const float d=radius*2;
    p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
    p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);p.CloseFigure();
}
void FillRound(Graphics& g,RectF r,float radius,Color c) {GraphicsPath p;RoundPath(p,r,radius);SolidBrush b(c);g.FillPath(&b,&p);}
void Text(Graphics& g,const std::wstring& text,RectF rect,float size,Color color,bool bold=false,StringAlignment align=StringAlignmentNear) {
    Font font(L"Microsoft YaHei UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);
    StringFormat format;format.SetAlignment(align);format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);format.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush brush(color);g.DrawString(text.c_str(),-1,&font,rect,&format,&brush);
}
void DrawIcon(Graphics& g,int type,float cx,float cy,Color color) {
    Pen pen(color,1.8f);pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);pen.SetLineJoin(LineJoinRound);
    SolidBrush brush(color);
    if(type==1 || type==3) {
        const float sign=type==1?-1.f:1.f;
        PointF points[]={{cx+sign*6,cy},{cx-sign*4,cy-6},{cx-sign*4,cy+6}};
        g.FillPolygon(&brush,points,3);g.DrawLine(&pen,cx+sign*8,cy-6,cx+sign*8,cy+6);
    } else if(type==2) {
        if(a.snapshot.status==PlayStatus::Playing){FillRound(g,{cx-6,cy-7,4,14},1,color);FillRound(g,{cx+2,cy-7,4,14},1,color);}
        else {PointF points[]={{cx-4,cy-8},{cx+8,cy},{cx-4,cy+8}};g.FillPolygon(&brush,points,3);}
    } else if(type==4) {
        g.DrawLine(&pen,cx-4,cy-4,cx+4,cy+4);g.DrawLine(&pen,cx+4,cy-4,cx-4,cy+4);
    } else if(type==5) {
        g.DrawRectangle(&pen,cx-7,cy-6,14.f,12.f);g.DrawEllipse(&pen,cx+1,cy-3,3.f,3.f);
        PointF points[]={{cx-6,cy+4},{cx-1,cy-1},{cx+2,cy+2},{cx+5,cy}};g.DrawLines(&pen,points,4);
    } else if(type==6) DrawNoteGlyph(g,cx,cy,20,0,color);
    else if(type==7) {
        g.DrawEllipse(&pen,cx-7,cy-7,14.f,14.f);g.DrawLine(&pen,cx-9,cy+9,cx+9,cy-9);
    } else if(type==8) {
        g.DrawEllipse(&pen,cx-1,cy-1,2.f,2.f);g.DrawEllipse(&pen,cx-7,cy-1,2.f,2.f);g.DrawEllipse(&pen,cx+5,cy-1,2.f,2.f);
    }
}
std::wstring Time(double seconds){int value=(int)std::max(0.,seconds);wchar_t text[40];swprintf_s(text,L"%d:%02d",value/60,value%60);return text;}
std::wstring StatusText(){
    if(a.snapshot.reconnecting) return L"正在切换曲目…";
    if(a.preview) return L"音符预览中";
    if(!a.snapshot.valid) return a.media.SelectedId().empty()?L"等待音乐播放":L"等待选定的播放器";
    switch(a.snapshot.status){case PlayStatus::Playing:return L"正在播放";case PlayStatus::Paused:return L"已暂停";case PlayStatus::Stopped:return L"已停止";default:return L"等待播放状态";}
}
void Toggle(Graphics& g,float x,float y,bool on) {
    FillRound(g,{x,y,32,19},9.5f,on?teal:Color(255,210,219,216));SolidBrush white(Color(255,255,255,255));g.FillEllipse(&white,x+(on?15.f:3.f),y+3,13.f,13.f);
}
struct Hit {int id;RectF rect;};
const Hit hits[]={{10,{300,15,28,28}},{11,{20,57,304,29}},{1,{67,159,46,46}},{2,{144,155,56,56}},{3,{231,159,46,46}},
                  {12,{16,255,312,51}},{13,{16,308,312,50}},{14,{16,360,312,50}},{15,{18,430,160,30}},{16,{259,430,67,30}}};
bool Enabled(int id){if(id==1)return a.snapshot.canPrev;if(id==2)return a.snapshot.canToggle;if(id==3)return a.snapshot.canNext;return true;}
void PaintPanel(Graphics& g) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    SolidBrush panelBackground(paper);g.FillRectangle(&panelBackground,0,0,PanelW,PanelH);
    // Quiet mint header, warm accent, generous space around media controls.
    FillRound(g,{20,20,23,23},8,teal);DrawNoteGlyph(g,31.5f,31.5f,16,0,Color(255,255,255,255));
    Text(g,L"一起听歌",{51,17,180,29},17,ink,true);
    Text(g,L"MUSIC PET",{211,21,80,22},9,muted);
    if(a.hover==10)FillRound(g,{300,15,28,28},8,Color(255,230,237,233));DrawIcon(g,4,314,29,muted);
    FillRound(g,{20,57,304,29},9,a.hover==11?Color(255,224,237,231):Color(255,233,241,236));
    SolidBrush dot(a.snapshot.status==PlayStatus::Playing?teal:Color(255,174,186,180));g.FillEllipse(&dot,30.f,68.f,6.f,6.f);
    Text(g,a.snapshot.valid?a.snapshot.appName:L"选择播放器",{43,58,238,27},11,teal);
    Text(g,L"⌄",{290,58,22,27},14,teal,false,StringAlignmentCenter);
    const auto title=a.snapshot.valid&&!a.snapshot.title.empty()?a.snapshot.title:L"让音乐陪你待一会儿";
    Text(g,title,{24,99,296,31},19,ink,true);
    const auto subtitle=a.snapshot.reconnecting?StatusText():(a.snapshot.valid?(a.snapshot.artist.empty()?StatusText():a.snapshot.artist):L"播放音乐后，音符会轻轻飘起来");
    Text(g,subtitle,{24,131,296,21},11,muted);
    for(int id=1;id<=3;++id) {
        const RectF r=id==1?RectF(67,159,46,46):id==2?RectF(144,155,56,56):RectF(231,159,46,46);
        const bool enabled=Enabled(id);
        const Color bg=id==2?(enabled?teal:Color(255,222,230,225)):(a.hover==id&&enabled?Color(255,229,238,232):Color(255,239,244,240));
        SolidBrush b(bg);g.FillEllipse(&b,r);
        DrawIcon(g,id,r.X+r.Width/2,r.Y+r.Height/2,id==2?Color(255,255,255,255):enabled?ink:Color(255,180,190,184));
    }
    FillRound(g,{25,226,294,3},1.5f,Color(255,223,232,225));
    if(a.snapshot.durationSec>0){
        const float width=(float)std::clamp(a.snapshot.positionSec/a.snapshot.durationSec,0.,1.)*294;
        if(width>3)FillRound(g,{25,226,width,3},1.5f,teal);
        Text(g,Time(a.snapshot.positionSec),{25,233,80,15},9,muted);
        Text(g,Time(a.snapshot.durationSec),{239,233,80,15},9,muted,false,StringAlignmentFar);
    } else Text(g,a.snapshot.reconnecting?L"正在等待新曲目进度…":(a.snapshot.valid?L"播放器未提供进度":L"可先用「音符预览」看看效果"),{25,233,294,15},9,muted);
    for(int id=12;id<=14;++id) {
        const float y=255+(id-12)*52.5f;
        if(a.hover==id)FillRound(g,{16,y,312,51},10,Color(255,235,241,235));
        FillRound(g,{25,y+9,31,31},9,Color(255,233,240,233));
        DrawIcon(g,id==12?5:id==13?6:7,40.5f,y+24.5f,teal);
        const wchar_t* labels[]={L"桌宠形象",L"音符效果",L"点击穿透"};
        const wchar_t* desc[]={L"更换图片 · 调整大小",L"只随播放状态出现",a.trayAdded?L"开启后从托盘或再次启动恢复":L"托盘暂不可用 · 再次启动可恢复"};
        Text(g,labels[id-12],{68,y+7,200,21},12,ink);
        Text(g,desc[id-12],{68,y+28,210,16},9,muted);
        if(id==12)Text(g,L"›",{293,y+10,23,29},22,muted,false,StringAlignmentCenter);
        else Toggle(g,286,y+16,id==13?a.settings.effectsOn!=0:a.settings.clickThrough);
    }
    Pen separator(line,1);g.DrawLine(&separator,24.f,422.f,320.f,422.f);
    if(a.hover==15)FillRound(g,{18,430,160,30},8,Color(255,232,240,233));
    DrawIcon(g,8,34,445,muted);Text(g,L"更多设置",{49,430,120,30},11,muted);
    if(a.hover==16)FillRound(g,{259,430,67,30},8,Color(255,245,229,225));
    Text(g,L"退出",{270,430,42,30},11,a.hover==16?Color(255,173,91,80):muted,false,StringAlignmentCenter);
    if(!a.notice.empty() && GetTickCount64()<a.noticeUntil) {
        FillRound(g,{18,383,308,34},9,Color(255,43,66,66));Text(g,a.notice,{29,383,286,34},10,Color(255,255,255,255));
    }
}
int HitAt(LPARAM value){const float scale=a.panelDpi/96.f;PointF p(GET_X_LPARAM(value)/scale,GET_Y_LPARAM(value)/scale);for(const auto& h:hits)if(h.rect.Contains(p))return h.id;return 0;}
void Notify(const std::wstring& text){a.notice=text;a.noticeUntil=GetTickCount64()+3500;if(IsWindowVisible(a.panel))InvalidateRect(a.panel,nullptr,FALSE);}
void Save(){if(a.saveOnExit)SaveSettings(a.settings);}
NOTIFYICONDATAW TrayData(){
    NOTIFYICONDATAW n{sizeof(n)};n.hWnd=a.pet;n.uID=1;if(a.trayUseGuid){n.uFlags=NIF_GUID;n.guidItem=TrayGuid;}return n;
}
std::wstring ShortcutText(){return a.hotkey?shortcuts[a.activeShortcut].label:L"未启用（可再次启动程序恢复）";}
void ConfigureShortcut(){
    UnregisterHotKey(a.pet,1);a.hotkey=false;a.activeShortcut=-1;
    if(a.settings.hotkeyPreset<4){
        for(int offset=0;offset<4;++offset){int i=(a.settings.hotkeyPreset+offset)%4;
            if(RegisterHotKey(a.pet,1,shortcuts[i].modifiers|MOD_NOREPEAT,shortcuts[i].key)){
                a.hotkey=true;a.activeShortcut=i;break;
            }
        }
    }
    // Also exposed as a diagnostic; 0 means unavailable/disabled.
    SetPropW(a.pet,L"MusicPet.ActiveShortcut",(HANDLE)(INT_PTR)(a.activeShortcut+1));
    if(a.settings.hotkeyPreset<4&&a.activeShortcut!=a.settings.hotkeyPreset)
        Notify(a.hotkey?L"首选快捷键冲突，已改用 "+ShortcutText():L"所有备用快捷键均被占用，可从托盘或再次启动恢复。");
}
void UpdateTray(){
    if(!a.trayAdded)return;auto n=TrayData();n.uFlags|=NIF_TIP|NIF_SHOWTIP;
    auto tip=std::wstring(L"音乐桌宠 · ")+StatusText()+L"\n穿透快捷键："+ShortcutText();
    if(!a.snapshot.title.empty())tip+=L"\n"+a.snapshot.title;
    wcsncpy_s(n.szTip,tip.c_str(),_TRUNCATE);
    if(!Shell_NotifyIconW(NIM_MODIFY,&n)){a.trayAdded=false;AddTray();}
}
void ApplyThrough(){
    auto style=GetWindowLongPtrW(a.pet,GWL_EXSTYLE);
    if(a.settings.clickThrough)style|=WS_EX_TRANSPARENT;else style&=~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(a.pet,GWL_EXSTYLE,style);
    SetWindowPos(a.pet,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
}
void Place(int x,int y){
    a.settings.posX=x;a.settings.posY=y;
    SetWindowPos(a.pet,HWND_TOPMOST,x,y,a.image.width,a.image.height,SWP_NOACTIVATE);
    const auto layout=ComputeNoteLayout(a.image.width,a.image.height,a.dpi);
    const int mx=layout.margin,top=layout.top,bottom=layout.bottom;
    const int w=a.image.width+2*mx,h=a.image.height+top+bottom;
    a.notes.Resize(w,h);a.notes.SetOrigin(mx+a.image.width*.5f,top+a.image.height*.39f,a.image.width*.54f);
    a.notes.Configure(a.settings.effectsOn?(a.settings.gameMode?1:a.settings.density):0,ComputeNoteLayout(a.image.width,a.image.height,a.dpi).scale);
    SetWindowPos(a.fx,HWND_TOPMOST,x-mx,y-top,w,h,SWP_NOACTIVATE);
    SetWindowPos(a.pet,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}
void ClampPosition(int& x,int& y,int w,int h){
    RECT requested{x,y,x+w,y+h};MONITORINFO m{sizeof(m)};GetMonitorInfoW(MonitorFromRect(&requested,MONITOR_DEFAULTTONEAREST),&m);
    x=std::clamp(x,(int)m.rcWork.left,std::max((int)m.rcWork.left,(int)m.rcWork.right-w));
    y=std::clamp(y,(int)m.rcWork.top,std::max((int)m.rcWork.top,(int)m.rcWork.bottom-h));
}
bool ApplyImage(const std::wstring& path,bool center=false){
    Trace(L"image-decode");
    PetImage next;const int target=Px(a.settings.petHeight);
    // Paired artwork keeps the full static canvas as the reference viewport.
    // Cropping only the idle image would enlarge it relative to every animation.
    if(!LoadPngImage(path,target*2,target,next,a.settings.motionPath.empty())){Notify(L"图片加载失败，请选择 PNG、JPG 或 BMP。");return false;}
    Trace(L"image-decoded");int x=a.settings.posX,y=a.settings.posY;
    if(a.image.dc && !center){x+=(a.image.width-next.width)/2;y+=a.image.height-next.height;}
    a.image.Reset();a.image=next;
    a.notes.SetPalette(a.settings.notePalette,a.image.bits,a.image.width,a.image.height);
    if(x==INT_MIN || y==INT_MIN || center){MONITORINFO m{sizeof(m)};GetMonitorInfoW(MonitorFromWindow(a.pet,MONITOR_DEFAULTTOPRIMARY),&m);x=m.rcWork.right-next.width-Px(55);y=m.rcWork.bottom-next.height-Px(20);}
    ClampPosition(x,y,next.width,next.height);Trace(L"image-place");Place(x,y);Trace(L"image-placed");
    POINT destination{x,y},source{};SIZE size{next.width,next.height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    if(!UpdateLayeredWindow(a.pet,nullptr,&destination,&size,next.dc,&source,0,&blend,ULW_ALPHA))return false;
    ShowWindow(a.pet,SW_SHOWNOACTIVATE);a.settings.skinPath=path;
    a.motionImage.Reset();a.motionVisible=false;a.motionFailed=false;
    a.motion.Configure(a.settings.motionPath,a.image.width,a.image.height,a.settings.motionSpeed,a.settings.motionPingPong);SyncMotion();
    UpdateAnimation();Trace(L"image-visible");return true;
}
void DrawPet(const PetImage& image){
    POINT source{};SIZE size{image.width,image.height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(a.pet,nullptr,nullptr,&size,image.dc,&source,0,&blend,ULW_ALPHA);
}
void SyncMotion(){
    const bool play=!a.settings.motionPath.empty()&&a.snapshot.valid&&a.snapshot.status==PlayStatus::Playing;
    a.motion.Play(play);
    if(a.motion.Active()&&!a.motionFailed)SetTimer(a.pet,7,a.settings.gameMode?50:20,nullptr);
    else {KillTimer(a.pet,7);if(a.motionVisible){DrawPet(a.image);a.motionVisible=false;}}
}
bool SelectSkin(const SkinItem& item){
    const auto previousMotion=a.settings.motionPath;
    const int previousSpeed=a.settings.motionSpeed;const bool previousBounce=a.settings.motionPingPong;
    a.settings.motionPath=item.motionPath;
    a.settings.motionSpeed=item.motionSpeed;a.settings.motionPingPong=item.motionPingPong;
    if(!ApplyImage(item.path)){a.settings.motionPath=previousMotion;a.settings.motionSpeed=previousSpeed;a.settings.motionPingPong=previousBounce;return false;}
    a.settings.presetPath=item.presetFile.empty()?L"":std::filesystem::path(item.presetFile).filename().wstring();
    return true;
}
std::wstring CurrentSkinName(){return SkinDisplayName(a.settings.presetPath.empty()?a.settings.skinPath:(std::filesystem::path(SkinFolderBesideExe())/a.settings.presetPath).wstring());}
bool SaveCurrentPair(const std::wstring& motion){
    SkinItem pair;if(!SavePairedPreset(a.settings.skinPath,motion,CurrentSkinName(),pair,a.settings.motionSpeed,a.settings.motionPingPong))return false;
    if(!SelectSkin(pair))return false;
    a.skins=ScanSkins();Save();return true;
}
void ChooseMotion(){
    a.busy=true;wchar_t path[32768]{};OPENFILENAMEW file{sizeof(file)};
    file.hwndOwner=a.panel;file.lpstrFilter=L"动态皮肤（GIF / APNG / MP4）\0*.gif;*.apng;*.png;*.mp4\0\0";
    file.lpstrFile=path;file.nMaxFile=32768;file.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    file.lpstrTitle=L"选择播放时的动态皮肤（静音循环）";
    if(GetOpenFileNameW(&file)){
        try{
            namespace fs=std::filesystem;const fs::path input(path);
            auto ext=input.extension().wstring();for(auto& c:ext)c=(wchar_t)towlower(c);
            if(ext!=L".gif"&&ext!=L".apng"&&ext!=L".png"&&ext!=L".mp4")throw std::runtime_error("Unsupported file");
            if(ext==L".apng"||ext==L".png")ReadApng(path);
            if(!SaveCurrentPair(path))throw std::runtime_error("Cannot save pair");
            RenameSkin();
            Notify(L"已保存静态＋动态组合预设；暂停后播完当前一轮再切回静态图。");
        }catch(...){Notify(L"组合保存失败。请选择有效 GIF、APNG 或 MP4（不超过 256 MB）；普通 PNG 不属于动态素材。");}
    }a.busy=false;
}
INT_PTR CALLBACK SizeProc(HWND dialog,UINT msg,WPARAM w,LPARAM){
    auto apply=[&]{const int value=(int)SendDlgItemMessageW(dialog,1101,TBM_GETPOS,0,0);
        const auto text=std::to_wstring(value)+L" 逻辑像素";SetDlgItemTextW(dialog,1102,text.c_str());
        if(a.settings.petHeight!=value){const int before=a.settings.petHeight;a.settings.petHeight=value;
            if(!ApplyImage(a.settings.skinPath))a.settings.petHeight=before;}};
    if(msg==WM_INITDIALOG){SetWindowLongPtrW(dialog,DWLP_USER,a.settings.petHeight);
        SendDlgItemMessageW(dialog,1101,TBM_SETRANGE,TRUE,MAKELPARAM(100,600));
        SendDlgItemMessageW(dialog,1101,TBM_SETPOS,TRUE,a.settings.petHeight);
        SendDlgItemMessageW(dialog,1101,TBM_SETPAGESIZE,0,20);apply();SetTimer(dialog,1,100,nullptr);return TRUE;}
    if(msg==WM_TIMER&&w==1){apply();return TRUE;}
    if(msg==WM_HSCROLL){const auto text=std::to_wstring(SendDlgItemMessageW(dialog,1101,TBM_GETPOS,0,0))+L" 逻辑像素";SetDlgItemTextW(dialog,1102,text.c_str());return TRUE;}
    if(msg==WM_COMMAND){
        if(LOWORD(w)==1103){SendDlgItemMessageW(dialog,1101,TBM_SETPOS,TRUE,300);apply();return TRUE;}
        if(LOWORD(w)==IDOK){apply();Save();EndDialog(dialog,IDOK);return TRUE;}
        if(LOWORD(w)==IDCANCEL){const int original=(int)GetWindowLongPtrW(dialog,DWLP_USER);a.settings.petHeight=original;ApplyImage(a.settings.skinPath);EndDialog(dialog,IDCANCEL);return TRUE;}
    }
    if(msg==WM_CLOSE){SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);return TRUE;}
    if(msg==WM_DESTROY)KillTimer(dialog,1);return FALSE;
}
INT_PTR CALLBACK SkinNameProc(HWND dialog,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_INITDIALOG){auto text=(std::wstring*)l;SetWindowLongPtrW(dialog,DWLP_USER,l);
        SetDlgItemTextW(dialog,1001,text->c_str());SendDlgItemMessageW(dialog,1001,EM_SETLIMITTEXT,80,0);
        SendDlgItemMessageW(dialog,1001,EM_SETSEL,0,-1);SetFocus(GetDlgItem(dialog,1001));return FALSE;}
    if(msg==WM_COMMAND){if(LOWORD(w)==IDOK){wchar_t value[81]{};GetDlgItemTextW(dialog,1001,value,81);std::wstring name=value;
        const auto first=name.find_first_not_of(L" \t\r\n");
        if(first==std::wstring::npos){MessageBoxW(dialog,L"请输入皮肤名称。",L"自定义皮肤名称",MB_OK|MB_ICONINFORMATION);return TRUE;}
        name=name.substr(first,name.find_last_not_of(L" \t\r\n")-first+1);
        *(std::wstring*)GetWindowLongPtrW(dialog,DWLP_USER)=name;EndDialog(dialog,IDOK);return TRUE;}
        if(LOWORD(w)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}}
    if(msg==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}return FALSE;
}
void RenameSkin(){
    const bool wasBusy=a.busy;a.busy=true;auto name=CurrentSkinName();
    const auto result=DialogBoxParamW(a.instance,MAKEINTRESOURCEW(101),a.panel,SkinNameProc,(LPARAM)&name);
    if(result==IDOK){
        const auto named=a.settings.presetPath.empty()?a.settings.skinPath:(std::filesystem::path(SkinFolderBesideExe())/a.settings.presetPath).wstring();
        if(SetSkinDisplayName(named,name)){a.skins=ScanSkins();Notify(L"已保存皮肤名称："+name);}
        else Notify(L"皮肤名称保存失败，请检查程序文件夹写入权限。");
    } else if(result==-1)Notify(L"无法打开皮肤命名窗口。");
    a.busy=wasBusy;SetForegroundWindow(a.panel);
}
void ChooseImage(){
    a.busy=true;wchar_t path[32768]{};OPENFILENAMEW file{sizeof(file)};
    file.hwndOwner=a.panel;file.lpstrFilter=L"图片（PNG / JPG / BMP）\0*.png;*.jpg;*.jpeg;*.bmp\0所有文件\0*.*\0";
    file.lpstrFile=path;file.nMaxFile=32768;file.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    file.lpstrTitle=L"选择桌宠图片（推荐透明 PNG）";
    if(GetOpenFileNameW(&file)&&SelectSkin({path,L"",L"",L""})){
        std::wstring preset;
        if(SaveSkinPreset(path,preset)){
            a.settings.skinPath=preset;a.skins=ScanSkins();
            RenameSkin();
        }else Notify(L"图片已加载，但预设保存失败，请检查 skins 文件夹写入权限。");
        Save();
    }
    a.busy=false;SetForegroundWindow(a.panel);InvalidateRect(a.panel,nullptr,FALSE);
}
UINT Popup(HMENU menu){
    a.busy=true;POINT p{};GetCursorPos(&p);SetForegroundWindow(a.panel);
    const UINT result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,p.x,p.y,0,a.panel,nullptr);
    DestroyMenu(menu);a.busy=false;return result;
}
void ChooseProgram(){
    a.busy=true;wchar_t path[32768]{};OPENFILENAMEW file{sizeof(file)};
    file.hwndOwner=a.panel;file.lpstrFilter=L"程序或快捷方式（EXE / LNK）\0*.exe;*.lnk\0";
    file.lpstrFile=path;file.nMaxFile=32768;file.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_NODEREFERENCELINKS;
    file.lpstrTitle=L"选择桌宠要打开的软件";
    if(GetOpenFileNameW(&file)&&IsLaunchTarget(path)){a.settings.launchPath=path;Save();Notify(L"已保存指定软件，可在更多设置中打开。");}
    a.busy=false;SetForegroundWindow(a.panel);
}
void StartAppSearch(){
    if(a.appSearch.valid()){Notify(L"正在查找，请稍候。");return;}
    Notify(L"正在查找已安装或运行中的音乐软件…");
    a.appSearch=std::async(std::launch::async,FindMusicApps);SetTimer(a.pet,6,100,nullptr);
}
void AppSearchResults(){
    if(a.busy||!a.appSearch.valid()||a.appSearch.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
    KillTimer(a.pet,6);
    try{a.launchTargets=a.appSearch.get();}catch(...){Notify(L"自动查找失败，可手动选择程序。");return;}
    POINT point{a.settings.posX,a.settings.posY+a.image.height/2};ShowPanel(point);
    if(a.launchTargets.empty()){Notify(L"未找到音乐软件，请使用手动配置。");return;}
    auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING|MF_GRAYED,0,L"选择要保存的软件（此时不会启动）");
    for(size_t i=0;i<a.launchTargets.size()&&i<200;++i){
        auto label=a.launchTargets[i].label+L"  —  "+a.launchTargets[i].path;
        // Escape ampersands so paths are displayed literally in the menu.
        size_t at=0;while((at=label.find(L'&',at))!=std::wstring::npos){label.insert(at,1,L'&');at+=2;}
        AppendMenuW(menu,MF_STRING,CmdLaunchFound+(UINT)i,label.c_str());
    }
    auto result=Popup(menu);if(result)Command(result);
}
void Players(){
    auto menu=CreatePopupMenu();const auto selected=a.media.SelectedId();
    AppendMenuW(menu,MF_STRING|(selected.empty()?MF_CHECKED:0),CmdAuto,L"自动选择正在播放的会话");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    a.menuSessions=a.media.Sessions();
    for(size_t i=0;i<a.menuSessions.size()&&i<500;++i){const auto& s=a.menuSessions[i];auto label=s.appName+(s.playing?L"  · 正在播放":L"");
        AppendMenuW(menu,MF_STRING|(selected==s.id?MF_CHECKED:0),CmdSession+(UINT)i,label.c_str());}
    if(a.menuSessions.empty())AppendMenuW(menu,MF_STRING|MF_GRAYED,0,L"没有会话：请先在播放器中播放音乐");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    auto modes=CreatePopupMenu();
    AppendMenuW(modes,MF_STRING|(a.snapshot.canShuffle?0:MF_GRAYED)|(a.snapshot.shuffle==0?MF_CHECKED:0),CmdMode,L"关闭随机播放");
    AppendMenuW(modes,MF_STRING|(a.snapshot.canShuffle?0:MF_GRAYED)|(a.snapshot.shuffle==1?MF_CHECKED:0),CmdMode+1,L"随机播放");
    AppendMenuW(modes,MF_SEPARATOR,0,nullptr);
    const wchar_t* names[]={L"不循环",L"单曲循环",L"列表循环"};
    for(int i=0;i<3;++i)AppendMenuW(modes,MF_STRING|(a.snapshot.canRepeat?0:MF_GRAYED)|(a.snapshot.repeat==i?MF_CHECKED:0),CmdMode+2+i,names[i]);
    if(!a.snapshot.canShuffle&&!a.snapshot.canRepeat)AppendMenuW(modes,MF_STRING|MF_GRAYED,0,L"当前播放器未开放模式控制");
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)modes,L"音乐播放模式");
    auto result=Popup(menu);if(result)Command(result);
}
void Appearance(){
    auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,CmdChoose,L"选择图片并保存为预设…");
    AppendMenuW(menu,MF_STRING,CmdSkinName,L"自定义当前皮肤名称…");
    AppendMenuW(menu,MF_STRING,CmdMotionChoose,L"添加动态素材并保存组合预设…");
    AppendMenuW(menu,MF_STRING,CmdPairSave,L"另存当前静态＋动态组合…");
    auto speeds=CreatePopupMenu();for(int i=0;i<7;++i)AppendMenuW(speeds,MF_STRING|(a.settings.motionSpeed==motionSpeeds[i]?MF_CHECKED:0),CmdMotionSpeed+i,motionSpeedLabels[i]);
    AppendMenuW(menu,MF_POPUP|(a.settings.motionPath.empty()?MF_GRAYED:0),(UINT_PTR)speeds,L"动画速度");
    AppendMenuW(menu,MF_STRING|(a.settings.motionPath.empty()?MF_GRAYED:0)|(a.settings.motionPingPong?MF_CHECKED:0),CmdMotionBounce,L"往返播放");
    AppendMenuW(menu,MF_STRING|(a.settings.motionPath.empty()?MF_GRAYED:0),CmdMotionClear,L"关闭动态皮肤（始终显示静态图）");
    a.skins=ScanSkins();if(!a.skins.empty())AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    for(size_t i=0;i<a.skins.size()&&i<500;++i){const auto& skin=a.skins[i];
        const bool selected=skin.presetFile.empty()?(a.settings.presetPath.empty()&&skin.path==a.settings.skinPath):std::filesystem::path(skin.presetFile).filename()==a.settings.presetPath;
        auto label=skin.label+(skin.presetFile.empty()?L"":L" [组合]");
        AppendMenuW(menu,MF_STRING|(selected?MF_CHECKED:0),CmdSkin+(UINT)i,label.c_str());}
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,CmdSizeSlider,L"调整桌宠大小…");
    auto result=Popup(menu);if(result)Command(result);
}
void More(){
    auto menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING|(IsAutoStartEnabled()?MF_CHECKED:0),CmdAutoStart,L"开机自启");
    AppendMenuW(menu,MF_STRING|(a.settings.launchPath.empty()?MF_GRAYED:0),CmdLaunch,L"打开指定软件");
    auto launch=CreatePopupMenu();
    AppendMenuW(launch,MF_STRING,CmdLaunchChoose,L"手动配置程序…");
    AppendMenuW(launch,MF_STRING,CmdLaunchFind,L"自动查找音乐软件…");
    AppendMenuW(launch,MF_STRING|(a.settings.launchPath.empty()?MF_GRAYED:0),CmdLaunchClear,L"清除指定软件");
    if(!a.settings.launchPath.empty()){auto label=L"当前："+a.settings.launchPath;AppendMenuW(launch,MF_STRING|MF_GRAYED,0,label.c_str());}
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)launch,L"配置启动软件");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,CmdPreview,L"音符预览（8 秒）");
    auto density=CreatePopupMenu();const wchar_t* labels[]={L"轻盈",L"适中",L"热闹"};
    for(int i=1;i<=3;++i)AppendMenuW(density,MF_STRING|(a.settings.density==i?MF_CHECKED:0),CmdDensity+i,labels[i-1]);
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)density,L"音符数量");
    auto palette=CreatePopupMenu();
    AppendMenuW(palette,MF_STRING|(a.settings.notePalette==0?MF_CHECKED:0),CmdPalette,L"默认配色");
    AppendMenuW(palette,MF_STRING|(a.settings.notePalette==1?MF_CHECKED:0),CmdPalette+1,L"跟随图片");
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)palette,L"音符配色");
    AppendMenuW(menu,MF_STRING|(a.settings.locked?MF_CHECKED:0),CmdLock,L"锁定桌宠位置");
    AppendMenuW(menu,MF_STRING|(a.settings.gameMode?MF_CHECKED:0),CmdGame,L"游戏模式（穿透、减少音符）");
    AppendMenuW(menu,MF_STRING,CmdReset,L"找回桌宠 / 重置位置");
    if(!a.trayAdded)AppendMenuW(menu,MF_STRING,CmdTrayRetry,L"重试托盘注册");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    auto keys=CreatePopupMenu();
    for(int i=0;i<4;++i)AppendMenuW(keys,MF_STRING|(a.settings.hotkeyPreset==i?MF_CHECKED:0),CmdHotkey+i,shortcuts[i].label);
    AppendMenuW(keys,MF_STRING|(a.settings.hotkeyPreset==4?MF_CHECKED:0),CmdHotkey+4,L"关闭全局快捷键");
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)keys,L"穿透快捷键（首选，冲突时自动备用）");
    auto actual=L"当前："+ShortcutText();AppendMenuW(menu,MF_STRING|MF_GRAYED,0,actual.c_str());
    AppendMenuW(menu,MF_STRING|MF_GRAYED,0,L"再次启动 MusicPet.exe 可关闭穿透并打开面板");
    auto result=Popup(menu);if(result)Command(result);
}
void UpdateAnimation(){
    const bool emit=a.settings.effectsOn&&(a.snapshot.status==PlayStatus::Playing||GetTickCount64()<a.previewUntil);
    a.notes.SetPlaying(emit);
    if(emit&&!a.animating){a.animating=true;a.lastFrame=GetTickCount64();SetTimer(a.pet,1,a.settings.gameMode?50:33,nullptr);}
}
void Command(int id){
    if((id>=CmdMotionSpeed&&id<CmdMotionSpeed+7)||id==CmdMotionBounce){
        if(a.settings.motionPath.empty())return;
        const int speed=id==CmdMotionBounce?a.settings.motionSpeed:motionSpeeds[id-CmdMotionSpeed];
        const bool bounce=id==CmdMotionBounce?!a.settings.motionPingPong:a.settings.motionPingPong;
        if(!a.settings.presetPath.empty()&&!SavePairedMotionOptions((std::filesystem::path(SkinFolderBesideExe())/a.settings.presetPath).wstring(),speed,bounce)){
            Notify(L"动画设置保存失败，当前设置未更改。");return;}
        a.settings.motionSpeed=speed;a.settings.motionPingPong=bounce;
        a.motionFailed=false;a.motionVisible=false;DrawPet(a.image);
        a.motion.Configure(a.settings.motionPath,a.image.width,a.image.height,speed,bounce);SyncMotion();Save();
        return;
    }
    if(id==CmdMotionChoose){ChooseMotion();return;}
    if(id==CmdMotionClear){a.settings.presetPath.clear();a.settings.motionPath.clear();ApplyImage(a.settings.skinPath);Save();return;}
    if(id==CmdPairSave){if(SaveCurrentPair(a.settings.motionPath))RenameSkin();else Notify(L"组合预设保存失败，请检查素材文件和文件夹权限。");return;}
    if(id==CmdSizeSlider){a.busy=true;DialogBoxParamW(a.instance,MAKEINTRESOURCEW(102),a.panel,SizeProc,0);a.busy=false;return;}
    if(id>=CmdMode&&id<=CmdMode+4){a.media.Send(static_cast<MediaCommand>(CMD_SHUFFLE_OFF+id-CmdMode));return;}
    if(id==CmdAutoStart){
        const bool enabled=!IsAutoStartEnabled();
        if(!SetAutoStartEnabled(enabled))Notify(L"无法修改开机自启设置，请检查当前用户权限。");
        else Notify(enabled?L"已开启开机自启，下次登录 Windows 时启动。":L"已关闭开机自启。");
        return;
    }
    if(id==CmdTrayRetry){AddTray();UpdateTray();Notify(a.trayAdded?L"托盘注册成功，请查看隐藏图标区域。":L"系统仍拒绝托盘注册，请查看诊断文件。");InvalidateRect(a.panel,nullptr,FALSE);return;}
    if(id==CmdSkinName){RenameSkin();return;}
    if(id>=CmdLaunchFound&&id<CmdLaunchFound+200){size_t i=id-CmdLaunchFound;if(i<a.launchTargets.size()){
        a.settings.launchPath=a.launchTargets[i].path;Save();Notify(L"已保存指定软件，可从更多设置打开。");}return;}
    if(id==CmdLaunchChoose){ChooseProgram();return;}
    if(id==CmdLaunchFind){StartAppSearch();return;}
    if(id==CmdLaunchClear){a.settings.launchPath.clear();Save();Notify(L"已清除指定软件。");return;}
    if(id==CmdLaunch){if(!OpenLaunchTarget(a.settings.launchPath,a.panel))Notify(L"无法打开指定软件，请检查路径或重新配置。");return;}

    if(id==CmdPalette||id==CmdPalette+1){a.settings.notePalette=id-CmdPalette;
        a.notes.SetPalette(a.settings.notePalette,a.image.bits,a.image.width,a.image.height);
        Save();Notify(L"已切换音符配色，新出现的音符将使用新颜色。");return;}
    if(id>=CmdHotkey&&id<=CmdHotkey+4){a.settings.hotkeyPreset=id-CmdHotkey;ConfigureShortcut();UpdateTray();Save();Notify(L"当前穿透快捷键："+ShortcutText());return;}
    if(id>=CmdSession&&id<CmdSession+500){const size_t i=id-CmdSession;if(i<a.menuSessions.size())a.media.Select(a.menuSessions[i].id);return;}
    if(id>=CmdSkin&&id<CmdSkin+500){const size_t i=id-CmdSkin;if(i<a.skins.size()&&SelectSkin(a.skins[i]))Save();return;}
    if(id>=CmdSize+100&&id<=CmdSize+600){a.settings.petHeight=id-CmdSize;ApplyImage(a.settings.skinPath);Save();return;}
    // Density command IDs are deliberately outside the size range.
    if(id>=901&&id<=903){a.settings.density=id-900;}
    else switch(id){
    case CmdChoose:ChooseImage();return;
    case CmdPreview:a.previewUntil=GetTickCount64()+8000;a.settings.effectsOn=1;a.preview=true;a.notes.Preview(8);break;
    case CmdThrough:a.settings.clickThrough=!a.settings.clickThrough;ApplyThrough();break;
    case CmdEffects:a.settings.effectsOn=!a.settings.effectsOn;if(!a.settings.effectsOn){a.previewUntil=0;a.preview=false;a.notes.Preview(0);}break;
    case CmdLock:a.settings.locked=!a.settings.locked;break;
    case CmdGame:a.settings.gameMode=!a.settings.gameMode;if(a.settings.gameMode)a.settings.clickThrough=true;ApplyThrough();if(a.animating)SetTimer(a.pet,1,a.settings.gameMode?50:33,nullptr);break;
    case CmdReset:a.settings.clickThrough=false;ApplyThrough();ApplyImage(a.settings.skinPath,true);break;
    case CmdAuto:a.media.Select(L"");return;
    case CmdExit:DestroyWindow(a.pet);return;
    default:return;
    }
    a.notes.Configure(a.settings.effectsOn?(a.settings.gameMode?1:a.settings.density):0,ComputeNoteLayout(a.image.width,a.image.height,a.dpi).scale);UpdateAnimation();UpdateTray();Save();InvalidateRect(a.panel,nullptr,FALSE);
}
void ActivateHit(int id){
    if(!Enabled(id))return;
    if(id==1)a.media.Send(CMD_PREV);if(id==2)a.media.Send(CMD_TOGGLE);if(id==3)a.media.Send(CMD_NEXT);
    if(id==10)ShowWindow(a.panel,SW_HIDE);if(id==11)Players();if(id==12)Appearance();if(id==13)Command(CmdEffects);if(id==14)Command(CmdThrough);if(id==15)More();if(id==16)Command(CmdExit);
}
void ShowPanel(POINT p){
    a.snapshot=a.media.Snapshot();MONITORINFO m{sizeof(m)};GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&m);
    a.panelDpi=(int)GetDpiForWindow(a.pet);const float scale=a.panelDpi/96.f;const int w=(int)(PanelW*scale),h=(int)(PanelH*scale);
    int x=p.x-w-12,y=p.y-h/2; x=std::clamp(x,(int)m.rcWork.left+8,std::max((int)m.rcWork.left+8,(int)m.rcWork.right-w-8));
    y=std::clamp(y,(int)m.rcWork.top+8,std::max((int)m.rcWork.top+8,(int)m.rcWork.bottom-h-8));
    a.hover=0;a.pressed=0;SetWindowPos(a.panel,HWND_TOPMOST,x,y,w,h,SWP_SHOWWINDOW);
    SetWindowRgn(a.panel,CreateRoundRectRgn(0,0,w+1,h+1,(int)(22*scale),(int)(22*scale)),TRUE);
    SetForegroundWindow(a.panel);SetFocus(a.panel);InvalidateRect(a.panel,nullptr,FALSE);
    a.panelOpened=GetTickCount64();a.outsideButtonDown=true;SetTimer(a.panel,3,80,nullptr);
}
void AddTray(){
    auto n=TrayData();n.uFlags|=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_SHOWTIP;n.uCallbackMessage=TrayMessage;n.hIcon=a.icon;
    wcscpy_s(n.szTip,L"音乐桌宠 · 右键打开控制面板");
    // A duplicate ADD can happen if Explorer hasn't discarded the old entry yet.
    a.trayAdded=Shell_NotifyIconW(NIM_ADD,&n)!=FALSE;
    if(!a.trayAdded)a.trayAdded=Shell_NotifyIconW(NIM_MODIFY,&n)!=FALSE;
    // Explorer binds GUID icons to an executable identity/path. A copied build
    // can be denied (ERROR_ACCESS_DENIED); recover with the standard HWND/uID API.
    if(!a.trayAdded&&a.trayUseGuid){
        a.trayUseGuid=false;n=TrayData();
        n.uFlags|=NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_SHOWTIP;n.uCallbackMessage=TrayMessage;n.hIcon=a.icon;
        wcscpy_s(n.szTip,L"音乐桌宠 · 右键打开控制面板");
        a.trayAdded=Shell_NotifyIconW(NIM_ADD,&n)!=FALSE;
        if(!a.trayAdded)a.trayAdded=Shell_NotifyIconW(NIM_MODIFY,&n)!=FALSE;
    }
    const auto trayError=a.trayAdded?0:GetLastError();
    const auto diagnostics=ExeDir()+L"\\MusicPet.diagnostics.ini";
    WritePrivateProfileStringW(L"tray",L"registered",a.trayAdded?L"1":L"0",diagnostics.c_str());
    WritePrivateProfileStringW(L"tray",L"lastError",std::to_wstring(trayError).c_str(),diagnostics.c_str());
    WritePrivateProfileStringW(L"tray",L"identity",a.trayUseGuid?L"GUID":L"HWND",diagnostics.c_str());
    SetPropW(a.pet,L"MusicPet.TrayUsesGuid",(HANDLE)(INT_PTR)(a.trayUseGuid?1:0));
    SetPropW(a.pet,L"MusicPet.TrayRegistered",(HANDLE)(INT_PTR)(a.trayAdded?1:0));
    a.trayV4=false;
    if(a.trayAdded){n.uVersion=NOTIFYICON_VERSION_4;a.trayV4=Shell_NotifyIconW(NIM_SETVERSION,&n)!=FALSE;}
}
void Recover(){
    if(!a.ready){a.recoverPending=true;return;}
    a.settings.clickThrough=false;ApplyThrough();
    int x=a.settings.posX,y=a.settings.posY;ClampPosition(x,y,a.image.width,a.image.height);Place(x,y);
    ShowWindow(a.pet,SW_SHOWNOACTIVATE);Save();
    POINT p{x,y+a.image.height/2};ShowPanel(p);Notify(L"已关闭点击穿透，可以操作桌宠了。");
}
LRESULT CALLBACK PanelProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);Bitmap buffer(std::max(1L,r.right),std::max(1L,r.bottom),PixelFormat32bppPARGB);
        {Graphics g(&buffer);g.ScaleTransform(a.panelDpi/96.f,a.panelDpi/96.f);PaintPanel(g);}Graphics output(dc);output.DrawImage(&buffer,0,0);EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEMOVE:{int hit=HitAt(l);if(hit!=a.hover){a.hover=hit;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);SetCursor(LoadCursorW(nullptr,hit&&Enabled(hit)?IDC_HAND:IDC_ARROW));return 0;}
    case WM_MOUSELEAVE:a.hover=0;InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:a.pressed=HitAt(l);SetCapture(h);return 0;
    case WM_LBUTTONUP:{int hit=HitAt(l),pressed=a.pressed;a.pressed=0;ReleaseCapture();if(hit==pressed)ActivateHit(hit);return 0;}
    case WM_KEYDOWN:if(w==VK_ESCAPE){ShowWindow(h,SW_HIDE);return 0;}if(w==VK_TAB){size_t index=0;for(size_t i=0;i<std::size(hits);++i)if(hits[i].id==a.hover)index=i+1;for(size_t i=0;i<std::size(hits);++i){int id=hits[(index+i)%std::size(hits)].id;if(Enabled(id)){a.hover=id;break;}}InvalidateRect(h,nullptr,FALSE);return 0;}if(w==VK_RETURN||w==VK_SPACE){ActivateHit(a.hover);return 0;}break;
    case WM_TIMER:if(w==3){
        const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000);
        if(down&&!a.outsideButtonDown&&!a.busy&&GetTickCount64()-a.panelOpened>300){
            POINT p;RECT r;GetCursorPos(&p);GetWindowRect(h,&r);
            if(!PtInRect(&r,p))ShowWindow(h,SW_HIDE);
        }
        a.outsideButtonDown=down;return 0;
    }break;
    case WM_SHOWWINDOW:if(!w)KillTimer(h,3);break;
    case WM_ACTIVATE:return 0;
    case WM_DPICHANGED:{
        a.panelDpi=HIWORD(w);const auto suggested=(RECT*)l;
        const int width=MulDiv(PanelW,a.panelDpi,96),height=MulDiv(PanelH,a.panelDpi,96);
        int x=suggested->left,y=suggested->top;ClampPosition(x,y,width,height);
        SetWindowPos(h,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);
        SetWindowRgn(h,CreateRoundRectRgn(0,0,width+1,height+1,MulDiv(22,a.panelDpi,96),MulDiv(22,a.panelDpi,96)),TRUE);
        InvalidateRect(h,nullptr,FALSE);return 0;
    }
    case WM_CLOSE:ShowWindow(h,SW_HIDE);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
LRESULT CALLBACK FxProc(HWND h,UINT msg,WPARAM w,LPARAM l){if(msg==WM_NCHITTEST)return HTTRANSPARENT;if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;return DefWindowProcW(h,msg,w,l);}
LRESULT CALLBACK PetProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    static const UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(msg==taskbarCreated){a.trayAdded=false;AddTray();return 0;}
    switch(msg){
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:if(!a.settings.locked){POINT p;GetCursorPos(&p);RECT r;GetWindowRect(h,&r);a.dragOffset={p.x-r.left,p.y-r.top};a.dragging=true;SetCapture(h);}return 0;
    case WM_MOUSEMOVE:if(a.dragging){POINT p;GetCursorPos(&p);Place(p.x-a.dragOffset.x,p.y-a.dragOffset.y);}return 0;
    case WM_LBUTTONUP:if(a.dragging){a.dragging=false;ReleaseCapture();int x=a.settings.posX,y=a.settings.posY;ClampPosition(x,y,a.image.width,a.image.height);Place(x,y);Save();}return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:if(a.dragging){a.dragging=false;Save();}return 0;
    case WM_RBUTTONUP:{POINT p;GetCursorPos(&p);ShowPanel(p);return 0;}
    case RecoverMessage:Recover();return 0;
    case WM_COMMAND:Command(LOWORD(w));return 0;
    case WM_HOTKEY:if(w==1&&a.hotkey)Command(CmdThrough);return 0;
    case WM_APP_MEDIA_UPDATE:a.snapshot=a.media.Snapshot();SyncMotion();UpdateAnimation();UpdateTray();if(IsWindowVisible(a.panel))InvalidateRect(a.panel,nullptr,FALSE);return 0;
    case WM_APP+3:Notify(L"播放器未接受这次控制，请稍后重试。");return 0;
    case TrayMessage:{const UINT event=a.trayV4?LOWORD(l):(UINT)l;if(event==WM_CONTEXTMENU||event==NIN_SELECT||event==NIN_KEYSELECT||event==WM_RBUTTONUP){POINT p;GetCursorPos(&p);ShowPanel(p);}return 0;}
    case WM_TIMER:
        if(w==7){
            if(a.motion.Failed()){a.motionFailed=true;KillTimer(h,7);DrawPet(a.image);a.motionVisible=false;Notify(L"动态皮肤解码或缓存失败，已显示静态图。可更换素材、关闭往返或调小尺寸（往返缓存上限 128 MB）。");return 0;}
            if(!a.motion.Active()){KillTimer(h,7);if(a.motionVisible)DrawPet(a.image);a.motionVisible=false;return 0;}
            std::vector<BYTE> pixels;if(a.motion.Frame(pixels)){
                if(!a.motionImage.dc)CreateDib(a.image.width,a.image.height,a.motionImage);
                if(a.motionImage.dc&&pixels.size()==size_t(a.image.width)*a.image.height*4){memcpy(a.motionImage.bits,pixels.data(),pixels.size());DrawPet(a.motionImage);a.motionVisible=true;}
            }return 0;
        }
        if(w==1){const auto now=GetTickCount64();const double dt=std::min(.1,(now-a.lastFrame)/1000.);a.lastFrame=now;a.preview=now<a.previewUntil;UpdateAnimation();
            if(a.notes.Tick(dt))a.notes.Render();else{KillTimer(h,1);a.animating=false;a.notes.Hide();}return 0;}
        if(w==6){AppSearchResults();return 0;}
        if(w==5){if(!a.trayAdded)AddTray();else UpdateTray();return 0;}
        if(w==2){DestroyWindow(h);return 0;}
        return 0;
    case WM_DPICHANGED:
        a.dpi=HIWORD(w);
        // Resizing a newly created window can dispatch this inside ApplyImage.
        // Reload after the current layout completes, never recursively.
        if(!a.dpiReloadPending){a.dpiReloadPending=true;PostMessageW(h,WM_APP+4,0,0);}return 0;
    case WM_APP+4:a.dpiReloadPending=false;if(a.image.dc&&!a.settings.skinPath.empty())ApplyImage(a.settings.skinPath);return 0;
    case WM_DISPLAYCHANGE:{int x=a.settings.posX,y=a.settings.posY;ClampPosition(x,y,a.image.width,a.image.height);Place(x,y);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_DESTROY:
        if(a.shuttingDown)return 0;a.shuttingDown=true;
        KillTimer(h,1);KillTimer(h,2);KillTimer(h,5);KillTimer(h,6);KillTimer(h,7);a.motion.Stop();a.motionImage.Reset();RemovePropW(h,L"MusicPet.ActiveShortcut");UnregisterHotKey(h,1);a.media.Stop();
        if(a.saveOnExit)Save();if(a.trayAdded){auto n=TrayData();Shell_NotifyIconW(NIM_DELETE,&n);a.trayAdded=false;}
        if(a.panel)DestroyWindow(a.panel);if(a.fx)DestroyWindow(a.fx);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
int PngEncoder(CLSID& id){UINT count{},size{};GetImageEncodersSize(&count,&size);std::vector<BYTE> data(size);auto encoders=(ImageCodecInfo*)data.data();GetImageEncoders(count,size,encoders);for(UINT i=0;i<count;++i)if(wcscmp(encoders[i].MimeType,L"image/png")==0){id=encoders[i].Clsid;return 0;}return 1;}
bool RenderPreview(){
    PetImage pet;if(!LoadPngImage(ExeDir()+L"\\skins\\大肥鱼.png",600,420,pet))return false;
    CLSID encoder;if(PngEncoder(encoder)){pet.Reset();return false;}
    bool ok=true;
    for(int theme=0;theme<2;++theme){
        Bitmap image(980,660,PixelFormat32bppPARGB);
        {Graphics g(&image);g.SetSmoothingMode(SmoothingModeAntiAlias);g.Clear(theme?Color(255,242,244,240):Color(255,30,41,47));
        Text(g,L"MUSIC PET",{42,24,350,26},13,theme?muted:Color(255,167,190,187),true);
        Text(g,L"静态陪伴 · 轻盈音符",{42,55,450,40},25,theme?ink:Color(255,235,244,235),true);
        NoteLayer notes;notes.Configure(2,1.15f);notes.SetOrigin(650,355,pet.width*.60f);notes.SetPlaying(true);for(int i=0;i<105;++i)notes.Tick(1./30);notes.Paint(g);
        Bitmap artwork(pet.width,pet.height,pet.width*4,PixelFormat32bppPARGB,(BYTE*)pet.bits);g.DrawImage(&artwork,650-pet.width/2,205,pet.width,pet.height);
        auto saved=a.snapshot;a.snapshot={};a.snapshot.valid=true;a.snapshot.appName=L"网易云音乐";a.snapshot.title=L"此刻，和音乐待在一起";a.snapshot.artist=L"控制面板外观预览";a.snapshot.status=PlayStatus::Playing;a.snapshot.canToggle=a.snapshot.canNext=a.snapshot.canPrev=true;
        a.snapshot.positionSec=84;a.snapshot.durationSec=238;
        const auto state=g.Save();g.TranslateTransform(40.f,131.f);PaintPanel(g);g.Restore(state);a.snapshot=saved;
        Text(g,L"右键打开控制 · 拖动移动 · Ctrl + Alt + P 点击穿透",{440,621,510,20},11,theme?muted:Color(255,167,190,187));}
        const auto file=ExeDir()+L"\\qa\\preview-"+(theme?L"light.png":L"dark.png");if(image.Save(file.c_str(),&encoder)!=Ok)ok=false;
    }
    pet.Reset();return ok;
}
}
int APIENTRY wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};InitCommonControlsEx(&common);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    GdiplusStartupInput input;ULONG_PTR token{};if(GdiplusStartup(&token,&input,nullptr)!=Ok)return 1;
    int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool preview=false,render=false,panel=false,recover=false;DWORD quitAfter{};
    for(int i=1;i<argc;++i){std::wstring value=argv[i];if(value==L"--preview")preview=true;if(value==L"--render-preview")render=true;if(value==L"--panel")panel=true;if(value==L"--recover")recover=true;
        if(value.rfind(L"--quit-after=",0)==0)quitAfter=(DWORD)_wtoi(value.c_str()+13);}
    LocalFree(argv);a.instance=instance;
    if(render){const bool ok=RenderPreview();GdiplusShutdown(token);if(SUCCEEDED(com))CoUninitialize();return ok?0:2;}
    HANDLE single=CreateMutexW(nullptr,TRUE,L"Local\\MusicPet.Native.Rewrite.v2");
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        HWND existing{};for(int i=0;i<60&&!existing;++i){existing=FindWindowW(L"MusicPet.Native.Pet",nullptr);if(!existing)Sleep(50);}
        if(existing){AllowSetForegroundWindow(ASFW_ANY);PostMessageW(existing,RecoverMessage,0,0);}
        else MessageBoxW(nullptr,L"桌宠尚未完成启动，请稍后再次运行恢复。",L"音乐桌宠",MB_ICONINFORMATION);
        if(single)CloseHandle(single);GdiplusShutdown(token);if(SUCCEEDED(com))CoUninitialize();return 0;
    }
    LoadSettings(a.settings);
    if(!a.settings.presetPath.empty()){
        SkinItem pair;const auto relative=std::filesystem::path(a.settings.presetPath).filename();
        if(LoadPairedPreset((std::filesystem::path(SkinFolderBesideExe())/relative).wstring(),pair)){
            a.settings.skinPath=pair.path;a.settings.motionPath=pair.motionPath;a.settings.presetPath=relative.wstring();
            a.settings.motionSpeed=pair.motionSpeed;a.settings.motionPingPong=pair.motionPingPong;
        }else {a.settings.presetPath.clear();a.settings.motionPath.clear();}
    }
    a.dpi=GetDpiForSystem();a.saveOnExit=quitAfter==0;
    WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpfnWndProc=PetProc;wc.lpszClassName=L"MusicPet.Native.Pet";
    RegisterClassExW(&wc);wc.lpfnWndProc=FxProc;wc.lpszClassName=L"MusicPet.Native.Notes";RegisterClassExW(&wc);
    wc.lpfnWndProc=PanelProc;wc.lpszClassName=L"MusicPet.Native.Panel";wc.style=CS_DROPSHADOW;RegisterClassExW(&wc);
    const DWORD ex=WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST;
    a.fx=CreateWindowExW(ex|WS_EX_TRANSPARENT,L"MusicPet.Native.Notes",L"音乐桌宠 · 音符",WS_POPUP,0,0,200,200,nullptr,nullptr,instance,nullptr);
    a.pet=CreateWindowExW(ex,L"MusicPet.Native.Pet",L"音乐桌宠",WS_POPUP,0,0,200,300,nullptr,nullptr,instance,nullptr);
    a.panel=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"MusicPet.Native.Panel",L"一起听歌 · 音乐桌宠",WS_POPUP,0,0,PanelW,PanelH,a.pet,nullptr,instance,nullptr);
    if(!a.fx||!a.pet||!a.panel){MessageBoxW(nullptr,L"窗口创建失败。",L"音乐桌宠",MB_ICONERROR);return 3;}
    Trace(L"windows-created");a.notes.Create(a.fx,200,200);auto imagePath=a.settings.skinPath;
    if(imagePath.empty()||GetFileAttributesW(imagePath.c_str())==INVALID_FILE_ATTRIBUTES)imagePath=ExeDir()+L"\\skins\\大肥鱼.png";
    if(!ApplyImage(imagePath)){MessageBoxW(nullptr,L"请把一张透明 PNG 命名为 大肥鱼.png 放入 skins 文件夹。",L"音乐桌宠",MB_ICONERROR);DestroyWindow(a.pet);a.image.Reset();GdiplusShutdown(token);return 4;}
    Trace(L"before-through");ApplyThrough();Trace(L"before-icon");a.icon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);
    if(!a.icon)a.icon=CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));Trace(L"before-tray");AddTray();Trace(L"after-tray");
    ConfigureShortcut();SetTimer(a.pet,5,5000,nullptr);UpdateTray();
    if(!a.trayAdded){a.settings.clickThrough=false;ApplyThrough();panel=true;Notify(L"托盘注册失败，已关闭穿透；再次启动也可恢复。");}
    Trace(L"before-media");a.media.Start(a.pet);Trace(L"after-media");
    a.ready=true;if(recover||a.recoverPending)Recover();
    if(preview)Command(CmdPreview);if(quitAfter)SetTimer(a.pet,2,quitAfter,nullptr);
    if(panel){Trace(L"before-panel");POINT p{a.settings.posX,a.settings.posY+a.image.height/2};ShowPanel(p);Trace(L"after-panel");}
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    a.media.Stop();a.image.Reset();if(a.icon)DestroyIcon(a.icon);if(single)CloseHandle(single);
    // NoteLayer owns a GDI+ bitmap: release it before shutting GDI+ down.
    a.notes.Shutdown();
    GdiplusShutdown(token);if(SUCCEEDED(com))CoUninitialize();return 0;
}
