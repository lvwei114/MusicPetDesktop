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
#include <cmath>
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
    // Event callbacks own only this signal, never Impl. An in-flight callback can
    // safely finish after revocation/shutdown without touching freed state/handles.
    struct Signal {
        HANDLE wake{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
        std::atomic<bool> metadataDirty{true};
        ~Signal(){if(wake)CloseHandle(wake);}
    };
    HWND notify{}; std::shared_ptr<Signal> signal; std::thread thread; std::atomic<bool> stop{false};
    mutable std::mutex mutex;
    MediaSnapshot snapshot;
    std::vector<SessionEntry> entries;
    std::wstring selected,selectedApp;
    std::vector<std::pair<std::wstring,std::wstring>> sessionApps;
    struct Command { MediaCommand kind; std::wstring target; };
    std::deque<Command> commands;
    struct Item { Session object{nullptr}; std::wstring id,appId; };
    std::vector<Item> known;
    unsigned serial{};
    Manager manager{nullptr};
    Manager::SessionsChanged_revoker sessionsChanged;
    Manager::CurrentSessionChanged_revoker currentChanged;
    Session watched{nullptr};
    Session::MediaPropertiesChanged_revoker propertiesChanged;
    Session::PlaybackInfoChanged_revoker playbackChanged;
    Session::TimelinePropertiesChanged_revoker timelineChanged;
    ULONGLONG missingSince{};
    void Watch(Session session) {
        if(watched==session)return;
        propertiesChanged.revoke();playbackChanged.revoke();timelineChanged.revoke();
        watched=session;signal->metadataDirty=true;
        if(!session)return;
        const auto shared=signal;
        propertiesChanged=session.MediaPropertiesChanged(winrt::auto_revoke,[shared](auto const&,auto const&){shared->metadataDirty=true;SetEvent(shared->wake);});
        playbackChanged=session.PlaybackInfoChanged(winrt::auto_revoke,[shared](auto const&,auto const&){SetEvent(shared->wake);});
        timelineChanged=session.TimelinePropertiesChanged(winrt::auto_revoke,[shared](auto const&,auto const&){SetEvent(shared->wake);});
    }
    void Missing(std::vector<SessionEntry> list) {
        const auto now=GetTickCount64();if(!missingSince)missingSince=now;
        MediaSnapshot previous;
        {std::lock_guard lock(mutex);
            if(selected.empty()||selected==snapshot.sessionId||selectedApp==snapshot.appId)previous=snapshot;
        }
        if(previous.valid&&now-missingSince<1500){
            previous.reconnecting=true;
            previous.canToggle=previous.canNext=previous.canPrev=false;
            previous.positionSec=previous.durationSec=0;
            Publish(std::move(previous),std::move(list));
        }else Publish({},std::move(list));
        timeline.valid=false;lastProperties=0;
    }
    std::wstring lastId,title,artist;
    ULONGLONG lastProperties{};
    struct Timeline {
        std::wstring session;
        bool valid{};
        double raw{},duration{},position{},rate{1};
        int64_t updated{};
        ULONGLONG sampled{};
        PlayStatus status{PlayStatus::Unknown};
    } timeline;
    std::wstring timelineTitle,timelineArtist;
    double Estimate(ULONGLONG now) const {
        const double elapsed=timeline.status==PlayStatus::Playing?(now-timeline.sampled)/1000.:0.;
        return std::clamp(timeline.position+elapsed*timeline.rate,0.,timeline.duration);
    }
    void Publish(MediaSnapshot next, std::vector<SessionEntry> list) {
        { std::lock_guard lock(mutex); next.revision=snapshot.revision+1; snapshot=std::move(next); entries=std::move(list);
          sessionApps.clear();for(const auto& item:known)sessionApps.emplace_back(item.id,item.appId); }
        if(notify && !stop) PostMessageW(notify,WM_APP_MEDIA_UPDATE,0,0);
    }
    void Tick() {
        if(!manager) {
            manager=Await(Manager::RequestAsync());
            const auto shared=signal;
            sessionsChanged=manager.SessionsChanged(winrt::auto_revoke,[shared](auto const&,auto const&){SetEvent(shared->wake);});
            currentChanged=manager.CurrentSessionChanged(winrt::auto_revoke,[shared](auto const&,auto const&){SetEvent(shared->wake);});
        }
        std::wstring wanted,wantedApp;
        { std::lock_guard lock(mutex); wanted=selected;wantedApp=selectedApp; }
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
        // Keep the exact session while it exists. Rebind only when there is
        // exactly one candidate from the same source app; browsers can be ambiguous.
        if(!wanted.empty()&&!chosen&&!wantedApp.empty()) {
            const Item* candidate=nullptr;size_t matches=0;
            for(const auto& item:current)if(item.appId==wantedApp){candidate=&item;++matches;}
            if(matches==1){chosen=candidate->object;chosenId=candidate->id;
                std::lock_guard lock(mutex);
                if(selected==wanted&&selectedApp==wantedApp)selected=chosenId;
            }
        }
        if(wanted.empty()) {
            chosen=playing?playing:system;
            if(!chosen && !current.empty()) chosen=current.front().object;
            for(const auto& item:current) if(item.object==chosen) {chosenId=item.id;break;}
        }
        // A missing/ambiguous manual choice waits instead of falling back to another app.
        known=std::move(current);
        Watch(chosen);
        MediaSnapshot next;
        std::deque<Command> pending;
        { std::lock_guard lock(mutex); pending.swap(commands); }
        for(const auto& command:pending) {
            if(stop) break;
            // Never replay a click against a replacement or unavailable session.
            if(!chosen || command.target!=chosenId) continue;
            bool ok=false;
            try {
                if(command.kind==CMD_TOGGLE) ok=Await(chosen.TryTogglePlayPauseAsync());
                if(command.kind==CMD_NEXT) ok=Await(chosen.TrySkipNextAsync());
                if(command.kind==CMD_PREV) ok=Await(chosen.TrySkipPreviousAsync());
            } catch(...) {}
            if(ok&&(command.kind==CMD_NEXT||command.kind==CMD_PREV))lastProperties=0;
            if(!ok && !stop) PostMessageW(notify,WM_APP+3,0,0);
        }
        if(stop) return;
        if(chosen) {
            next.valid=true; next.sessionId=chosenId; next.appId=chosen.SourceAppUserModelId().c_str(); next.appName=Name(next.appId);
            auto info=chosen.GetPlaybackInfo(); next.status=Map(info.PlaybackStatus()); auto controls=info.Controls();
            next.canToggle=controls.IsPlayPauseToggleEnabled(); next.canNext=controls.IsNextEnabled(); next.canPrev=controls.IsPreviousEnabled();
            if(lastId!=chosenId) { title.clear();artist.clear();lastProperties=0;lastId=chosenId; }
            const bool dirty=signal->metadataDirty.exchange(false);
            if(dirty||!lastProperties||GetTickCount64()-lastProperties>=3000) {
                try { auto properties=Await(chosen.TryGetMediaPropertiesAsync());title=properties.Title().c_str();artist=properties.Artist().c_str(); } catch(...) {}
                lastProperties=GetTickCount64();
            }
            next.title=title;next.artist=artist;
            try {
                const auto time=chosen.GetTimelineProperties();
                const double start=time.StartTime().count()/1e7;
                const double duration=time.EndTime().count()/1e7-start;
                const double raw=time.Position().count()/1e7-start;
                const int64_t updated=time.LastUpdatedTime().time_since_epoch().count();
                const auto now=GetTickCount64();
                double rate=1.;if(auto value=info.PlaybackRate())rate=value.Value();
                if(!std::isfinite(rate)||rate<0)rate=1.;
                if(duration>0&&std::isfinite(duration)&&std::isfinite(raw)) {
                    const bool reset=!timeline.valid||timeline.session!=chosenId||
                        next.title!=timelineTitle||next.artist!=timelineArtist;
                    const bool changed=reset||updated!=timeline.updated||raw!=timeline.raw||duration!=timeline.duration;
                    // Only extrapolate a real timeline. Fresh samples reconcile seeks
                    // and drift; repeated samples keep advancing on a monotonic clock.
                    double position=changed?raw:Estimate(now);
                    if(changed&&next.status==PlayStatus::Playing&&updated>0) {
                        const auto utc=winrt::clock::now().time_since_epoch().count();
                        const double age=(utc-updated)/1e7;
                        if(age>=0)position+=age*rate;
                    }
                    timeline={chosenId,true,raw,duration,std::clamp(position,0.,duration),rate,updated,now,next.status};
                    timelineTitle=next.title;timelineArtist=next.artist;
                    next.durationSec=duration;next.positionSec=timeline.position;
                } else timeline.valid=false;
            } catch(...) {timeline.valid=false;}
        } else { Missing(std::move(list));return; }
        missingSince=0;
        Publish(std::move(next),std::move(list));
    }
    void Run() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            while(!stop) {
                try {Tick();} catch(...) {
                    sessionsChanged.revoke();currentChanged.revoke();Watch(nullptr);
                    manager=nullptr;Missing({});
                }
                if(!stop) WaitForSingleObject(signal->wake,missingSince&&GetTickCount64()-missingSince<1500?100:650);
            }
            sessionsChanged.revoke();currentChanged.revoke();Watch(nullptr);
            known.clear();manager=nullptr;winrt::uninit_apartment();
        } catch(...) {Publish({},{});}
    }
};
MediaWatcher::MediaWatcher():m_impl(std::make_unique<Impl>()){}
MediaWatcher::~MediaWatcher(){Stop();}
void MediaWatcher::Start(HWND notify) {
    if(m_impl->thread.joinable()) return;
    m_impl->notify=notify;m_impl->stop=false;m_impl->signal=std::make_shared<Impl::Signal>();
    if(m_impl->signal->wake) m_impl->thread=std::thread([this]{m_impl->Run();});
}
void MediaWatcher::Stop() {
    if(!m_impl || !m_impl->thread.joinable()) return;
    m_impl->stop=true;SetEvent(m_impl->signal->wake);m_impl->thread.join();
    m_impl->signal.reset();
}
MediaSnapshot MediaWatcher::Snapshot()const {std::lock_guard lock(m_impl->mutex);return m_impl->snapshot;}
std::vector<SessionEntry> MediaWatcher::Sessions()const {std::lock_guard lock(m_impl->mutex);return m_impl->entries;}
std::wstring MediaWatcher::SelectedId()const {std::lock_guard lock(m_impl->mutex);return m_impl->selected;}
void MediaWatcher::Select(const std::wstring& id) {{std::lock_guard lock(m_impl->mutex);m_impl->selected=id;m_impl->selectedApp.clear();
    for(const auto& entry:m_impl->sessionApps)if(entry.first==id){m_impl->selectedApp=entry.second;break;}
    }if(m_impl->signal)SetEvent(m_impl->signal->wake);}
void MediaWatcher::Send(MediaCommand cmd) {
    {std::lock_guard lock(m_impl->mutex);if(m_impl->snapshot.valid&&!m_impl->snapshot.reconnecting&&m_impl->commands.size()<8)m_impl->commands.push_back({cmd,m_impl->snapshot.sessionId});}
    if(m_impl->signal)SetEvent(m_impl->signal->wake);
}
