#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <random>
#include <vector>
struct NoteLayout {float scale;int margin,top,bottom;};
NoteLayout ComputeNoteLayout(int imageWidth,int imageHeight,UINT dpi);
struct NoteParticle {
    float x{}, y{}, vx{}, vy{}, size{}, life{}, maxLife{}, phase{}, rot{};
    int kind{};
    Gdiplus::Color color;
};
class NoteLayer {
public:
    ~NoteLayer();
    bool Create(HWND window, int w, int h);
    bool Resize(int w, int h);
    void Configure(int density, float scale);
    void SetPalette(int mode, const void* pixels, int width, int height);
    void SetOrigin(float x, float y, float radius);
    void SetPlaying(bool playing) { m_playing = playing; }
    void Preview(double seconds) { m_previewLeft = seconds; }
    bool Tick(double dt);
    void Paint(Gdiplus::Graphics& graphics) const;
    void Render();
    void Hide();
    void Shutdown() { Release(); }
    int Width() const { return m_w; }
    int Height() const { return m_h; }
private:
    void Spawn();
    void Release();
    HWND m_hwnd{};
    int m_w{}, m_h{}, m_density{1};
    float m_scale{1}, m_x{}, m_y{}, m_radius{60};
    bool m_playing{}, m_visible{};
    double m_previewLeft{}, m_acc{};
    HDC m_dc{};
    HBITMAP m_dib{}, m_old{};
    void* m_bits{};
    Gdiplus::Bitmap* m_canvas{};
    std::vector<NoteParticle> m_particles;
    std::vector<Gdiplus::Color> m_palette{Gdiplus::Color(255,157,184,163),Gdiplus::Color(255,210,188,145),Gdiplus::Color(255,197,161,170)};
    std::mt19937 m_rng{0x4d555349};
};
