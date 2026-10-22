import ctypes as c,time
from ctypes import wintypes as w
u=c.WinDLL('user32',use_last_error=True);k=c.WinDLL('kernel32',use_last_error=True)
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)];u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)];k.CloseHandle.argtypes=[w.HANDLE]
h=u.FindWindowW('MusicPet.Native.Pet',None)
if h:
    pid=w.DWORD();u.GetWindowThreadProcessId(h,c.byref(pid));p=k.OpenProcess(0x1000,False,pid);path=c.create_unicode_buffer(32768);size=w.DWORD(32768)
    assert k.QueryFullProcessImageNameW(p,0,path,c.byref(size));k.CloseHandle(p)
    assert path.value.replace(chr(92),chr(47)).lower()=='E:/Claude works/MusicPet/MusicPet.exe'.lower(),path.value
    print('Closing workspace copy',pid.value,flush=True);assert u.PostMessageW(h,0x10,0,0)
    end=time.monotonic()+8
    while time.monotonic()<end and u.FindWindowW('MusicPet.Native.Pet',None):time.sleep(.1)
    assert not u.FindWindowW('MusicPet.Native.Pet',None),'Workspace copy did not close'
else:print('No workspace copy window')
