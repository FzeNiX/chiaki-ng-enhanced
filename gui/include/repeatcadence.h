#ifndef CHIAKI_GUI_REPEATCADENCE_H
#define CHIAKI_GUI_REPEATCADENCE_H

#include <cstddef>
#include <cstdint>

// Recognises a stream that sends every picture twice, e.g. a 30 fps game in a
// 60 fps Remote Play stream. The console encodes the repeat as a much smaller
// P-frame (on a PS5 about 7-12 KB against 80-100 KB for a new picture), so it
// shows up as a small packet between normal ones. Frame generation fed with
// those repeats interpolates between identical pictures and then jumps, which
// looks wobbly; dropping them restores an even 30 fps source.
class RepeatCadenceDetector
{
public:
    // Feeds the compressed size of the next frame (0 = unknown, ignored).
    // Returns true if that frame repeats the previous picture and can be
    // dropped. Only a small frame in the slot where a repeat is expected is
    // ever dropped, so a new picture is never lost to a misjudged size.
    bool feed(size_t packet_size)
    {
        if (!packet_size)
            return false;
        last_size = packet_size;
        const bool small = last_new > 0 && static_cast<double>(packet_size) < static_cast<double>(last_new) * kSmallRatio;
        if (!small)
            last_new = packet_size;

        if (!locked) {
            history = (history << 1) | (small ? 1u : 0u);
            if (seen < 32)
                seen++;
            // 12 frames (200 ms at 60 fps) strictly alternating.
            const uint32_t last12 = history & 0xfffu;
            if (seen >= 12 && (last12 == 0xaaau || last12 == 0x555u)) {
                locked = true;
                misses = 0;
                expect_repeat = !small;
                return small;
            }
            return false;
        }

        if (expect_repeat) {
            if (small) {
                misses = 0;
                expect_repeat = false;
                return true;
            }
            // A new picture where a repeat was due: the game runs faster.
            if (++misses >= 2)
                unlock();
            return false;
        }
        // Slot of a new picture: always kept. A small frame here means the game
        // dropped below 30 fps for a moment; keep the phase.
        expect_repeat = true;
        return false;
    }

    // Stream frames per picture: 2 while every picture is repeated, else 1.
    int cadence() const { return locked ? 2 : 1; }
    size_t lastSize() const { return last_size; }
    // Size of the last frame judged a new picture.
    size_t newPictureSize() const { return last_new; }

    void reset() { *this = RepeatCadenceDetector(); }

private:
    void unlock()
    {
        locked = false;
        history = 0;
        seen = 0;
    }

    static constexpr double kSmallRatio = 0.3;
    size_t last_new = 0;
    size_t last_size = 0;
    uint32_t history = 0;
    int seen = 0;
    int misses = 0;
    bool locked = false;
    bool expect_repeat = false;
};

#endif // CHIAKI_GUI_REPEATCADENCE_H
