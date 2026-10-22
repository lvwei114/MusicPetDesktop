import os
import ctypes as c, time, subprocess, json
from pathlib import Path
from ctypes import wintypes as w
from PIL import ImageGrab
root=Path(os.environ.get('MUSICPET_TEST_ROOT',str(Path(__file__).resolve().parents[1])))
u=c.WinDLL('user32',use_last_error=True)
u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetWindowLongPtrW.argtypes=[w.HWND,c.c_int];u.GetWindowLongPtrW.restype=c.c_ssize_t
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=c.c_ssize_t
u.GetWindowRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
u.GetDpiForWindow.argtypes=[w.HWND];u.GetDpiForWindow.restype=w.UINT
process=subprocess.Popen([str(root/'MusicPet.exe'),'--preview','--panel','--quit-after=14000'])
report={}
try:
    pet=panel=fx=None
    deadline=time.monotonic()+18
    while time.monotonic()<deadline:
        pet=u.FindWindowW('MusicPet.Native.Pet',None); panel=u.FindWindowW('MusicPet.Native.Panel',None); fx=u.FindWindowW('MusicPet.Native.Notes',None)
        if pet and panel and fx and u.IsWindowVisible(panel):break
        time.sleep(.1)
    assert pet and panel and fx and u.IsWindowVisible(panel),'App did not finish opening the visible panel'
    owner=w.DWORD();u.GetWindowThreadProcessId(pet,c.byref(owner))
    assert owner.value==process.pid,'Another MusicPet is running; refusing to interact with it'
    report['window_creation']=True
    time.sleep(1.3)
    rect=w.RECT();u.GetWindowRect(panel,c.byref(rect))
    ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).save(root/'qa'/'actual-panel.png')
    report['panel_size']=[rect.right-rect.left,rect.bottom-rect.top]
    assert u.GetWindowLongPtrW(fx,-20)&0x20,'Effects intercept input'
    assert u.GetWindowLongPtrW(pet,-20)&0x08000000,'Pet may activate on click'
    report['effects_always_clickthrough']=True
    def click(x,y):
        scale=u.GetDpiForWindow(panel)/96
        lp=int(x*scale)|(int(y*scale)<<16)
        u.SendMessageW(panel,0x201,1,lp);u.SendMessageW(panel,0x202,0,lp)
    before=bool(u.GetWindowLongPtrW(pet,-20)&0x20)
    click(302,385);time.sleep(.15)
    assert bool(u.GetWindowLongPtrW(pet,-20)&0x20)!=before,'Click-through switch did not toggle'
    u.SendMessageW(pet,0x312,1,0);time.sleep(.15)
    assert bool(u.GetWindowLongPtrW(pet,-20)&0x20)==before,'Recovery shortcut failed'
    report['clickthrough_and_recovery']=True
    click(302,332)
    time.sleep(4.5)
    assert not u.IsWindowVisible(fx),'Effects did not settle after being disabled'
    report['effects_stop_and_release_timer']=True
    u.PostMessageW(panel,0x100,0x1B,0);time.sleep(.1)
    assert not u.IsWindowVisible(panel),'Escape did not close panel'
    report['escape_dismiss']=True
    u.PostMessageW(pet,0x205,0,0);time.sleep(.2)
    assert u.IsWindowVisible(panel),'Cannot reopen panel'
    report['rightclick_reopen']=True
    u.PostMessageW(pet,0x10,0,0)
    process.wait(timeout=8)
    assert process.returncode==0,f'Unexpected exit: {process.returncode}'
    report['clean_shutdown']=True
finally:
    if process.poll() is None:process.terminate();process.wait(timeout=5)
    (root/'qa'/'smoke-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
