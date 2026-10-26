import ctypes as c,subprocess,time,json,os
from ctypes import wintypes as w
from pathlib import Path
from PIL import ImageGrab
r=Path(os.environ['MUSICPET_TEST_ROOT']);u=c.WinDLL('user32',use_last_error=True)
u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=c.c_ssize_t
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.GetWindowRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
def wait(fn,secs=8):
 end=time.monotonic()+secs
 while time.monotonic()<end:
  v=fn()
  if v:return v
  time.sleep(.05)
 raise AssertionError('Timed out')
def dims(h):
 rect=w.RECT();u.GetWindowRect(h,c.byref(rect));return [rect.right-rect.left,rect.bottom-rect.top]
(r/'MusicPet.ini').write_text('[pet]\nHeight=300\n',encoding='utf-16')
report={};p=subprocess.Popen([str(r/'MusicPet.exe'),'--panel'])
try:
 pet=wait(lambda:u.FindWindowW('MusicPet.Native.Pet',None));owner=w.DWORD();u.GetWindowThreadProcessId(pet,c.byref(owner));assert owner.value==p.pid,'Another MusicPet is running'
 time.sleep(1);original=dims(pet);report['default_fish_window']=original
 def dialog():
  u.PostMessageW(pet,0x111,259,0);return wait(lambda:u.FindWindowW('#32770','调整桌宠大小'))
 d=dialog();track=u.GetDlgItem(d,1101);assert track
 assert u.SendMessageW(track,0x400,0,0)==300
 u.SendMessageW(track,0x400+5,1,420);wait(lambda:dims(pet)!=original)
 report['slider_live_preview']=True
 rect=w.RECT();u.GetWindowRect(d,c.byref(rect));ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).save(r/'qa'/'size-dialog.png')
 u.PostMessageW(d,0x111,2,0);wait(lambda:not u.IsWindow(d));assert dims(pet)==original;report['cancel_restores_original']=True
 d=dialog();track=u.GetDlgItem(d,1101);u.SendMessageW(track,0x400+5,1,380);time.sleep(.25);u.PostMessageW(d,0x111,1,0);wait(lambda:not u.IsWindow(d));assert 'Height=380' in (r/'MusicPet.ini').read_text(encoding='utf-16');report['confirm_persists']=True
 d=dialog();u.PostMessageW(d,0x111,1103,0);wait(lambda:dims(pet)==original);u.PostMessageW(d,0x111,1,0);wait(lambda:not u.IsWindow(d));report['reset_default']=True
 u.PostMessageW(pet,0x10,0,0);p.wait(timeout=8);assert p.returncode==0;report['clean_exit']=True
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=5)
 (r/'qa'/'size-ui-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
