from pathlib import Path
r=Path(__file__).resolve().parents[1]
# Keep the application logic identical; only isolate its window/mutex namespace
# so the developer's running instance is never touched by the test.
source=(r/'src/main.cpp').read_text(encoding='utf-8-sig')
source=source.replace('MusicPet.Native.','MusicPet.QAPlayback.')
source=source.replace('0xd43878c2','0x7a37bd91')
(r/'qa/main-playback-ui.cpp').write_text(source,encoding='utf-8')
test=(r/'qa/pair-ui-test.py').read_text(encoding='utf-8-sig')
test=test.replace('MusicPet.Native.','MusicPet.QAPlayback.')
(r/'qa/playback-ui-test.py').write_text(test,encoding='utf-8')
