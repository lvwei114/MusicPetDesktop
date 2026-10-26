#pragma once
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
// Local files only. Decoding is isolated from the UI thread, with one latest frame.
class MotionPlayer {
public:
    MotionPlayer();~MotionPlayer();
    void Configure(const std::wstring& path,int width,int height,int speedPercent=100,bool pingPong=false);
    void Play(bool playing);
    bool Frame(std::vector<BYTE>& pixels);
    bool Failed();
    bool Active(); // includes finishing the current cycle after Play(false)
    void Stop();
private:
    struct Impl;std::unique_ptr<Impl> impl;
};
