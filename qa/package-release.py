from pathlib import Path
import argparse,hashlib,json,zipfile
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Package a tested MusicPet build without personal configuration.')
parser.add_argument('--version',default='2026.10.30')
parser.add_argument('--exe',type=Path,default=root/'MusicPet-next.exe')
args=parser.parse_args()
if not args.version or any(c not in '0123456789.-' for c in args.version):
    raise SystemExit('Version must contain only digits, dots and hyphens')
out=root/'releases'
out.mkdir(exist_ok=True)
name=f'MusicPet-{args.version}-win-x64'
readme=f'''MusicPet 音乐桌宠 — {args.version}

系统：Windows 10 1809 及以上 / Windows 11，x64。
请先将整个压缩包解压到普通用户可写目录，再双击 MusicPet.exe。
无需安装 .NET 或 VC++ 运行库；不要直接在压缩包内运行。

操作
拖动图片移动；右键图片或托盘音符打开控制面板。
控制面板使用“听歌 / 桌宠 / 音符 / 设置”四页，保留原有功能。
“桌宠”页可导入图片、保存预设和自定义名称。
默认静态形象为大肥鱼；可另选播放时的 GIF / APNG / MP4，暂不支持 WebM。
静态图片与动态素材可作为组合保存预设，并自定义名称。
内置 mika 组合预设：在“桌宠 → 当前预设”中选择 mika，音乐播放时显示动画，暂停后收尾并切回静态图。
“桌宠”页中可调整动画速度（0.25～2 倍）和往返播放，并随组合预设保存。
音乐暂停后完成当前动画循环，再切回静态图；往返模式会完成一次完整往返。
组合皮肤共用完整画布和缩放规则；同尺寸且主体位置一致的素材保持相同位置。
动画使用固定画布，不会逐帧裁切透明边缘；不同画布比例按水平居中、底部对齐显示。
动态素材静音循环，普通 MP4 有矩形背景，系统不支持的编码会回退静态图。
“桌宠”页可直接拖动大小滑动条，松开保存，拖动时按 Esc 撤销。
点击尺寸数值可打开独立大小对话框，确定保存、取消恢复。
关闭当前动态不会删除组合预设，从预设库重选组合可恢复动态。
“听歌”页支持 GSMTC 随机及循环模式，未开放的能力显示为禁用。
“音符”页可选择音符数量、配色及预览；“设置”页可配置开机自启和指定软件。
Tab / Shift+Tab 选择控件，Enter / 空格操作，Ctrl+Tab / Ctrl+Shift+Tab 切页。
面板按内容调整高度，屏幕空间不足时缩小显示比例，以保持全部控件可见。
默认音符数量为“适中”，开机自启默认关闭；开启后下次登录 Windows 时启动。
开启自启后请保持程序路径不变；移动前先关闭自启，在新目录启动后重新开启。
退出不会自动关闭自启；不再使用时先从设置关闭自启，再删除文件夹。
默认 Ctrl+Alt+P 切换点击穿透；冲突时自动尝试备用快捷键。
无法操作时，再次运行程序或运行“恢复桌宠.cmd”。面板右上角电源按钮退出程序。

播放器
通过 Windows GSMTC 读取媒体会话，不录音、不读取音乐账号。
播放器必须支持系统媒体控制；部分客户端需在设置中开启 SMTC。
切歌短暂丢失会话时保留曲目信息，显示“正在切换曲目”，禁用控制并隐藏旧进度。
恢复缓冲为 1.5 秒；会话及曲目事件触发及时刷新，保留轮询兜底。
客户端未提供进度时显示未知。不能保证覆盖独占全屏或受保护游戏。

配置
首次保存时在程序目录生成 MusicPet.ini；皮肤预设在 skins 中。
本包不包含开发者个人配置、日志、源码、证书私钥或证书安装脚本。

签名
本包 EXE 尚未进行数字签名，Windows SmartScreen 可能显示安全提示。
无需也不应为使用本包而关闭系统安全保护。

图片授权
随附 大肥鱼.png 与 mika 静态、动画素材来自用户提供的图库，素材授权独立于源码 MIT 许可，需由分发者确认。
向他人分发前请确认拥有图片再分发许可；没有许可时应换为允许分发的图片，
并保持默认文件名 skins/大肥鱼.png。程序签名不代表拥有图片版权。
'''
files={p:(root/p).read_bytes() for p in ['LICENSE','启动桌宠.cmd','恢复桌宠.cmd','预览效果.cmd','skins/大肥鱼.png','skins/mika.pet.ini','skins/mika/frame_0001.png','skins/mika/mika.apng']}
files['MusicPet.exe']=args.exe.read_bytes()
files['使用说明.txt']=readme.encode('utf-8-sig')
files['SHA256SUMS.txt']=''.join(hashlib.sha256(b).hexdigest()+'  '+p+'\n' for p,b in files.items()).encode('utf-8')
archive=out/(name+'.zip')
if archive.exists():raise SystemExit('Archive already exists; refusing to overwrite')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p,b in files.items():z.writestr(name+'/'+p,b)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for p,b in files.items():assert z.read(name+'/'+p)==b
archive_hash=hashlib.sha256(archive.read_bytes()).hexdigest()
(out/f'MusicPet-{args.version}-SHA256SUMS.txt').write_text(archive_hash+'  '+archive.name+'\n',encoding='utf-8')
report={'archive':archive.name,'bytes':archive.stat().st_size,'sha256':archive_hash,'exeSha256':hashlib.sha256(files['MusicPet.exe']).hexdigest(),'files':list(files),'zipVerified':True}
(out/(name+'-manifest.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
