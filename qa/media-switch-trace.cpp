#include "media.h"
#include <winrt/Windows.Foundation.h>
#include <fstream>
#include <thread>
#include <chrono>
int main(){
    std::ofstream("media-switch-trace.log")<<"Read-only trace of unmodified media algorithm; monotonic timestamps in ms\n";
    MediaWatcher watcher;watcher.Start(nullptr);
    bool pinned=false;
    for(int i=0;i<1200;++i){
        auto s=watcher.Snapshot();
        if(!pinned&&s.valid&&s.status==PlayStatus::Playing){watcher.Select(s.sessionId);pinned=true;}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    watcher.Stop();
}
