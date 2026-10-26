import ctypes as c,subprocess,time,json,configparser,os
from pathlib import Path
from ctypes import wintypes as w
r=Path(os.environ['MUSICPET_TEST_ROOT']);u=c.WinDLL('user32',use_last_error=True)
u.FindWindowW.argtypes=[w.LPCWSTR,w.LPCWSTR];u.FindWindowW.restype=w.HWND
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=c.c_ssize_t
u.GetClientRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
def ini(path):
    for attempt in range(30):
        try:
            b=path.read_bytes();p=configparser.ConfigParser(interpolation=None);p.read_string(b.decode('utf-16' if b.startswith(b'\xff\xfe') else 'utf-8-sig'));return p
        except (PermissionError,UnicodeError,configparser.Error):
            if attempt==29:raise
            time.sleep(.03)
def wait(fn):
    end=time.monotonic()+12
    while time.monotonic()<end:
        v=fn()
        if v:return v
        time.sleep(.05)
    raise AssertionError('Timed out')
skins=[]
names=ini(r/'SkinNames.ini')['names'] if (r/'SkinNames.ini').exists() else {}
def label(file):
    return names.get('skin'+''.join(f'{ord(ch):04x}' for ch in file.name.lower()),file.stem)
for file in (r/'skins').iterdir():
    if file.suffix.lower() in ['.png','.jpg','.jpeg','.bmp']:skins.append((label(file),file,None))
    elif file.name.endswith('.pet.ini'):
        p=ini(file)['preset'];image=r/'skins'/p['image'];motion=r/'skins'/p['motion'] if p.get('motion') else None
        if '..' in Path(p['image']).parts or (p.get('motion') and '..' in Path(p['motion']).parts):continue
        if image.is_file() and (motion is None or motion.is_file()):skins.append((p.get('name',label(file)),file,p))
skins.sort(key=lambda s:s[0])
animated=next((i,s) for i,s in enumerate(skins) if s[2] is not None and s[2].get('motion'))
plain=next((i,s) for i,s in enumerate(skins) if s[2] is None)
(r/'MusicPet.ini').write_text('[pet]\nPreset='+animated[1][1].name+'\nMotion=missing.mp4\n',encoding='utf-16')
p=subprocess.Popen([str(r/'MusicPet.exe'),'--panel']);report={}
try:
    pet=wait(lambda:u.FindWindowW('MusicPet.Native.Pet',None));pid=w.DWORD();u.GetWindowThreadProcessId(pet,c.byref(pid));assert pid.value==p.pid,'Refusing to operate another instance'
    time.sleep(.8)
    u.PostMessageW(pet,0x111,240,0)
    wait(lambda:ini(r/'MusicPet.ini')['pet'].get('motion','')!='missing.mp4');saved=ini(r/'MusicPet.ini')['pet']
    assert saved['preset']==animated[1][1].name and Path(saved['motion']).is_file();report['startup_resolves_relative_pair']=True
    # Set a known state via the same commands used by the animation menu.
    u.PostMessageW(pet,0x111,281,0)
    wait(lambda:ini(r/'MusicPet.ini')['pet'].get('motionspeed')=='50')
    if ini(r/'MusicPet.ini')['pet'].get('motionpingpong','0')=='0':u.PostMessageW(pet,0x111,287,0)
    wait(lambda:ini(r/'MusicPet.ini')['pet'].get('motionpingpong')=='1')
    options=ini(animated[1][1])['preset'];assert options['speed']=='50' and options['pingpong']=='1';report['menu_commands_save_pair_options']=True
    # Build the same list as the actual appearance menu without clicking outside.
    # The menu's ScanSkins runs when it opens; use the appearance button with ESC.
    panel=wait(lambda:u.FindWindowW('MusicPet.Native.Panel',None))
    # Keyboard shortcut to appearance button is not assumed. WM_COMMAND skin
    # selection requires the menu list, populated via opening the menu below.
    rect=w.RECT();u.GetClientRect(panel,c.byref(rect));scale=rect.right/344
    u.SendMessageW(panel,0x200,0,int(120*scale)|(int(280*scale)<<16))
    u.PostMessageW(panel,0x100,0x0d,0)
    time.sleep(.5);u.PostMessageW(panel,0x1f,0,0);time.sleep(.4)
    u.PostMessageW(pet,0x111,1000+plain[0],0)
    wait(lambda:ini(r/'MusicPet.ini')['pet'].get('preset','')=='');assert ini(r/'MusicPet.ini')['pet'].get('motion','')=='';report['plain_selection_clears_animation']=True
    u.PostMessageW(pet,0x111,1000+animated[0],0)
    wait(lambda:ini(r/'MusicPet.ini')['pet'].get('preset','')==animated[1][1].name);assert Path(ini(r/'MusicPet.ini')['pet']['motion']).is_file();report['pair_selection_restores_animation']=True
    restored=ini(r/'MusicPet.ini')['pet'];assert restored['motionspeed']=='50' and restored['motionpingpong']=='1';report['pair_selection_restores_speed_bounce']=True
    u.PostMessageW(pet,0x10,0,0);p.wait(timeout=8);assert p.returncode==0;report['clean_exit']=True
finally:
    if p.poll() is None:p.terminate();p.wait(timeout=5)
    (r/'qa'/'pair-ui-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
