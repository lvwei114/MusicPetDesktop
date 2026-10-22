// 共用的矢量音符图形，避免依赖字体。
#pragma once

#include <windows.h>  // gdiplus.h 依赖 wingdi 的 META_* 定义，必须先引入

#include <gdiplus.h>

// 在 (cx, cy) 中心绘制音符，size 约等于音符高度（像素），angle 为角度。
void DrawNoteGlyph(Gdiplus::Graphics& g, float cx, float cy, float size, float angle,
                   const Gdiplus::Color& color);
