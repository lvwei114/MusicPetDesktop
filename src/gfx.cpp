#include "gfx.h"
using namespace Gdiplus;
void DrawNoteGlyph(Graphics& g,float cx,float cy,float size,float angle,const Color& color,int kind) {
    const auto state=g.Save();
    g.TranslateTransform(cx,cy);g.RotateTransform(angle);g.ScaleTransform(size/100.f,size/100.f);
    SolidBrush brush(color);Pen stem(color,7.f);stem.SetStartCap(LineCapRound);stem.SetEndCap(LineCapRound);
    auto head=[&](float x,float y){const auto h=g.Save();g.TranslateTransform(x,y);g.RotateTransform(-22.f);
        g.FillEllipse(&brush,-17.f,-10.f,34.f,20.f);g.Restore(h);};
    auto flag=[&](float y){GraphicsPath p;
        p.AddBezier(8.f,y,14.f,y+19,45.f,y+15,25.f,y+41);
        p.AddBezier(25.f,y+41,37.f,y+21,16.f,y+25,8.f,y+17);
        p.CloseFigure();g.FillPath(&brush,&p);};
    if(kind==2){
        head(-25,29);head(22,18);
        g.DrawLine(&stem,-10.f,26.f,-10.f,-30.f);g.DrawLine(&stem,37.f,15.f,37.f,-41.f);
        PointF beam[]={{-13,-33},{40,-45},{40,-32},{-13,-20}};g.FillPolygon(&brush,beam,4);
    } else {
        head(-8,28);g.DrawLine(&stem,8.f,25.f,8.f,-40.f);
        if(kind==1||kind==3)flag(-41);
        if(kind==3)flag(-22);
    }
    g.Restore(state);
}
