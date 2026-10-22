import os
import ctypes as c,subprocess,time,json
from pathlib import Path
from ctypes import wintypes as w
from PIL import ImageGrab
root=Path(os.environ.get('MUSICPET_TEST_ROOT',str(Path(__file__).resolve().parents[1])))
u=c.WinDLL('user32',use_last_error=True);u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetPropW.argtypes=[w.HWND,w.LPCWSTR];u.GetPropW.restype=c.c_void_p
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
p=subprocess.Popen([str(root/'MusicPet.exe'),'--panel','--quit-after=15000'])
report={}
try:
    end=time.monotonic()+10;pet=None
    while time.monotonic()<end and p.poll() is None:
        pet=u.FindWindowW('MusicPet.Native.Pet',None)
        if pet:break
        time.sleep(.1)
    time.sleep(1)
    report={'pid':p.pid,'exit':p.poll(),'pet':pet,'shell':u.FindWindowW('Shell_TrayWnd',None),'trayRegistered':u.GetPropW(pet,'MusicPet.TrayRegistered') if pet else None}
    ImageGrab.grab(all_screens=True).save(root/'qa'/'startup-desktop.png')
    print(json.dumps(report))
finally:
    if p.poll() is None and pet:u.PostMessageW(pet,0x10,0,0)
    try:p.wait(timeout=8)
    except subprocess.TimeoutExpired:p.terminate();p.wait()
    report['finalExit']=p.returncode
    (root/'qa'/'startup-report.json').write_text(json.dumps(report,indent=2))
