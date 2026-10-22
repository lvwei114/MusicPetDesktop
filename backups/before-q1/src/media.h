// GSMTC（Windows.Media.Control）适配器：后台线程轮询媒体会话状态。
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app.h"

enum MediaCommand {
    CMD_TOGGLE = 1,
    CMD_NEXT = 2,
    CMD_PREV = 3,
};

class MediaWatcher {
public:
    MediaWatcher();
    ~MediaWatcher();

    void Start(HWND notify);
    void Stop();

    MediaSnapshot Snapshot() const;
    std::vector<SessionEntry> Sessions() const;
    std::wstring SelectedId() const;

    void Select(const std::wstring& sessionId);  // 空字符串 = 自动选择
    void Send(MediaCommand cmd);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
