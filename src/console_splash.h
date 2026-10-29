#pragma once
// Launch animation for the RareStage Console, in the family of the RareClip intro: a swarm of probes closes in,
// lasers flicker across the frame, the swarm assimilates into the Rare mark with a flash, a scan etches the
// wordmark, the version number decodes in, and the whole thing fades into the console. A click or key skips it.
// Everything is drawn from one clock into the ImGui background draw list, so it costs nothing once it ends.
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "imgui.h"

namespace rarestage::console {

class Splash {
public:
    static constexpr float Duration = 3.7f, Fade = .45f;

    bool load(ID3D11Device* device, const std::wstring& logoPath, const std::string& version) {
        version_ = version;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromFilename(logoPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        decoder->GetFrame(0, &frame);
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        factory->CreateFormatConverter(&converter);
        if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return false;
        UINT w = 0, h = 0;
        converter->GetSize(&w, &h);
        std::vector<unsigned char> pixels(size_t(w) * h * 4);
        if (FAILED(converter->CopyPixels(nullptr, w * 4, UINT(pixels.size()), pixels.data()))) return false;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.data(), w * 4, 0};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if (FAILED(device->CreateTexture2D(&td, &data, &texture)) || FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &srv_))) return false;
        buildSwarm(pixels, int(w), int(h));
        ready_ = true;
        return true;
    }

    bool active() const { return ready_ && !finished_; }
    void skip() { if (active() && clock_ < Duration) clock_ = Duration; }

    // Advances the clock and draws one frame. Returns false once the sequence has finished.
    bool draw(ImDrawList* dl, ImVec2 origin, ImVec2 size, float dt) {
        if (!active()) return false;
        clock_ += std::min(dt, .05f);
        if (clock_ >= Duration + Fade) { finished_ = true; return false; }
        const float t = clock_;
        const float fadeOut = t > Duration ? 1.f - smooth((t - Duration) / Fade) : 1.f;
        const ImVec2 mid{origin.x + size.x * .5f, origin.y + size.y * .5f - size.y * .04f};
        const float logoSide = std::min(size.x, size.y) * .42f;
        const ImVec2 lo{mid.x - logoSide * .5f, mid.y - logoSide * .5f}, hi{mid.x + logoSide * .5f, mid.y + logoSide * .5f};

        dl->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, col(0.035f, 0.045f, 0.06f, fadeOut));

        // Probes closing in on the mark.
        const float swarmFade = 1.f - smooth((t - 1.5f) / .4f);
        if (swarmFade > 0.f) {
            for (const Probe& p : swarm_) {
                const float u = ease(clamp01((t - .2f - p.delay) / p.dur));
                const float cx = p.sx + (p.tx - p.sx) * u, cy = p.sy + (p.ty - p.sy) * u;
                const float swirl = (1.f - u) * std::sin(t * 3.f + p.seed * 9.f) * .07f;
                const float px = mid.x + (cx - .5f) * logoSide + (cy - .5f) * logoSide * swirl * 4.f;
                const float py = mid.y + (cy - .5f) * logoSide - (cx - .5f) * logoSide * swirl * 4.f;
                const float a = swarmFade * fadeOut * (.35f + .65f * u) * (.75f + .25f * std::sin(t * 20.f + p.seed * 40.f));
                const float s = (1.2f + p.size * (1.f - .5f * u)) * dpi_;
                dl->AddRectFilled({px - s, py - s}, {px + s, py + s}, col(.24f + .7f * u, .9f, 1.f, a));
                if (u < .98f) dl->AddLine({px, py}, {px - (p.tx - p.sx) * logoSide * .02f, py - (p.ty - p.sy) * logoSide * .02f}, col(.24f, .9f, 1.f, a * .4f), 1.f);
            }
        }
        // Lasers flicker in while the swarm closes.
        if (t > .6f && t < 1.55f) {
            unsigned seed = unsigned(t * 28.f);
            for (int i = 0; i < 12; ++i) {
                const float a0 = hash01(seed * 31u + unsigned(i)) * 6.2832f, r0 = std::max(size.x, size.y) * .62f;
                const ImVec2 from{mid.x + std::cos(a0) * r0, mid.y + std::sin(a0) * r0};
                const float tx = lo.x + hash01(seed * 7u + unsigned(i) * 13u) * (hi.x - lo.x), ty = lo.y + hash01(seed * 5u + unsigned(i) * 17u) * (hi.y - lo.y);
                dl->AddLine(from, {tx, ty}, col(.3f, .95f, 1.f, .22f * fadeOut * (1.f - smooth((t - 1.35f) / .2f))), 1.f);
            }
        }
        // The mark resolves with a flash.
        const float logoA = smooth((t - 1.35f) / .5f) * fadeOut;
        if (logoA > 0.f) {
            dl->AddImage(reinterpret_cast<ImTextureID>(srv_.Get()), lo, hi, {0, 0}, {1, 1}, col(1, 1, 1, logoA));
            const float flash = std::exp(-std::max(t - 1.5f, 0.f) * 7.f) * (t > 1.5f ? .38f : 0.f) * fadeOut;
            if (flash > .003f) dl->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, col(.8f, 1.f, 1.f, flash));
        }
        // Wordmark, etched by a scan line.
        const float fontSize = std::min(size.x, size.y) * .062f;
        const ImVec2 wordTop{mid.x, hi.y + fontSize * .6f};
        const char* rare = "RARE";
        const char* stage = "STAGE";
        const float spacing = fontSize * .22f;
        ImFont* font = ImGui::GetFont();
        auto widthOf = [&](const char* s) { float w = 0.f; for (const char* c = s; *c; ++c) w += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, c, c + 1).x + spacing; return w; };
        const float total = widthOf(rare) + widthOf(stage);
        const float scanU = clamp01((t - 1.62f) / .78f);
        const float scanY = wordTop.y - fontSize * .2f + (fontSize * 1.5f) * scanU;
        if (t > 1.62f) {
            dl->PushClipRect({origin.x, wordTop.y - fontSize * .3f}, {origin.x + size.x, std::min(scanY, wordTop.y + fontSize * 1.4f)}, true);
            float x = wordTop.x - total * .5f;
            for (const char* c = rare; *c; ++c) { dl->AddText(font, fontSize, {x, wordTop.y}, col(.94f, .97f, 1.f, fadeOut), c, c + 1); x += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, c, c + 1).x + spacing; }
            for (const char* c = stage; *c; ++c) { dl->AddText(font, fontSize, {x, wordTop.y}, col(.24f, .9f, 1.f, fadeOut), c, c + 1); x += font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, c, c + 1).x + spacing; }
            dl->PopClipRect();
            if (scanU < 1.f) {
                dl->AddRectFilled({wordTop.x - total * .7f, scanY - 1.5f}, {wordTop.x + total * .7f, scanY + 1.5f}, col(.4f, 1.f, 1.f, .9f * fadeOut));
                dl->AddRectFilledMultiColor({wordTop.x - total * .7f, scanY - fontSize * .4f}, {wordTop.x + total * .7f, scanY}, col(.2f, .9f, 1.f, 0.f), col(.2f, .9f, 1.f, 0.f), col(.2f, .9f, 1.f, .22f * fadeOut), col(.2f, .9f, 1.f, .22f * fadeOut));
            }
        }
        // Version number decodes in, then the tagline.
        if (t > 2.3f && !version_.empty()) {
            static const char glyphs[] = "0123456789ABCDEF#%/<>+=";
            std::string text = "v" + version_;
            std::string shown;
            for (size_t i = 0; i < text.size(); ++i) {
                const float resolveAt = 2.3f + float(i) * .08f;
                shown += t >= resolveAt + .25f || text[i] == '.' ? text[i] : glyphs[unsigned(hash01(unsigned(t * 40.f) * 13u + unsigned(i) * 7u) * (sizeof(glyphs) - 2))];
            }
            const float vs = fontSize * .42f;
            const ImVec2 ext = font->CalcTextSizeA(vs, FLT_MAX, 0.f, shown.c_str());
            dl->AddText(font, vs, {mid.x - ext.x * .5f, wordTop.y + fontSize * 1.55f}, col(.6f, .75f, .85f, .85f * fadeOut * smooth((t - 2.3f) / .3f)), shown.c_str());
        }
        if (t > 2.65f) {
            const char* tag = "REAL-TIME STAGE VISUALS";
            const float ts = fontSize * .3f;
            const ImVec2 ext = font->CalcTextSizeA(ts, FLT_MAX, 0.f, tag);
            dl->AddText(font, ts, {mid.x - ext.x * .5f, wordTop.y + fontSize * 2.15f}, col(.24f, .9f, 1.f, .7f * fadeOut * smooth((t - 2.65f) / .5f)), tag);
        }
        // CRT scanlines and a vignette over everything.
        for (float y = origin.y; y < origin.y + size.y; y += 3.f * dpi_) dl->AddLine({origin.x, y}, {origin.x + size.x, y}, col(1, 1, 1, .028f * fadeOut), 1.f);
        const float vg = size.y * .35f;
        const ImU32 dark = col(0, 0, 0, .5f * fadeOut), clear = col(0, 0, 0, 0);
        dl->AddRectFilledMultiColor(origin, {origin.x + size.x, origin.y + vg}, dark, dark, clear, clear);
        dl->AddRectFilledMultiColor({origin.x, origin.y + size.y - vg}, {origin.x + size.x, origin.y + size.y}, clear, clear, dark, dark);
        return true;
    }

    void setDpi(float dpi) { dpi_ = dpi; }

