#include "gfx.h"
using namespace Gdiplus;
void DrawNoteGlyph(Graphics& g, float cx, float cy, float size, float angle, const Color& color) {
    const auto state = g.Save();
    // Prepend local transforms: rotation and scale must not scale the translation.
    g.TranslateTransform(cx, cy);
    g.RotateTransform(angle);
    g.ScaleTransform(size / 100.f, size / 100.f);
    SolidBrush brush(color);
    Pen stem(color, 8.f);
    stem.SetStartCap(LineCapRound); stem.SetEndCap(LineCapRound);
    const auto headState = g.Save();
    g.TranslateTransform(-8.f, 28.f); g.RotateTransform(-22.f);
    g.FillEllipse(&brush, -18.f, -11.f, 36.f, 22.f);
    g.Restore(headState);
    g.DrawLine(&stem, 8.f, 25.f, 8.f, -40.f);
    GraphicsPath flag;
    flag.AddBezier(8.f, -41.f, 14.f, -22.f, 47.f, -26.f, 25.f, 0.f);
    flag.AddBezier(25.f, 0.f, 37.f, -20.f, 16.f, -16.f, 8.f, -24.f);
    flag.CloseFigure(); g.FillPath(&brush, &flag);
    g.Restore(state);
}
