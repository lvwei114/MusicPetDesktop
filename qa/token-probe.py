import ctypes as c,json
from ctypes import wintypes as w
u=c.WinDLL('user32',use_last_error=True);k=c.WinDLL('kernel32',use_last_error=True);a=c.WinDLL('advapi32',use_last_error=True)
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
k.GetCurrentProcess.restype=w.HANDLE;k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
a.OpenProcessToken.argtypes=[w.HANDLE,w.DWORD,c.POINTER(w.HANDLE)]
a.GetTokenInformation.argtypes=[w.HANDLE,c.c_int,c.c_void_p,w.DWORD,c.POINTER(w.DWORD)]
a.GetSidSubAuthorityCount.argtypes=[c.c_void_p];a.GetSidSubAuthorityCount.restype=c.POINTER(c.c_ubyte)
a.GetSidSubAuthority.argtypes=[c.c_void_p,w.DWORD];a.GetSidSubAuthority.restype=c.POINTER(w.DWORD)
class Label(c.Structure):_fields_=[('sid',c.c_void_p),('attributes',w.DWORD)]
def integrity(process):
    token=w.HANDLE();assert a.OpenProcessToken(process,8,c.byref(token));size=w.DWORD();a.GetTokenInformation(token,25,None,0,c.byref(size));buf=c.create_string_buffer(size.value)
    assert a.GetTokenInformation(token,25,buf,size,c.byref(size));label=c.cast(buf,c.POINTER(Label)).contents
    level=a.GetSidSubAuthority(label.sid,a.GetSidSubAuthorityCount(label.sid)[0]-1)[0];k.CloseHandle(token);return level
pid=w.DWORD();shell=u.FindWindowW('Shell_TrayWnd',None);u.GetWindowThreadProcessId(shell,c.byref(pid));process=k.OpenProcess(0x1000,False,pid)
k.IsProcessInJob.argtypes=[w.HANDLE,w.HANDLE,c.POINTER(w.BOOL)]
k.QueryInformationJobObject.argtypes=[w.HANDLE,c.c_int,c.c_void_p,w.DWORD,c.POINTER(w.DWORD)]
job=w.BOOL();k.IsProcessInJob(k.GetCurrentProcess(),None,c.byref(job))
flags=w.DWORD();n=w.DWORD();ok=k.QueryInformationJobObject(None,4,c.byref(flags),4,c.byref(n))
print(json.dumps({'selfIL':integrity(k.GetCurrentProcess()),'shellIL':integrity(process),'shellPID':pid.value,'inJob':job.value,'jobUIQuery':ok,'jobUIFlags':flags.value}))

a.IsTokenRestricted.argtypes=[w.HANDLE]
for name,handle in [('self',k.GetCurrentProcess()),('explorer',process)]:
    token=w.HANDLE();a.OpenProcessToken(handle,8,c.byref(token));values={}
    for kind,label in [(12,'session'),(18,'restrictedSids'),(20,'elevation'),(24,'virtualization'),(26,'uiAccess'),(29,'appContainer')]:
        size=w.DWORD();a.GetTokenInformation(token,kind,None,0,c.byref(size));buf=c.create_string_buffer(max(size.value,4))
        ok=a.GetTokenInformation(token,kind,buf,len(buf),c.byref(size));values[label]=int.from_bytes(buf.raw[:4],'little') if ok else 'failed'
    buf=c.create_string_buffer(512);size=w.DWORD();a.GetTokenInformation(token,1,buf,len(buf),c.byref(size));sid=c.cast(buf,c.POINTER(c.c_void_p))[0];out=w.LPWSTR();a.ConvertSidToStringSidW.argtypes=[c.c_void_p,c.POINTER(w.LPWSTR)];a.ConvertSidToStringSidW(sid,c.byref(out));values['user']=out.value
    values['restricted']=bool(a.IsTokenRestricted(token));print(name,values);k.CloseHandle(token)

for func in ['GetCurrentPackageFullName','GetCurrentApplicationUserModelId']:
    api=getattr(k,func);api.argtypes=[c.POINTER(w.UINT),w.LPWSTR];n=w.UINT(1024);value=c.create_unicode_buffer(1024);code=api(c.byref(n),value);print(func,code,value.value)
