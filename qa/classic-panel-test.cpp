// Component tests exercise the actual native panel, without desktop input,
// changing logon startup, or writing the user's configuration.
#include "../src/main.cpp"
#include <fstream>
#include <stdexcept>

static void CheckPanel(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static LPARAM PanelPoint(float x,float y){return MAKELPARAM((int)std::lround(x*a.panelDpi/96.f),(int)std::lround(y*a.panelDpi/96.f));}
static void ClickPanel(int id){BuildPanelHits();auto r=HitRect(id);CheckPanel(r.Width>0,"Missing control");auto p=PanelPoint(r.X+r.Width/2,r.Y+r.Height/2);SendMessageW(a.panel,WM_LBUTTONDOWN,MK_LBUTTON,p);SendMessageW(a.panel,WM_LBUTTONUP,0,p);}
static void PanelPng(const wchar_t* name,int dpi=192){
    a.panelDpi=dpi;Bitmap image(MulDiv(PanelW,dpi,96),MulDiv(PanelHeight(),dpi,96),PixelFormat32bppPARGB);
    {Graphics g(&image);g.ScaleTransform(dpi/96.f,dpi/96.f);PaintPanel(g);}
    CLSID encoder;CheckPanel(!PngEncoder(encoder)&&image.Save(name,&encoder)==Ok,"PNG export");
}
int wmain(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);GdiplusStartupInput input;ULONG_PTR token{};GdiplusStartup(&token,&input,nullptr);
    a.saveOnExit=false;a.instance=GetModuleHandleW(nullptr);a.dpi=96;
    WNDCLASSW cls{};cls.hInstance=a.instance;cls.lpfnWndProc=PanelProc;cls.lpszClassName=L"MusicPet.ClassicPanel.Component";RegisterClassW(&cls);
    a.panel=CreateWindowW(cls.lpszClassName,L"Component test",WS_POPUP,0,0,PanelW,PanelH,nullptr,nullptr,a.instance,nullptr);
    a.pet=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Isolated pet",WS_POPUP,0,0,200,300,nullptr,nullptr,a.instance,nullptr);
    a.fx=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Isolated effects",WS_POPUP,0,0,200,300,nullptr,nullptr,a.instance,nullptr);
    a.notes.Create(a.fx,200,300);
    int result=0;std::ofstream report("classic-panel-report.txt");
    try{
        CheckPanel(a.panel&&a.pet&&a.fx,"Component windows");
        a.settings.skinPath=ExeDir()+L"\\skins\\大肥鱼.png";a.settings.petHeight=300;
        CheckPanel(ApplyImage(a.settings.skinPath),"Static image load");
        a.settings.motionPath=ExeDir()+L"\\skins\\mika\\mika.apng";a.settings.presetPath=L"mika.pet.ini";panelSkinName=L"mika";
        a.snapshot.valid=true;a.snapshot.title=L"晚风";a.snapshot.artist=L"演示曲目";a.snapshot.appName=L"MusicFree";a.snapshot.status=PlayStatus::Playing;
        a.snapshot.canToggle=a.snapshot.canPrev=a.snapshot.canNext=a.snapshot.canShuffle=a.snapshot.canRepeat=true;a.snapshot.durationSec=176;a.snapshot.positionSec=74;a.snapshot.repeat=2;
        a.trayAdded=true;
        const wchar_t* images[]={L"music.png",L"pet.png",L"notes.png",L"settings.png"};
        for(int page=0;page<4;++page){
            panelPage=page;BuildPanelHits();PanelPng(images[page]);
            for(int dpi:{96,120,144,168,192}){
                panelRequestedDpi=dpi;a.panelDpi=dpi;SetPanelPage(page);BuildPanelHits();
                for(size_t i=0;i<hits.size();++i){auto& h=hits[i];auto r=h.rect;
                    CheckPanel(r.X>=0&&r.Y>=0&&r.GetRight()<=PanelW&&r.GetBottom()<=PanelHeight(),"Control outside panel");
                    CheckPanel(HitAt(PanelPoint(r.X+r.Width/2,r.Y+r.Height/2))==h.id,"DPI hit mapping");
                    for(size_t j=i+1;j<hits.size();++j){const auto other=hits[j].rect;CheckPanel(!(r.X<other.GetRight()&&r.GetRight()>other.X&&r.Y<other.GetBottom()&&r.GetBottom()>other.Y),"Overlapping controls");}
                }
                // Use the actual mouse message path to switch between pages.
                ClickPanel(UiTabMusic+(page+1)%4);CheckPanel(panelPage==(page+1)%4,"Mouse tab navigation");SetPanelPage(page);
            }
        }
        report<<"PASS: four native page renders, control boundaries, non-overlap and mouse hit mapping at 96/120/144/168/192 DPI\n";
        for(int page=0;page<4;++page){panelPage=page;for(int dpi:{96,168,192,240,288}){
            for(const RECT work:{RECT{0,0,1366,728},RECT{-1920,-1080,0,-40},RECT{0,0,3840,2080}}){
                const int fit=FitPanelDpi(dpi,work);
                CheckPanel(fit<=dpi&&MulDiv(PanelW,fit,96)<=work.right-work.left-16&&MulDiv(PanelHeight(),fit,96)<=work.bottom-work.top-16,"Panel must fit monitor work area");
            }
        }}
        report<<"PASS: work-area fit through 300 percent scaling, short screens and negative monitor coordinates\n";
        SetPanelPage(0);a.hover=UiTabMusic;SendMessageW(a.panel,WM_KEYDOWN,VK_RIGHT,0);CheckPanel(panelPage==1,"Arrow tab navigation");
        SendMessageW(a.panel,WM_KEYDOWN,VK_END,0);CheckPanel(panelPage==3,"End tab navigation");
        SendMessageW(a.panel,WM_KEYDOWN,VK_HOME,0);CheckPanel(panelPage==0,"Home tab navigation");
        a.hover=0;SendMessageW(a.panel,WM_KEYDOWN,VK_TAB,0);CheckPanel(a.hover==10,"Tab starts at first control");
        report<<"PASS: keyboard Tab, left/right and Home/End page navigation\n";
        SetPanelPage(2);ClickPanel(UiDensityDense);CheckPanel(a.settings.density==3,"Density selection");
        ClickPanel(UiPaletteImage);CheckPanel(a.settings.notePalette==1,"Palette selection");
        const auto effects=a.settings.effectsOn;ClickPanel(CmdEffects);CheckPanel(a.settings.effectsOn!=effects,"Effect toggle");
        ClickPanel(CmdEffects);CheckPanel(a.settings.effectsOn==effects,"Effect toggle restores");
        SetPanelPage(3);ClickPanel(CmdLock);CheckPanel(a.settings.locked,"Lock toggle");ClickPanel(CmdLock);
        ClickPanel(CmdThrough);CheckPanel(a.settings.clickThrough,"Clickthrough toggle");ClickPanel(CmdThrough);
        a.settings.launchPath.clear();CheckPanel(!Enabled(CmdLaunch)&&!Enabled(CmdLaunchClear),"No launch target must disable actions");
        a.snapshot.reconnecting=true;CheckPanel(!Enabled(1)&&!Enabled(2)&&!Enabled(3)&&!Enabled(UiShuffle)&&!Enabled(UiRepeat),"Reconnect must disable media controls");
        SetPanelPage(0);PanelPng(L"reconnecting.png");a.snapshot.reconnecting=false;
        a.snapshot.durationSec=0;PanelPng(L"unknown-progress.png");
        report<<"PASS: effect/density/palette, lock/clickthrough toggles and unavailable/reconnecting capabilities\n";
        a.settings.motionPath.clear();a.settings.presetPath.clear();a.snapshot.status=PlayStatus::Paused;
        a.motion.Stop();SetPanelPage(1);a.panelDpi=168;
        CheckPanel(!Enabled(UiSpeed)&&!Enabled(CmdMotionBounce)&&!Enabled(CmdMotionClear),"Static-only animation capabilities");
        SendMessageW(a.panel,WM_LBUTTONDOWN,MK_LBUTTON,PanelPoint(216,414));
        SendMessageW(a.panel,WM_MOUSEMOVE,MK_LBUTTON,PanelPoint(344,414));SendMessageW(a.panel,WM_TIMER,4,0);
        CheckPanel(a.settings.petHeight==600,"Slider live preview");SendMessageW(a.panel,WM_KEYDOWN,VK_ESCAPE,0);
        CheckPanel(a.settings.petHeight==300&&!panelSizeDragging,"Escape restores slider");
        SendMessageW(a.panel,WM_LBUTTONDOWN,MK_LBUTTON,PanelPoint(216,414));
        SendMessageW(a.panel,WM_LBUTTONUP,0,PanelPoint(216,414));CheckPanel(a.settings.petHeight==400&&!panelSizeDragging,"Slider release commits");
        a.hover=UiSize;SendMessageW(a.panel,WM_KEYDOWN,VK_LEFT,0);CheckPanel(a.settings.petHeight==390,"Slider keyboard decrement");
        SendMessageW(a.panel,WM_KEYDOWN,VK_HOME,0);CheckPanel(a.settings.petHeight==100,"Slider lower bound");
        SendMessageW(a.panel,WM_KEYDOWN,VK_END,0);CheckPanel(a.settings.petHeight==600,"Slider upper bound");
        SendMessageW(a.panel,WM_LBUTTONDOWN,MK_LBUTTON,PanelPoint(80,414));SendMessageW(a.panel,WM_CANCELMODE,0,0);
        CheckPanel(a.settings.petHeight==600&&!panelSizeDragging,"Capture cancel restores slider");
        report<<"PASS: inline slider throttled preview, release commit, Escape/capture cancellation, keyboard steps and bounds\n";
        a.trayAdded=false;SetPanelPage(3);CheckPanel(HitRect(CmdTrayRetry).Width>0,"Tray recovery missing");PanelPng(L"tray-recovery.png");
        a.trayAdded=true;BuildPanelHits();CheckPanel(HitRect(CmdTrayRetry).Width==0,"Tray recovery shown when registered");
        CheckPanel(GetFileAttributesW((ExeDir()+L"\\MusicPet.ini").c_str())==INVALID_FILE_ATTRIBUTES,"Test saved user settings");
        report<<"PASS: conditional tray recovery and no user configuration writes\n";
    }catch(const std::exception& error){report<<"FAIL: "<<error.what()<<"\n";result=1;}
    a.motion.Stop();a.image.Reset();a.motionImage.Reset();a.notes.Shutdown();
    if(a.panel)DestroyWindow(a.panel);if(a.pet)DestroyWindow(a.pet);if(a.fx)DestroyWindow(a.fx);
    GdiplusShutdown(token);CoUninitialize();return result;
}
