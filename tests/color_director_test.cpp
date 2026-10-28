#include "color_director.h"

#include <array>
#include <cmath>
#include <iostream>

using namespace rarestage::show;

namespace {
bool Fail(const char* message) {
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

bool RingIsOrderedAndWide() {
    static_assert(PaletteCount == 24, "the ring must hold 24 stops");
    for (int stop = 1; stop < PaletteCount; ++stop)
        if (Palettes[stop].primary <= Palettes[stop - 1].primary) return Fail("primary hues must rise around the ring");
    for (int stop = 0; stop < PaletteCount; ++stop) {
        const int next = (stop + 1) % PaletteCount;
        const float step = std::abs(PaletteDelta(Palettes[stop].primary, Palettes[next].primary) / 360.f * 24.f);
        if (!Palettes[stop].name || !Palettes[stop].name[0]) return Fail("every stop needs a name");
        if (Palettes[stop].primary < 0 || Palettes[stop].primary >= 360) return Fail("hue out of range");
        (void)step;
    }
    // Neighbouring stops stay within 35 degrees so a glide never skips a colour family.
    for (int stop = 0; stop < PaletteCount; ++stop) {
        const float gap = std::fmod(Palettes[(stop + 1) % PaletteCount].primary - Palettes[stop].primary + 360.f, 360.f);
        if (gap > 35.f) return Fail("neighbouring hues are too far apart");
    }
    // Every colour role is a vivid, balanced colour: none is black or washed to grey.
    for (int stop = 0; stop < PaletteCount; ++stop)
        for (int role = 0; role < 3; ++role) {
            const auto c = PaletteColor(stop, role);
            const float hi = std::max({c[0], c[1], c[2]}), lo = std::min({c[0], c[1], c[2]});
            if (hi < .45f || hi - lo < .45f) return Fail("palette colour is dull");
        }
    return true;
}

bool ShortestPathWraps() {
    if (std::abs(PaletteDelta(22.f, 1.f) - 3.f) > 1e-4f) return Fail("22 to 1 must travel +3 stops");
    if (std::abs(PaletteDelta(1.f, 22.f) + 3.f) > 1e-4f) return Fail("1 to 22 must travel -3 stops");
    if (std::abs(WrapPalette(-1.f) - 23.f) > 1e-4f || std::abs(WrapPalette(24.5f) - .5f) > 1e-4f) return Fail("wrap is wrong");
    ColorDirector color;
    color.reset(22.f);
    color.setTarget(1.f, 2.f);
    for (int frame = 0; frame < 60 * 10; ++frame) {
        color.update(1.f / 60.f);
        const float p = color.position();
        if (p > 2.f && p < 21.f) return Fail("glide crossed the far side of the ring");
    }
    if (std::abs(PaletteDelta(color.position(), 1.f)) > .05f) return Fail("glide did not arrive");
    return true;
}

bool GlideNeverOvershoots() {
    ColorDirector color;
    color.reset(4.f);
    color.setTarget(13.f, 3.2f);
    float previous = color.position();
    for (int frame = 0; frame < 60 * 20; ++frame) {
        color.update(1.f / 60.f);
        const float p = color.position();
        if (p < previous - 1e-4f) return Fail("glide moved backwards");
        if (p > 13.02f) return Fail("glide overshot its target");
        previous = p;
    }
    return std::abs(previous - 13.f) < .05f;
}

// Ten minutes at 60 fps: acts every ~20 s, a phrase retarget every 32 beats at 128 BPM.
bool LongSetIsSmoothAndVaried() {
    ColorDirector color;
    color.reset(13.f);
    std::array<int, PaletteCount> visits{};
    float previous = color.position(), previousStep = 0, worstStep = 0, worstAcceleration = 0, nextAct = 20, nextPhrase = 15;
    int section = 0, retargets = 0, repeats = 0, shortHops = 0;
    int last = 13;
    std::array<int, 4> recent{-1, -1, -1, -1};
    for (int frame = 0; frame < 60 * 600; ++frame) {
        const float seconds = frame / 60.f;
        const bool act = seconds >= nextAct, phrase = seconds >= nextPhrase;
        if (act || phrase) {
            if (act) { section = (section + 1) % 5; nextAct += 20; }
            if (phrase) nextPhrase += 15;
            color.retarget(section, .3f + .1f * float(section));
            const int target = int(std::lround(color.target())) % PaletteCount;
            ++visits[target];++retargets;
            for (int old : recent) repeats += old == target;
            shortHops += PaletteDistance(last, target) < 3;
            for (int i = 3; i > 0; --i) recent[i] = recent[i - 1];
            recent[0] = target;last = target;
        }
        color.update(1.f / 60.f);
        const float step = PaletteDelta(previous, color.position());
        worstStep = std::max(worstStep, std::abs(step));
        if (frame > 1) worstAcceleration = std::max(worstAcceleration, std::abs(step - previousStep));
        previousStep = step;previous = color.position();
    }
    // 16 stops a second is far above the fastest hand-off; a per-frame step above .35 would be a visible jump.
    if (worstStep > .35f) return Fail("colour stepped too far in one frame");
    if (worstAcceleration > .06f) return Fail("colour changed direction or speed abruptly");
    if (repeats) return Fail("a colour repeated inside the last four choices");
    if (shortHops) return Fail("a retarget was too small to notice");
    int unseen = 0, most = 0;
    for (int v : visits) { unseen += v == 0; most = std::max(most, v); }
    if (unseen) return Fail("some colours were never used");
    if (most > 3 * retargets / PaletteCount + 3) return Fail("one colour dominated the set");
    std::cout << "retargets " << retargets << ", worst step " << worstStep << ", worst acceleration " << worstAcceleration
              << ", most used " << most << " of " << retargets << '\n';
    return true;
}

bool ActsShapeTheHop() {
    auto average = [](int section) {
        ColorDirector color;
        color.reset(13.f);
        float total = 0;
        for (int n = 0; n < 40; ++n) {
            const float before = color.target();
            color.retarget(section, .5f);
            total += std::abs(PaletteDelta(before, color.target()));
        }
        return total / 40;
    };
    if (average(2) <= average(0)) return Fail("a drop should hop further than an open");
    return true;
}
}  // namespace

int main() {
    if (!RingIsOrderedAndWide() || !ShortestPathWraps() || !GlideNeverOvershoots() || !LongSetIsSmoothAndVaried() || !ActsShapeTheHop()) return 1;
    std::cout << "colour director checks passed\n";
    return 0;
}