private:
    struct Probe { float sx, sy, tx, ty, delay, dur, size, seed; };
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    std::vector<Probe> swarm_;
    std::string version_;
    float clock_{0}, dpi_{1};
    bool ready_{false}, finished_{false};

    static float clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }
    static float smooth(float v) { v = clamp01(v); return v * v * (3.f - 2.f * v); }
    static float ease(float u) { return u < .5f ? 4.f * u * u * u : 1.f - std::pow(-2.f * u + 2.f, 3.f) / 2.f; }
    static float hash01(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return float(x & 0xFFFFFF) / 16777215.f; }
    static ImU32 col(float r, float g, float b, float a) { return ImGui::ColorConvertFloat4ToU32({r, g, b, clamp01(a)}); }

    // Targets are random opaque pixels of the mark; starts sit on a wide ring, so the swarm reads as the mark
    // being assembled out of the dark.
    void buildSwarm(const std::vector<unsigned char>& pixels, int w, int h) {
        swarm_.clear();
        unsigned counter = 1;
        while (swarm_.size() < 900 && counter < 200000) {
            const float u = hash01(counter * 3u), v = hash01(counter * 5u + 1u);
            ++counter;
            const int x = std::min(w - 1, int(u * w)), y = std::min(h - 1, int(v * h));
            if (pixels[(size_t(y) * w + size_t(x)) * 4 + 3] < 110) continue;
            const float angle = hash01(counter * 7u) * 6.2832f, radius = 1.1f + hash01(counter * 11u) * .9f;
            swarm_.push_back({.5f + std::cos(angle) * radius, .5f + std::sin(angle) * radius, u, v, hash01(counter * 13u) * .5f, .7f + hash01(counter * 17u) * .3f, hash01(counter * 19u) * 1.4f, hash01(counter * 23u)});
        }
    }
};
}
