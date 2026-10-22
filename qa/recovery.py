import os
import ctypes as c, subprocess, time, json, shutil, uuid
from ctypes import wintypes as w
from pathlib import Path
root=Path(os.environ.get('MUSICPET_TEST_ROOT',str(Path(__file__).resolve().parents[1])))
run=root/'qa'/'ui-run';run.mkdir(exist_ok=True)
shutil.copy2(root/'MusicPet.exe',run/'MusicPet.exe');(run/'skins').mkdir(exist_ok=True)
shutil.copy2(root/'skins'/'Ellie.png',run/'skins'/'Ellie.png')
u=c.WinDLL('user32',use_last_error=True);s=c.WinDLL('shell32',use_last_error=True)
u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetWindowLongPtrW.argtypes=[w.HWND,c.c_int];u.GetWindowLongPtrW.restype=c.c_ssize_t
u.GetPropW.argtypes=[w.HWND,w.LPCWSTR];u.GetPropW.restype=c.c_void_p
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.RegisterHotKey.argtypes=[w.HWND,c.c_int,w.UINT,w.UINT];u.UnregisterHotKey.argtypes=[w.HWND,c.c_int]
u.IsWindowVisible.argtypes=[w.HWND];u.RegisterWindowMessageW.argtypes=[w.LPCWSTR]
class GUID(c.Structure):_fields_=[('bytes',c.c_ubyte*16)]
guid=GUID((c.c_ubyte*16).from_buffer_copy(uuid.UUID('d43878c2-6d9a-471f-a3a1-7967d06e28b4').bytes_le))
class Identifier(c.Structure):_fields_=[('cbSize',w.DWORD),('hWnd',w.HWND),('uID',w.UINT),('guidItem',GUID)]
class Notify(c.Structure):_fields_=[('cbSize',w.DWORD),('hWnd',w.HWND),('uID',w.UINT),('uFlags',w.UINT),('uCallbackMessage',w.UINT),('hIcon',w.HICON),('szTip',w.WCHAR*128),('dwState',w.DWORD),('dwStateMask',w.DWORD),('szInfo',w.WCHAR*256),('uVersion',w.UINT),('szInfoTitle',w.WCHAR*64),('dwInfoFlags',w.DWORD),('guidItem',GUID),('hBalloonIcon',w.HICON)]
s.Shell_NotifyIconGetRect.argtypes=[c.POINTER(Identifier),c.POINTER(w.RECT)]
s.Shell_NotifyIconW.argtypes=[w.DWORD,c.POINTER(Notify)]
report={};proc=None;held=[]
keys=[(3,ord('P')),(7,ord('P')),(3,0x79),(6,0x7B)]
def wait(predicate,seconds=10):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        if predicate():return True
        time.sleep(.08)
    return False
def through(pet):return bool(u.GetWindowLongPtrW(pet,-20)&0x20)
def tray(pet):
    ident=Identifier(c.sizeof(Identifier),pet,1,guid if u.GetPropW(pet,"MusicPet.TrayUsesGuid") else GUID());rect=w.RECT()
    return s.Shell_NotifyIconGetRect(c.byref(ident),c.byref(rect))==0
def delete_tray(pet):
    n=Notify();n.cbSize=c.sizeof(n);n.hWnd=pet;n.uID=1;n.uFlags=0x20 if u.GetPropW(pet,"MusicPet.TrayUsesGuid") else 0;n.guidItem=guid
    assert s.Shell_NotifyIconW(2,c.byref(n))
def reserve(index):
    mods,key=keys[index]
    if u.RegisterHotKey(None,1000+index,mods|0x4000,key):held.append(1000+index)
    else:assert c.get_last_error()==1409,'Unexpected registration error'
    print('reserve',index,'owned',1000+index in held,flush=True)
def start(preset=0):
    global proc
    (run/'MusicPet.ini').write_text('[pet]\nClickThrough=1\nHotkeyPreset='+str(preset)+'\nHeight=240\n',encoding='utf-8')
    proc=subprocess.Popen([str(run/'MusicPet.exe'),'--panel','--quit-after=45000'])
    pet=panel=None
    def ready():
        nonlocal pet,panel
        pet=u.FindWindowW('MusicPet.Native.Pet',None);panel=u.FindWindowW('MusicPet.Native.Panel',None)
        return pet and panel and u.IsWindowVisible(panel)
    assert wait(ready),'App startup failed'
    owner=w.DWORD();u.GetWindowThreadProcessId(pet,c.byref(owner))
    print('root',root,'run',run,'launched',proc.pid,'windowOwner',owner.value,'exit',proc.poll(),flush=True)
    assert owner.value==proc.pid,'Another MusicPet is already running; refusing to interact with it'
    return pet,panel
