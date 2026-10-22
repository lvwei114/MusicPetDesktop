from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root / 'src/media.cpp').read_text(encoding='utf-8-sig')
header = r'''
#include <fstream>
#include <sstream>
static std::mutex traceMutex;
static thread_local const char* traceStage="init";
static void TraceMedia(const std::string& value) {
    std::lock_guard lock(traceMutex);
    std::ofstream f("media-switch-trace.log",std::ios::app);
    f<<GetTickCount64()<<" "<<value<<std::endl;
}
static void Stage(const char* value){traceStage=value;TraceMedia(std::string("STAGE ")+value);}
static void TraceError(int line) {
    auto hr=winrt::to_hresult();std::ostringstream s;
    s<<"ERROR stage="<<traceStage<<" line="<<line<<" hr=0x"<<std::hex<<(unsigned)hr.value;
    TraceMedia(s.str());
}
'''
src = src.replace('using namespace std::chrono_literals;', header+'\nusing namespace std::chrono_literals;')
src = src.replace('catch(...) {', 'catch(...) {TraceError(__LINE__);')
src = src.replace('void Tick() {', 'void Tick() { Stage("tick-start");')
src = src.replace('if(!manager) manager=', 'Stage("manager"); if(!manager) manager=')
src = src.replace('auto all=manager.GetSessions();', 'Stage("enumerate"); auto all=manager.GetSessions();TraceMedia("SESSION_COUNT "+std::to_string(all.Size()));')
src = src.replace('auto state=Map(s.GetPlaybackInfo().PlaybackStatus());', 'Stage("enum-playback"); auto state=Map(s.GetPlaybackInfo().PlaybackStatus());TraceMedia("SESSION "+winrt::to_string(app)+" id="+winrt::to_string(id)+" state="+std::to_string((int)state));')
src = src.replace('known=std::move(current);', 'TraceMedia("CHOSEN "+winrt::to_string(chosenId)+" wanted="+winrt::to_string(wanted)+" app="+winrt::to_string(wantedApp));known=std::move(current);')
src = src.replace('auto info=chosen.GetPlaybackInfo();', 'Stage("chosen-playback");auto info=chosen.GetPlaybackInfo();')
src = src.replace('auto properties=Await(chosen.TryGetMediaPropertiesAsync());', 'Stage("metadata-begin");auto properties=Await(chosen.TryGetMediaPropertiesAsync());Stage("metadata-end");')
src = src.replace('const auto time=chosen.GetTimelineProperties();', 'Stage("timeline");const auto time=chosen.GetTimelineProperties();')
src = src.replace('void Publish(MediaSnapshot next, std::vector<SessionEntry> list) {', 'void Publish(MediaSnapshot next, std::vector<SessionEntry> list) {TraceMedia("PUBLISH valid="+std::to_string(next.valid)+" id="+winrt::to_string(next.sessionId)+" title="+winrt::to_string(next.title)+" pos="+std::to_string(next.positionSec)+" dur="+std::to_string(next.durationSec));')
(root/'qa/media-trace-copy.cpp').write_text(src,encoding='utf-8')
