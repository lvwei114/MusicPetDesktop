#include "effects.h"
#include "gfx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
using namespace Gdiplus;
NoteLayout ComputeNoteLayout(int imageWidth,int imageHeight,UINT dpi) {
    const float d=std::max(1U,dpi)/96.f;
    const float extent=std::min((float)imageHeight,imageWidth*1.8f)/d;
    const float scale=std::clamp(extent/300.f,.25f,2.f)*d;
    // Include the widest spawn, full lifetime drift, sway and rotated glyph bounds.
    return {scale,(int)std::ceil(imageWidth*.09f+80*scale+8*d),
        (int)std::ceil(170*scale+8*d),(int)std::ceil(25*scale+8*d)};
}
NoteLayer::~NoteLayer() { Release(); }
void NoteLayer::Release() {
    delete m_canvas; m_canvas = nullptr;
    if (m_dc && m_old) SelectObject(m_dc, m_old);
    if (m_dc) DeleteDC(m_dc);
    if (m_dib) DeleteObject(m_dib);
    m_dc = nullptr; m_dib = m_old = nullptr; m_bits = nullptr;
}
bool NoteLayer::Create(HWND h, int w, int height) { m_hwnd = h; return Resize(w, height); }
bool NoteLayer::Resize(int w, int h) {
    if (m_w == w && m_h == h && m_canvas) return true;
    Release(); m_particles.clear(); m_w = w; m_h = h;
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    m_dib = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    m_dc = CreateCompatibleDC(nullptr);
    if (!m_dib || !m_dc) { Release(); return false; }
    m_old = (HBITMAP)SelectObject(m_dc, m_dib);
    m_canvas = new Bitmap(w, h, PixelFormat32bppPARGB);
    if (m_canvas->GetLastStatus() != Ok) { Release(); return false; }
    return true;
}
void NoteLayer::Configure(int density, float scale) { m_density = std::clamp(density, 0, 3); m_scale = std::clamp(scale, .1f, 16.f); }
void NoteLayer::SetPalette(int mode,const void* pixels,int width,int height) {
    m_palette={Color(255,157,184,163),Color(255,210,188,145),Color(255,197,161,170)};
    if(mode!=1||!pixels||width<=0||height<=0)return;
    struct Bin {double weight{},r{},g{},b{};};
    Bin bins[512]{};
    const auto data=static_cast<const BYTE*>(pixels);
    const int step=std::max(1,(int)std::sqrt((double)width*height/24000.));
    for(int y=0;y<height;y+=step)for(int x=0;x<width;x+=step){
        const auto p=data+(size_t(y)*width+x)*4;const int alpha=p[3];if(alpha<96)continue;
        // The artwork DIB uses premultiplied BGRA; unpremultiply before sampling.
        const int red=std::min(255,p[2]*255/alpha),green=std::min(255,p[1]*255/alpha),blue=std::min(255,p[0]*255/alpha);
        const int hi=std::max({red,green,blue}),lo=std::min({red,green,blue});
        if(hi<45||lo>232||hi-lo<18)continue;
        const int index=(red/32)*64+(green/32)*8+blue/32;
        const double weight=(alpha/255.)*(.25+(hi-lo)/255.);
        auto& bin=bins[index];bin.weight+=weight;bin.r+=red*weight;bin.g+=green*weight;bin.b+=blue*weight;
    }
    std::vector<Bin> ranked;for(const auto& bin:bins)if(bin.weight>0)ranked.push_back(bin);
    std::sort(ranked.begin(),ranked.end(),[](const Bin& x,const Bin& y){return x.weight>y.weight;});
    std::vector<Color> palette;
    for(const auto& bin:ranked){
        const double red=bin.r/bin.weight,green=bin.g/bin.weight,blue=bin.b/bin.weight;
        const double gray=.299*red+.587*green+.114*blue;
        auto soften=[&](double c){return (BYTE)std::clamp((gray*.42+c*.58)*.72+238*.28,100.,218.);};
        const Color c(255,soften(red),soften(green),soften(blue));bool distinct=true;
        for(const auto& other:palette){const double dr=(int)c.GetR()-other.GetR(),dg=(int)c.GetG()-other.GetG(),db=(int)c.GetB()-other.GetB();
            if(dr*dr+dg*dg+db*db<900)distinct=false;}
        if(distinct)palette.push_back(c);if(palette.size()==3)break;
    }
    // Transparent or grayscale artwork keeps the default palette.
    if(!palette.empty())m_palette=std::move(palette);
}
void NoteLayer::SetOrigin(float x, float y, float r) { m_x = x; m_y = y; m_radius = r; }
void NoteLayer::Spawn() {
    if (m_particles.size() >= 32) return;
    auto random = [&] { return std::uniform_real_distribution<float>(0, 1)(m_rng); };
    const float side = random() > .5f ? 1.f : -1.f;
    NoteParticle p;
    p.x = m_x + side * m_radius * (.70f + .38f * random());
    p.y = m_y + (random() - .5f) * 36 * m_scale;
    p.vx = side * (4 + random() * 7) * m_scale;
    p.vy = -(20 + random() * 13) * m_scale;
    p.size = (17 + random() * 10) * m_scale;
    p.maxLife = 2.8f + random() * 1.3f;
    p.phase = random() * 6.283185f; p.rot = (random() - .5f) * 25;
    p.kind = (int)(random() * 5);
    p.color = m_palette[m_rng() % m_palette.size()]; m_particles.push_back(p);
}
bool NoteLayer::Tick(double dt) {
    dt = std::clamp(dt, .001, .1);
    m_previewLeft = std::max(0., m_previewLeft - dt);
    const bool emit = (m_playing || m_previewLeft > 0) && m_density > 0;
    if (emit) {
        m_acc += dt * (m_density == 1 ? 1.7 : m_density == 2 ? 2.8 : 4.2);
        while (m_acc >= 1) { Spawn(); m_acc -= 1; }
    } else m_acc = 0;
    for (auto& p : m_particles) {
        p.life += (float)dt; p.x += p.vx * (float)dt; p.y += p.vy * (float)dt;
    }
    std::erase_if(m_particles, [](const auto& p) { return p.life >= p.maxLife; });
    return emit || !m_particles.empty();
}
void NoteLayer::Paint(Graphics& g) const {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    for (const auto& p : m_particles) {
        const float t = p.life / p.maxLife;
        const float alpha = std::min(1.f, p.life / .35f) * std::min(1.f, (p.maxLife - p.life) / 1.1f);
        const float x = p.x + std::sin(p.phase + p.life * 1.7f) * 6.f * m_scale;
        const float angle = p.rot + std::sin(p.life * 1.2f + p.phase) * 9.f;
        const Color c((BYTE)(alpha * 225), p.color.GetR(), p.color.GetG(), p.color.GetB());
        if (p.kind == 4) {
            Pen pen(c, 1.3f * m_scale); const float r = 3.f * m_scale;
            g.DrawLine(&pen, x-r, p.y, x+r, p.y); g.DrawLine(&pen, x, p.y-r, x, p.y+r);
        } else {
            // Fine dark outline keeps pastel notes legible over both white and dark apps.
            for (int j = 0; j < 4; ++j)
                DrawNoteGlyph(g, x + (j == 0 ? -.7f : j == 1 ? .7f : 0), p.y + (j == 2 ? -.7f : j == 3 ? .7f : 0), p.size, angle, Color((BYTE)(alpha * 55), 40, 52, 68), p.kind);
            DrawNoteGlyph(g, x, p.y, p.size, angle, c, p.kind);
        }
    }
}
void NoteLayer::Render() {
    if (!m_canvas || !m_hwnd) return;
    { Graphics g(m_canvas); g.SetCompositingMode(CompositingModeSourceCopy); g.Clear(Color(0,0,0,0)); g.SetCompositingMode(CompositingModeSourceOver); Paint(g); }
    BitmapData data{}; Rect rect(0,0,m_w,m_h);
    if (m_canvas->LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &data) != Ok) return;
    for (int y = 0; y < m_h; ++y) memcpy((BYTE*)m_bits + size_t(y)*m_w*4, (BYTE*)data.Scan0 + ptrdiff_t(y)*data.Stride, size_t(m_w)*4);
    m_canvas->UnlockBits(&data);
    SIZE size{m_w,m_h}; POINT source{}; BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    UpdateLayeredWindow(m_hwnd,nullptr,nullptr,&size,m_dc,&source,0,&blend,ULW_ALPHA);
    if (!m_visible) { ShowWindow(m_hwnd,SW_SHOWNOACTIVATE); m_visible=true; }
}
void NoteLayer::Hide() { if (m_hwnd) ShowWindow(m_hwnd,SW_HIDE); m_visible=false; }
