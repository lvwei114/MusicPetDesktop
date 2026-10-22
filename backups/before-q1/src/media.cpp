#include "media.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <cwctype>
using namespace std::chrono_literals;
namespace wmc = winrt::Windows::Media::Control;
using Session = wmc::GlobalSystemMediaTransportControlsSession;
using Manager = wmc::GlobalSystemMediaTransportControlsSessionManager;
using Status = wmc::GlobalSystemMediaTransportControlsSessionPlaybackStatus;
// A disconnected player must not hold the worker or process shutdown indefinitely.
template<class Operation> auto Await(Operation op) {
    if (op.wait_for(1200ms) != winrt::Windows::Foundation::AsyncStatus::Completed) {
        op.Cancel(); throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
    }
    return op.GetResults();
}
static std::wstring Name(std::wstring id) {
    auto lower=id; for(auto& c:lower) c=(wchar_t)towlower(c);
    const std::pair<const wchar_t*,const wchar_t*> names[] = {{L"cloudmusic",L"网易云音乐"},{L"qqmusic",L"QQ 音乐"},{L"kugou",L"酷狗音乐"},{L"kwmusic",L"酷我音乐"},{L"musicfree",L"MusicFree"},{L"spotify",L"Spotify"},{L"msedge",L"Microsoft Edge"},{L"chrome",L"Google Chrome"},{L"applemusic",L"Apple Music"},{L"potplayer",L"PotPlayer"},{L"firefox",L"Firefox"}};
    for(auto [key,value]:names) if(lower.find(key)!=std::wstring::npos) return value;
    return id.empty()?L"媒体播放器":id;
}
static PlayStatus Map(Status s) {
    if(s==Status::Playing) return PlayStatus::Playing;
    if(s==Status::Paused) return PlayStatus::Paused;
    if(s==Status::Stopped || s==Status::Closed) return PlayStatus::Stopped;
    return PlayStatus::Unknown;
}
struct MediaWatcher::Impl {
    HWND notify{}; HANDLE wake{}; std::thread thread; std::atomic<bool> stop{false};
    mutable std::mutex mutex;
    MediaSnapshot snapshot;
    std::vector<SessionEntry> entries;
    std::wstring selected;
    struct Command { MediaCommand kind; std::wstring target; };
    std::deque<Command> commands;
    struct Item { Session object{nullptr}; std::wstring id,appId; };
    std::vector<Item> known;
    unsigned serial{};
    Manager manager{nullptr};
    std::wstring lastId,title,artist;
    ULONGLONG lastProperties{};
    void Publish(MediaSnapshot next, std::vector<SessionEntry> list) {
        { std::lock_guard lock(mutex); next.revision=snapshot.revision+1; snapshot=std::move(next); entries=std::move(list); }
        if(notify && !stop) PostMessageW(notify,WM_APP_MEDIA_UPDATE,0,0);
    }
    void Tick() {
        if(!manager) manager=Await(Manager::RequestAsync());
        std::wstring wanted;
        { std::lock_guard lock(mutex); wanted=selected; }
        std::vector<Item> current;
        std::vector<SessionEntry> list;
        Session chosen{nullptr},playing{nullptr},system{nullptr};
        std::wstring chosenId;
        try { system=manager.GetCurrentSession(); } catch(...) {}
        auto all=manager.GetSessions();
        for(auto s:all) {
            try {
                const std::wstring app=s.SourceAppUserModelId().c_str();
                auto previous=std::find_if(known.begin(),known.end(),[&](const Item& i){return i.object==s;});
                const auto id=previous!=known.end()?previous->id:app+L"#"+std::to_wstring(++serial);
                auto state=Map(s.GetPlaybackInfo().PlaybackStatus());
                const bool active=state==PlayStatus::Playing;
                auto propsName=Name(app);
                // Distinguish multiple sessions from one browser/application.
                list.push_back({id,propsName,active}); current.push_back({s,id,app});
                if(active && !playing) playing=s;
                if(!wanted.empty() && wanted==id) { chosen=s; chosenId=id; }
            } catch(...) {}
        }
        if(wanted.empty()) {
            chosen=playing?playing:system;
            if(!chosen && !current.empty()) chosen=current.front().object;
            for(const auto& item:current) if(item.object==chosen) {chosenId=item.id;break;}
        }
        // If a manually selected session disappears, never silently control another app.
        known=std::move(current);
        MediaSnapshot next;
        std::deque<Command> pending;
        { std::lock_guard lock(mutex); pending.swap(commands); }
        for(const auto& command:pending) {
            if(stop) break;
            if(!chosen || command.target!=chosenId) continue;
            bool ok=false;
            try {
                if(command.kind==CMD_TOGGLE) ok=Await(chosen.TryTogglePlayPauseAsync());
                if(command.kind==CMD_NEXT) ok=Await(chosen.TrySkipNextAsync());
                if(command.kind==CMD_PREV) ok=Await(chosen.TrySkipPreviousAsync());
            } catch(...) {}
            if(!ok && !stop) PostMessageW(notify,WM_APP+3,0,0);
        }
        if(stop) return;
        if(chosen) {
            next.valid=true; next.sessionId=chosenId; next.appId=chosen.SourceAppUserModelId().c_str(); next.appName=Name(next.appId);
            auto info=chosen.GetPlaybackInfo(); next.status=Map(info.PlaybackStatus()); auto controls=info.Controls();
            next.canToggle=controls.IsPlayPauseToggleEnabled(); next.canNext=controls.IsNextEnabled(); next.canPrev=controls.IsPreviousEnabled();
            if(lastId!=chosenId) { title.clear();artist.clear();lastProperties=0;lastId=chosenId; }
            if(GetTickCount64()-lastProperties>=1800) {
                try { auto properties=Await(chosen.TryGetMediaPropertiesAsync());title=properties.Title().c_str();artist=properties.Artist().c_str(); } catch(...) {}
                lastProperties=GetTickCount64();
            }
            next.title=title;next.artist=artist;
            try {auto time=chosen.GetTimelineProperties(); const double start=time.StartTime().count()/1e7;
                next.durationSec=std::max(0.,time.EndTime().count()/1e7-start);
                next.positionSec=std::clamp(time.Position().count()/1e7-start,0.,next.durationSec);
            } catch(...) {}
        }
        Publish(std::move(next),std::move(list));
    }
    void Run() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            while(!stop) {
                try {Tick();} catch(...) { manager=nullptr;Publish({},{}); }
                if(!stop) WaitForSingleObject(wake,650);
            }
            known.clear();manager=nullptr;winrt::uninit_apartment();
        } catch(...) {Publish({},{});}
    }
};
MediaWatcher::MediaWatcher():m_impl(std::make_unique<Impl>()){}
MediaWatcher::~MediaWatcher(){Stop();}
void MediaWatcher::Start(HWND notify) {
    if(m_impl->thread.joinable()) return;
    m_impl->notify=notify;m_impl->stop=false;m_impl->wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(m_impl->wake) m_impl->thread=std::thread([this]{m_impl->Run();});
}
void MediaWatcher::Stop() {
    if(!m_impl || !m_impl->thread.joinable()) return;
    m_impl->stop=true;SetEvent(m_impl->wake);m_impl->thread.join();
    CloseHandle(m_impl->wake);m_impl->wake=nullptr;
}
MediaSnapshot MediaWatcher::Snapshot()const {std::lock_guard lock(m_impl->mutex);return m_impl->snapshot;}
std::vector<SessionEntry> MediaWatcher::Sessions()const {std::lock_guard lock(m_impl->mutex);return m_impl->entries;}
std::wstring MediaWatcher::SelectedId()const {std::lock_guard lock(m_impl->mutex);return m_impl->selected;}
void MediaWatcher::Select(const std::wstring& id) {{std::lock_guard lock(m_impl->mutex);m_impl->selected=id;}SetEvent(m_impl->wake);}
void MediaWatcher::Send(MediaCommand cmd) {
    {std::lock_guard lock(m_impl->mutex);if(m_impl->commands.size()<8)m_impl->commands.push_back({cmd,m_impl->snapshot.sessionId});}
    SetEvent(m_impl->wake);
}
