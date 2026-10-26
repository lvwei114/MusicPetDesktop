#pragma once
#include <algorithm>
#include <cmath>
struct ArtworkRect {int x,y,width,height;};
// One canvas transform for both idle and animated artwork. Never inspect a
// frame's alpha bounds: changing pose must not alter scale or anchor position.
inline ArtworkRect FitArtwork(unsigned width,unsigned height,int targetWidth,int targetHeight){
    const double scale=std::min(double(targetWidth)/width,double(targetHeight)/height);
    const int w=std::max(1,(int)std::lround(width*scale)),h=std::max(1,(int)std::lround(height*scale));
    return {(targetWidth-w)/2,targetHeight-h,w,h}; // center horizontally, feet on same baseline
}
