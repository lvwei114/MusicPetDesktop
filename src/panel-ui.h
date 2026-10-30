// Native classic-tab panel. Included by main.cpp after the drawing helpers.
#pragma once
enum PanelAction {
    UiTabMusic=50, UiTabPet, UiTabNotes, UiTabSettings,
    UiPreset=5000, UiSpeed, UiSize, UiShortcut, UiShuffle, UiRepeat,
    UiDensityLight, UiDensityMedium, UiDensityDense, UiPaletteDefault, UiPaletteImage
};
int panelPage=0;
int panelRequestedDpi=96;
bool panelKeyboard=false, panelAutoStart=false, panelSizeDragging=false;
int panelSizeOriginal=300, panelSizePending=300;
ULONGLONG panelSizeAppliedAt=0;
std::wstring panelSkinName;
std::wstring ShortcutText();
std::wstring CurrentSkinName();
struct Hit {int id;RectF rect;};
std::vector<Hit> hits;
int PanelHeight(){constexpr int heights[]={352,480,338,550};return heights[panelPage];}
int FitPanelDpi(int requested,const RECT& work){
    const int width=std::max(1L,work.right-work.left-16),height=std::max(1L,work.bottom-work.top-16);
    return std::max(1,std::min({requested,width*96/PanelW,height*96/PanelHeight()}));
}
bool Enabled(int id){
    if(id==1)return a.snapshot.canPrev&&!a.snapshot.reconnecting;
    if(id==2)return a.snapshot.canToggle&&!a.snapshot.reconnecting;
    if(id==3)return a.snapshot.canNext&&!a.snapshot.reconnecting;
    if(id==UiShuffle)return a.snapshot.canShuffle&&!a.snapshot.reconnecting;
    if(id==UiRepeat)return a.snapshot.canRepeat&&!a.snapshot.reconnecting;
    if(id==UiSpeed||id==CmdMotionBounce||id==CmdMotionClear)return !a.settings.motionPath.empty();
    if(id==CmdLaunch||id==CmdLaunchClear)return !a.settings.launchPath.empty();
    return true;
}
void BuildPanelHits(){
    hits={{10,{292,13,26,28}},{16,{326,13,26,28}}};
    for(int i=0;i<4;++i)hits.push_back({UiTabMusic+i,{18.f+i*84,63,82,32}});
    auto add=[](int id,float x,float y,float w,float h){hits.push_back({id,{x,y,w,h}});};
    switch(panelPage){
    case 0:
        add(UiShuffle,28,225,36,36);add(1,111,225,36,36);add(2,166,223,40,40);add(3,225,225,36,36);add(UiRepeat,304,225,36,36);
        add(CmdAuto,65,297,78,32);add(11,149,297,203,32);break;
    case 1:
        add(UiPreset,120,111,232,34);
        add(CmdChoose,16,158,164,34);add(CmdSkinName,188,158,164,34);
        add(CmdMotionChoose,16,200,164,34);add(CmdPairSave,188,200,164,34);
        add(UiSpeed,16,250,336,34);add(CmdMotionBounce,16,292,336,32);add(CmdMotionClear,16,330,336,32);
        add(UiSize,16,402,336,28);add(CmdSizeSlider,247,368,105,26);break;
    case 2:
        add(CmdEffects,16,110,336,34);add(CmdPreview,16,158,336,32);
        for(int i=0;i<3;++i)add(UiDensityLight+i,16.f+i*114,219,108,30);
        for(int i=0;i<2;++i)add(UiPaletteDefault+i,16.f+i*172,280,164,30);break;
    case 3:
        add(CmdThrough,16,110,336,32);add(CmdAutoStart,16,148,336,32);add(CmdLock,16,186,336,32);add(CmdGame,16,224,336,32);
        add(UiShortcut,16,262,336,32);
        add(CmdLaunch,16,348,164,32);add(CmdLaunchChoose,188,348,164,32);
        add(CmdLaunchFind,16,387,164,32);add(CmdLaunchClear,188,387,164,32);
        add(CmdReset,16,435,336,32);
        if(!a.trayAdded)add(CmdTrayRetry,16,475,336,30);break;
    }
}
int HitAt(LPARAM value){
    BuildPanelHits();const float scale=a.panelDpi/96.f;
    PointF p(GET_X_LPARAM(value)/scale,GET_Y_LPARAM(value)/scale);
    for(const auto& h:hits)if(h.rect.Contains(p))return h.id;
    return 0;
}
RectF HitRect(int id){for(const auto& h:hits)if(h.id==id)return h.rect;return {};}
void PanelFrame(Graphics& g,RectF r,float radius,Color c){GraphicsPath p;RoundPath(p,r,radius);Pen pen(c,1);g.DrawPath(&pen,&p);}
void PanelGlyph(Graphics& g,int kind,float x,float y,Color c){
    Pen p(c,1.6f);p.SetStartCap(LineCapRound);p.SetEndCap(LineCapRound);p.SetLineJoin(LineJoinRound);
    if(kind==0)DrawNoteGlyph(g,x,y,18,0,c);
    else if(kind==1){g.DrawEllipse(&p,x-7,y-7,14.f,14.f);g.DrawArc(&p,x-4,y-2,8.f,6.f,0,180);SolidBrush b(c);g.FillEllipse(&b,x-3,y-3,1.5f,1.5f);g.FillEllipse(&b,x+2,y-3,1.5f,1.5f);}
    else if(kind==2){g.DrawLine(&p,x-6,y,x+6,y);g.DrawLine(&p,x,y-6,x,y+6);g.DrawLine(&p,x-4,y-4,x+4,y+4);g.DrawLine(&p,x-4,y+4,x+4,y-4);}
    else if(kind==3){g.DrawEllipse(&p,x-3,y-3,6.f,6.f);for(int i=0;i<8;++i){float t=i*3.14159265f/4;g.DrawLine(&p,x+cosf(t)*6,y+sinf(t)*6,x+cosf(t)*8,y+sinf(t)*8);}}
    else if(kind==4){g.DrawLine(&p,x-6,y,x+6,y);}
    else if(kind==5){g.DrawArc(&p,x-7,y-7,14.f,14.f,-55,290);g.DrawLine(&p,x,y-9,x,y);}
    else if(kind==6){PointF l[]={{x-8,y-5},{x-4,y-5},{x+4,y+5},{x+8,y+5}};g.DrawLines(&p,l,4);PointF r[]={{x-8,y+5},{x-4,y+5},{x+4,y-5},{x+8,y-5}};g.DrawLines(&p,r,4);g.DrawLine(&p,x+5,y-8,x+8,y-5);g.DrawLine(&p,x+8,y-5,x+5,y-2);g.DrawLine(&p,x+5,y+2,x+8,y+5);g.DrawLine(&p,x+8,y+5,x+5,y+8);}
    else if(kind==7){g.DrawLine(&p,x-6,y-5,x+7,y-5);g.DrawLine(&p,x+7,y-5,x+4,y-8);g.DrawLine(&p,x+7,y-5,x+4,y-2);g.DrawLine(&p,x+7,y-5,x+7,y);g.DrawLine(&p,x+6,y+5,x-7,y+5);g.DrawLine(&p,x-7,y+5,x-4,y+8);g.DrawLine(&p,x-7,y+5,x-4,y+2);g.DrawLine(&p,x-7,y+5,x-7,y);}
}
void Toggle(Graphics& g,float x,float y,bool on,bool enabled=true){
    FillRound(g,{x,y,28,17},8.5f,enabled&&on?teal:Color(255,226,227,232));
    SolidBrush b(Color(255,255,255,255));g.FillEllipse(&b,x+(on?14.f:3.f),y+3,11.f,11.f);
}
void PanelHover(Graphics& g,int id){
    if(a.hover==id&&Enabled(id)){auto r=HitRect(id);FillRound(g,r,6,Color(255,235,240,248));if(panelKeyboard)PanelFrame(g,r,6,teal);}
}
void PanelButton(Graphics& g,int id,const std::wstring& text,bool active=false){
    const auto r=HitRect(id);const bool enabled=Enabled(id);
    FillRound(g,r,6,active?Color(255,234,242,252):Color(255,255,255,255));
    PanelFrame(g,r,6,active?teal:line);PanelHover(g,id);
    Text(g,text,{r.X+7,r.Y,r.Width-14,r.Height},12,enabled?(active?teal:ink):Color(255,160,163,172),active,StringAlignmentCenter);
}
void PanelRow(Graphics& g,int id,const std::wstring& label,const std::wstring& value=L"",bool arrow=false){
    auto r=HitRect(id);PanelHover(g,id);const auto c=Enabled(id)?ink:Color(255,160,163,172);
    Text(g,label,{r.X+2,r.Y,170,r.Height},12,c);
    if(!value.empty())Text(g,value,{r.X+159,r.Y,arrow?r.Width-181:r.Width-164,r.Height},12,Enabled(id)?muted:Color(255,160,163,172),false,StringAlignmentFar);
    if(arrow)Text(g,L"⌄",{r.GetRight()-19,r.Y,17,r.Height},14,muted,false,StringAlignmentCenter);
}
void PanelSwitch(Graphics& g,int id,const std::wstring& label,bool on){PanelRow(g,id,label);auto r=HitRect(id);Toggle(g,r.GetRight()-29,r.Y+(r.Height-17)/2,on,Enabled(id));}
void PaintPanel(Graphics& g){
    BuildPanelHits();g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    SolidBrush bg(paper);g.FillRectangle(&bg,0,0,PanelW,PanelHeight());
    PanelGlyph(g,0,28,28,ink);Text(g,L"MusicPet",{47,13,190,28},13,ink,true);
    PanelHover(g,10);PanelHover(g,16);PanelGlyph(g,4,305,27,muted);PanelGlyph(g,5,339,27,muted);
    Pen sep(line,1);g.DrawLine(&sep,16.f,50.f,352.f,50.f);
    FillRound(g,{16,61,336,36},7,Color(255,244,244,246));
    const wchar_t* tabs[]={L"听歌",L"桌宠",L"音符",L"设置"};
    for(int i=0;i<4;++i){const auto r=HitRect(UiTabMusic+i);
        if(i==panelPage)FillRound(g,r,6,Color(255,255,255,255));PanelHover(g,UiTabMusic+i);
        PanelGlyph(g,i,r.X+20,r.Y+16,i==panelPage?ink:muted);
        Text(g,tabs[i],{r.X+34,r.Y,43,r.Height},12,i==panelPage?ink:muted,i==panelPage);
    }
    if(panelPage==0){
        FillRound(g,{16,111,336,174},9,Color(255,245,245,248));PanelFrame(g,{16,111,336,174},9,line);
        Text(g,a.snapshot.valid&&!a.snapshot.title.empty()?a.snapshot.title:L"等待音乐播放",{29,122,310,28},16,ink,true);
        const auto sub=a.snapshot.reconnecting?StatusText():(a.snapshot.artist.empty()?StatusText():a.snapshot.artist);
        Text(g,sub,{29,150,310,21},12,muted);
        FillRound(g,{29,182,310,3},1.5f,Color(255,223,225,232));
        if(!a.snapshot.reconnecting&&a.snapshot.durationSec>0){const auto w=(float)std::clamp(a.snapshot.positionSec/a.snapshot.durationSec,0.,1.)*310;if(w>0)FillRound(g,{29,182,w,3},1.5f,teal);
            Text(g,Time(a.snapshot.positionSec),{29,190,90,17},11,muted);Text(g,Time(a.snapshot.durationSec),{251,190,88,17},11,muted,false,StringAlignmentFar);
        }else Text(g,a.snapshot.reconnecting?L"等待新曲目进度…":L"进度未知",{29,190,310,17},11,muted);
        const int mediaIds[]={UiShuffle,1,2,3,UiRepeat};for(int id:mediaIds){
            auto r=HitRect(id);PanelHover(g,id);const auto enabled=Enabled(id);const auto c=enabled?muted:Color(255,185,189,196);
            if(id==2){SolidBrush b(enabled?teal:Color(255,209,214,224));g.FillEllipse(&b,r);DrawIcon(g,2,r.X+20,r.Y+20,Color(255,255,255,255));}
            else if(id==UiShuffle)PanelGlyph(g,6,r.X+18,r.Y+18,enabled&&a.snapshot.shuffle==1?teal:c);
            else if(id==UiRepeat){PanelGlyph(g,7,r.X+18,r.Y+18,enabled&&a.snapshot.repeat>0?teal:c);if(a.snapshot.repeat==1)Text(g,L"1",{r.X+11,r.Y+9,14,17},10,teal,true,StringAlignmentCenter);}
            else DrawIcon(g,id,r.X+18,r.Y+18,c);
        }
        Text(g,L"播放源",{16,297,48,32},11,muted);
        const bool automatic=a.media.SelectedId().empty();auto r=HitRect(CmdAuto);PanelHover(g,CmdAuto);
        FillRound(g,{r.X+3,r.Y+10,12,12},3,automatic?teal:Color(255,235,235,240));if(automatic)Text(g,L"✓",{r.X+3,r.Y+8,12,15},10,Color(255,255,255,255),true,StringAlignmentCenter);
        Text(g,L"自动",{r.X+21,r.Y,50,r.Height},11,ink);
        PanelButton(g,11,(a.snapshot.valid?a.snapshot.appName:L"选择播放器")+L"  ⌄");
    }else if(panelPage==1){
        Text(g,L"当前预设",{16,111,100,34},12,ink);PanelButton(g,UiPreset,(panelSkinName.empty()?L"选择预设":panelSkinName)+L"  ⌄");
        PanelButton(g,CmdChoose,L"选择图片…");PanelButton(g,CmdSkinName,L"重命名…");PanelButton(g,CmdMotionChoose,L"添加动态…");PanelButton(g,CmdPairSave,L"另存组合…");
        int speed=3;for(int i=0;i<7;++i)if(motionSpeeds[i]==a.settings.motionSpeed)speed=i;
        PanelRow(g,UiSpeed,L"动画速度",motionSpeedLabels[speed],true);
        PanelSwitch(g,CmdMotionBounce,L"往返播放",a.settings.motionPingPong);
        PanelButton(g,CmdMotionClear,L"关闭当前动态，恢复静态图");
        Text(g,L"桌宠大小",{18,368,145,26},12,ink);PanelButton(g,CmdSizeSlider,std::to_wstring(a.settings.petHeight)+L" px  ›");
        auto r=HitRect(UiSize);PanelHover(g,UiSize);FillRound(g,{24,r.Y+12,320,3},1.5f,Color(255,221,225,232));
        const float x=24+(std::clamp(a.settings.petHeight,100,600)-100)/500.f*320;FillRound(g,{24,r.Y+12,std::max(1.f,x-24),3},1.5f,teal);SolidBrush knob(teal);g.FillEllipse(&knob,x-6,r.Y+7,12.f,12.f);
        Text(g,L"暂停音乐后，播完当前循环再恢复静态图",{18,436,332,20},11,muted);
    }else if(panelPage==2){
        PanelSwitch(g,CmdEffects,L"音符效果",a.settings.effectsOn!=0);PanelButton(g,CmdPreview,L"预览音符 · 8 秒");
        Text(g,L"音符数量",{18,194,300,21},11,muted);const wchar_t* density[]={L"轻盈",L"适中",L"热闹"};for(int i=0;i<3;++i)PanelButton(g,UiDensityLight+i,density[i],a.settings.density==i+1);
        Text(g,L"音符配色",{18,255,300,21},11,muted);PanelButton(g,UiPaletteDefault,L"默认配色",a.settings.notePalette==0);PanelButton(g,UiPaletteImage,L"跟随图片",a.settings.notePalette==1);
    }else{
        PanelSwitch(g,CmdThrough,L"点击穿透",a.settings.clickThrough);PanelSwitch(g,CmdAutoStart,L"开机自启",panelAutoStart);
        PanelSwitch(g,CmdLock,L"锁定桌宠位置",a.settings.locked);PanelSwitch(g,CmdGame,L"游戏模式",a.settings.gameMode);
        const auto preferred=a.settings.hotkeyPreset<4?shortcuts[a.settings.hotkeyPreset].label:L"关闭";
        PanelRow(g,UiShortcut,L"穿透快捷键",preferred,true);
        Text(g,L"当前："+ShortcutText(),{18,297,332,22},11,muted);
        const auto app=a.settings.launchPath.empty()?L"未配置":std::filesystem::path(a.settings.launchPath).stem().wstring();
        Text(g,L"启动软件",{18,321,100,21},11,muted);Text(g,app,{120,321,230,21},11,muted,false,StringAlignmentFar);
        PanelButton(g,CmdLaunch,L"打开指定软件");PanelButton(g,CmdLaunchChoose,L"手动配置…");PanelButton(g,CmdLaunchFind,L"自动查找…");PanelButton(g,CmdLaunchClear,L"清除配置");
        PanelButton(g,CmdReset,L"找回桌宠 / 重置位置");
        if(!a.trayAdded)PanelButton(g,CmdTrayRetry,L"重试托盘注册");else Text(g,L"再次启动程序，也可关闭穿透并打开菜单",{18,477,332,21},11,muted);
    }
    const float bottom=(float)PanelHeight()-25;
    if(!a.notice.empty()&&GetTickCount64()<a.noticeUntil)Text(g,a.notice,{18,bottom,332,20},11,teal);
    else if(panelPage==0)Text(g,StatusText(),{18,bottom,332,20},11,muted);
}
