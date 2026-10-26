#pragma once
#include "artwork-layout.h"
#include <gdiplus.h>
// The same sampler and pixel alignment for static and animated full canvases.
inline void RenderArtwork(const BYTE* raw,unsigned w,unsigned h,BYTE* output,int dw,int dh){
    Gdiplus::Bitmap source(w,h,w*4,PixelFormat32bppPARGB,const_cast<BYTE*>(raw));
    Gdiplus::Bitmap dest(dw,dh,dw*4,PixelFormat32bppPARGB,output);
    Gdiplus::Graphics g(&dest);g.Clear(Gdiplus::Color(0,0,0,0));
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::ImageAttributes attributes;attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
    const auto fit=FitArtwork(w,h,dw,dh);
    g.DrawImage(&source,Gdiplus::Rect(fit.x,fit.y,fit.width,fit.height),0,0,w,h,Gdiplus::UnitPixel,&attributes);
}
