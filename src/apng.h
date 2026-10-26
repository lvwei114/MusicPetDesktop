#pragma once
#include <windows.h>
#include <string>
#include <vector>
struct ApngFrame {
    unsigned x{},y{},width{},height{},delay{},dispose{},blend{};
    std::vector<BYTE> png;
};
struct ApngImage {unsigned width{},height{};std::vector<ApngFrame> frames;};
ApngImage ReadApng(const std::wstring& path);
