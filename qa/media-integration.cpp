#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <SystemMediaTransportControlsInterop.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Foundation.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>
#include <cmath>
#include "media.h"
using namespace winrt;
using namespace Windows::Media;
using namespace std::chrono_literals;
int main(){
    std::ofstream report("qa/media-report.txt");
    try{
        init_apartment(apartment_type::multi_threaded);
        SetCurrentProcessExplicitAppUserModelID(L"MusicPet.IntegrationTest");
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"MusicPet.MediaFixture";RegisterClassW(&wc);
        auto window=CreateWindowW(wc.lpszClassName,L"MusicPet test fixture",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
        SystemMediaTransportControls controls{nullptr};
        auto interop=get_activation_factory<SystemMediaTransportControls,ISystemMediaTransportControlsInterop>();
        check_hresult(interop->GetForWindow(window,guid_of<SystemMediaTransportControls>(),put_abi(controls)));
        controls.IsPlayEnabled(true);controls.IsPauseEnabled(true);controls.IsNextEnabled(true);controls.IsPreviousEnabled(true);
        controls.IsEnabled(true);controls.PlaybackStatus(MediaPlaybackStatus::Playing);
        controls.ShuffleEnabled(false);controls.AutoRepeatMode(MediaPlaybackAutoRepeatMode::None);
        auto shuffleToken=controls.ShuffleEnabledChangeRequested([](auto const& sender,auto const& args){sender.ShuffleEnabled(args.RequestedShuffleEnabled());});
        auto repeatToken=controls.AutoRepeatModeChangeRequested([](auto const& sender,auto const& args){sender.AutoRepeatMode(args.RequestedAutoRepeatMode());});
        auto updater=controls.DisplayUpdater();updater.Type(MediaPlaybackType::Music);updater.MusicProperties().Title(L"MusicPet integration track");updater.MusicProperties().Artist(L"Local test fixture");updater.Update();
        SystemMediaTransportControlsTimelineProperties timeline;timeline.StartTime(0s);timeline.EndTime(180s);timeline.MinSeekTime(0s);timeline.MaxSeekTime(180s);timeline.Position(15s);controls.UpdateTimelineProperties(timeline);
        std::atomic<int> toggles{},nexts{};
        auto token=controls.ButtonPressed([&](auto const& sender,auto const& args){
            if(args.Button()==SystemMediaTransportControlsButton::Pause){sender.PlaybackStatus(MediaPlaybackStatus::Paused);++toggles;}
            if(args.Button()==SystemMediaTransportControlsButton::Play){sender.PlaybackStatus(MediaPlaybackStatus::Playing);++toggles;}
            if(args.Button()==SystemMediaTransportControlsButton::Next){auto u=sender.DisplayUpdater();u.MusicProperties().Title(L"MusicPet second track");u.Update();++nexts;}
        });
        MediaWatcher watcher;watcher.Start(window);
        auto wait=[&](auto predicate,int ms=6500){auto end=GetTickCount64()+ms;while(GetTickCount64()<end){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(predicate())return true;std::this_thread::sleep_for(50ms);}return false;};
        std::wstring id;
        if(!wait([&]{for(auto& entry:watcher.Sessions())if(entry.appName.find(L"MusicPet")!=std::wstring::npos){id=entry.id;return true;}return false;}))throw std::runtime_error("Fixture did not publish an SMTC session");
        watcher.Select(id);
        if(!wait([&]{auto s=watcher.Snapshot();return s.sessionId==id&&s.status==PlayStatus::Playing&&s.title==L"MusicPet integration track";}))throw std::runtime_error("Metadata/playback read failed");
        report<<"PASS: real Windows SMTC session enumeration, metadata and playing state\n";
        auto state=watcher.Snapshot();if(!state.canToggle||!state.canNext||!state.canPrev||state.durationSec!=180||(state.positionSec<15||state.positionSec>25))throw std::runtime_error("Capabilities/timeline read failed");
        report<<"PASS: supported controls and timeline\n";
        if(!wait([&]{auto s=watcher.Snapshot();return s.shuffle==0&&s.repeat==0;}))throw std::runtime_error("Playback modes not read");
        if(watcher.Snapshot().canShuffle&&watcher.Snapshot().canRepeat){
            watcher.Send(CMD_SHUFFLE_ON);watcher.Send(CMD_REPEAT_TRACK);
            if(!wait([&]{auto s=watcher.Snapshot();return s.shuffle==1&&s.repeat==1;}))throw std::runtime_error("Shuffle/track repeat control failed");
            watcher.Send(CMD_SHUFFLE_OFF);watcher.Send(CMD_REPEAT_LIST);
            if(!wait([&]{auto s=watcher.Snapshot();return s.shuffle==0&&s.repeat==2;}))throw std::runtime_error("List repeat control failed");
            watcher.Send(CMD_REPEAT_OFF);
            if(!wait([&]{return watcher.Snapshot().repeat==0;}))throw std::runtime_error("Repeat off failed");
            report<<"PASS: shuffle on/off, track/list/off repeat round trips\n";
        }else report<<"LIMITATION: SMTC fixture did not expose writable playback modes; read verified\n";
        const double initial=watcher.Snapshot().positionSec;
        if(!wait([&]{return watcher.Snapshot().positionSec>initial+1.2;},3500))throw std::runtime_error("Sparse timeline did not advance");
        report<<"PASS: continuous progress from sparse publisher timeline\n";
        watcher.Send(CMD_TOGGLE);
        if(!wait([&]{return toggles>0&&watcher.Snapshot().status==PlayStatus::Paused;}))throw std::runtime_error("Pause command/state round trip failed");
        report<<"PASS: pause request reaches publisher and paused state returns\n";
        const double paused=watcher.Snapshot().positionSec;
        wait([]{return false;},1500);
        if(std::abs(watcher.Snapshot().positionSec-paused)>.05)throw std::runtime_error("Paused timeline advanced");
        report<<"PASS: paused progress stays fixed\n";
        watcher.Send(CMD_TOGGLE);
        if(!wait([&]{return toggles>1&&watcher.Snapshot().status==PlayStatus::Playing;}))throw std::runtime_error("Play command/state round trip failed");
        report<<"PASS: play request and returned state\n";
        const double resumed=watcher.Snapshot().positionSec;
        if(!wait([&]{return watcher.Snapshot().positionSec>resumed+.7;},3000))throw std::runtime_error("Resumed timeline did not advance");
        timeline.Position(70s);controls.UpdateTimelineProperties(timeline);
        if(!wait([&]{auto p=watcher.Snapshot().positionSec;return p>=70&&p<75;}))throw std::runtime_error("Seek correction failed");
        report<<"PASS: resume and seek correction\n";
        controls.PlaybackRate(2.);
        wait([]{return false;},800);
        const double rateStart=watcher.Snapshot().positionSec;
        if(!wait([&]{return watcher.Snapshot().positionSec>rateStart+2.8;},2500))throw std::runtime_error("Playback rate extrapolation failed");
        report<<"PASS: playback rate extrapolation\n";
        watcher.Send(CMD_NEXT);
        if(!wait([&]{return nexts>0&&watcher.Snapshot().title==L"MusicPet second track";}))throw std::runtime_error("Next track/metadata refresh failed");
        report<<"PASS: next-track command and refreshed title\n";
        controls.IsEnabled(false);
        bool disconnected=false;
        if(!wait([&]{auto s=watcher.Snapshot();disconnected|=!s.valid;return s.reconnecting;},1200))throw std::runtime_error("Missing session did not enter transition");
        auto transition=watcher.Snapshot();
        if(disconnected||transition.title!=L"MusicPet second track"||transition.canNext||transition.canToggle||transition.durationSec!=0)
            throw std::runtime_error("Transition lost title or exposed stale controls/timeline");
        watcher.Send(CMD_NEXT);
        wait([&]{disconnected|=!watcher.Snapshot().valid;return false;},350);
        const auto restoreAt=GetTickCount64();
        controls.IsEnabled(true);controls.PlaybackStatus(MediaPlaybackStatus::Playing);
        updater.MusicProperties().Title(L"Track after short gap");updater.Update();
        if(!wait([&]{auto s=watcher.Snapshot();disconnected|=!s.valid;return s.valid&&!s.reconnecting&&s.title==L"Track after short gap";},1200)||disconnected)
            throw std::runtime_error("Short gap flashed disconnected or failed to recover");
        if(nexts!=1)throw std::runtime_error("Command from transition was replayed");
        report<<"PASS: short session gap preserves title, hides stale timeline, disables/drops controls; recovery "<<GetTickCount64()-restoreAt<<" ms\n";
        const auto updateAt=GetTickCount64();
        updater.MusicProperties().Title(L"Event refreshed track");updater.Update();
        if(!wait([&]{return watcher.Snapshot().title==L"Event refreshed track";},1000))throw std::runtime_error("Metadata change did not wake worker");
        report<<"PASS: metadata event refresh "<<GetTickCount64()-updateAt<<" ms\n";
        controls.IsEnabled(false);
        controls.ButtonPressed(token);DestroyWindow(window);
        if(!wait([&]{return !watcher.Snapshot().valid;}))throw std::runtime_error("Pinned session did not become unavailable after closing");
        report<<"PASS: pinned session disappearance does not fall back to another app\n";
        auto replacementWindow=CreateWindowW(wc.lpszClassName,L"MusicPet replacement",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
        SystemMediaTransportControls replacement{nullptr};
        check_hresult(interop->GetForWindow(replacementWindow,guid_of<SystemMediaTransportControls>(),put_abi(replacement)));
        replacement.IsPlayEnabled(true);replacement.IsPauseEnabled(true);replacement.IsEnabled(true);replacement.PlaybackStatus(MediaPlaybackStatus::Playing);
        auto replacementUpdater=replacement.DisplayUpdater();replacementUpdater.Type(MediaPlaybackType::Music);
        replacementUpdater.MusicProperties().Title(L"Recreated player session");replacementUpdater.Update();
        if(!wait([&]{auto s=watcher.Snapshot();return s.valid&&s.title==L"Recreated player session"&&s.sessionId!=id&&watcher.SelectedId()==s.sessionId;}))throw std::runtime_error("Same-app session recreation did not rebind");
        if(watcher.Snapshot().durationSec!=0)throw std::runtime_error("Missing timeline must remain unknown");
        report<<"PASS: same-app recreated session rebinds; missing timeline stays unknown\n";
        replacement.IsEnabled(false);DestroyWindow(replacementWindow);
        const auto begin=GetTickCount64();watcher.Stop();if(GetTickCount64()-begin>2500)throw std::runtime_error("Worker shutdown too slow");
        report<<"PASS: clean worker shutdown\n";
        return 0;
    }catch(const hresult_error& e){report<<"FAIL HRESULT: "<<std::hex<<e.code().value<<" "<<to_string(e.message())<<"\n";return 1;}
    catch(const std::exception& e){report<<"FAIL: "<<e.what()<<"\n";return 2;}
}
