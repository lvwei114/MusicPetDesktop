// 共享类型：媒体状态快照、会话条目、自定义窗口消息。
#pragma once

#include <windows.h>
#include <cstdint>
#include <string>

enum class PlayStatus {
    Unknown = 0,
    Playing = 1,
    Paused = 2,
    Stopped = 3,
};

struct MediaSnapshot {
    bool valid = false;      // 是否连上了某个媒体会话
    bool reconnecting = false; // 短暂会话过渡；保留曲目展示，但禁用控制和进度。
    std::wstring sessionId;  // GSMTC 会话 Id
    std::wstring appId;      // SourceAppUserModelId
    std::wstring appName;    // 友好名称
    std::wstring title;
    std::wstring artist;
    PlayStatus status = PlayStatus::Unknown;
    bool canToggle = false;
    bool canNext = false;
    bool canPrev = false;
    bool canShuffle = false, canRepeat = false;
    int shuffle = -1;                   // -1 unknown, 0 off, 1 on
    int repeat = -1;                    // -1 unknown, 0 none, 1 track, 2 list
    double positionSec = 0.0;
    double durationSec = 0.0;
    uint64_t revision = 0;
};

struct SessionEntry {
    std::wstring id;
    std::wstring appName;
    bool playing = false;
};

constexpr UINT WM_APP_MEDIA_UPDATE = WM_APP + 1;
constexpr UINT WM_APP_TRAY = WM_APP + 2;