def close():
    global proc
    if proc and proc.poll() is None:
        pet=u.FindWindowW('MusicPet.Native.Pet',None)
        if pet:u.PostMessageW(pet,0x10,0,0)
        try:proc.wait(timeout=8)
        except subprocess.TimeoutExpired:proc.terminate();proc.wait()
    assert proc.returncode==0
try:
    reserve(0)
    pet,panel=start();index=(u.GetPropW(pet,'MusicPet.ActiveShortcut') or 0)-1
    assert index>0,'No fallback selected'
    report['occupied_default_falls_back']=index
    registered=wait(lambda:tray(pet),2)
    report['tray_registered']=registered
    mods,key=keys[index];vks=[]
    if mods&2:vks.append(0x11)
    if mods&1:vks.append(0x12)
    if mods&4:vks.append(0x10)
    before=through(pet)
    for vk in vks:u.keybd_event(vk,0,0,0)
    u.keybd_event(key,0,0,0);u.keybd_event(key,0,2,0)
    for vk in reversed(vks):u.keybd_event(vk,0,2,0)
    assert wait(lambda:through(pet)!=before,3),'Actual fallback keypress failed'
    report['real_fallback_keypress']=True
    if registered:
        delete_tray(pet);assert wait(lambda:tray(pet),8),'Tray periodic retry failed'
        report['tray_missing_entry_retried']=True
        delete_tray(pet);u.PostMessageW(pet,u.RegisterWindowMessageW('TaskbarCreated'),0,0)
        assert wait(lambda:tray(pet),3),'Explorer restart recovery failed'
        report['taskbar_created_recovery']=True
        u.PostMessageW(panel,0x100,0x1B,0);time.sleep(.2)
        u.PostMessageW(pet,0x8002,0,(1<<16)|0x7B) # Version-4 WM_CONTEXTMENU callback
        assert wait(lambda:u.IsWindowVisible(panel),3),'Tray callback failed'
        report['tray_opens_panel']=True
    else:
        report['tray_tests']='BLOCKED: Shell_NotifyIcon denied, see diagnostics'
    close()
    for i in range(1,4):reserve(i)
    pet,panel=start()
    print('all-conflict shortcut',u.GetPropW(pet,'MusicPet.ActiveShortcut'),'held',held,flush=True)
    assert not u.GetPropW(pet,'MusicPet.ActiveShortcut'),'All-conflict state incorrect'
    report['safe_start_when_tray_and_hotkeys_unavailable']=not through(pet)
    if not through(pet):
        u.GetDpiForWindow.argtypes=[w.HWND];u.GetDpiForWindow.restype=w.UINT
        u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
        scale=u.GetDpiForWindow(panel)/96;lp=int(302*scale)|(int(385*scale)<<16)
        u.SendMessageW(panel,0x201,1,lp);u.SendMessageW(panel,0x202,0,lp)
    assert through(pet),'Panel click-through toggle failed' 
    u.PostMessageW(panel,0x100,0x1B,0);time.sleep(.2)
    again=subprocess.Popen([str(run/'MusicPet.exe')]);again.wait(timeout=6)
    assert wait(lambda:not through(pet) and u.IsWindowVisible(panel),3),'Second launch did not recover'
    report['all_shortcuts_occupied_relaunch_recovers']=True
    close()
    pet,panel=start(4)
    assert not u.GetPropW(pet,'MusicPet.ActiveShortcut'),'Disabled hotkey still registered'
    subprocess.run([str(run/'MusicPet.exe'),'--recover'],check=True,timeout=6)
    assert wait(lambda:not through(pet),3),'Explicit recover flag failed'
    report['disabled_shortcut_explicit_recovery']=True
    close()
finally:
    for ident in held:u.UnregisterHotKey(None,ident)
    if proc and proc.poll() is None:proc.terminate();proc.wait(timeout=5)
    (root/'qa'/'recovery-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
