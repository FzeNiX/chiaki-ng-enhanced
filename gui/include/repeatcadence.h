#ifndef CHIAKI_GUI_REPEATCADENCE_H
#define CHIAKI_GUI_REPEATCADENCE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

// Recognises a stream that sends every picture twice, e.g. a 30 fps game in a
// 60 fps Remote Play stream. The console then encodes the repeat as an almost
// empty P-frame, so it shows up as a tiny packet between normal ones. Frame
// generation fed with those repeats interpolates between identical pictures
// and then jumps, which looks wobbly; dropping them restores an even 30 fps
// source that frame generation can fill properly.
class RepeatCadenceDetector
{
public:
    // Feeds the compressed size of the next frame (0 = unknown, ignored).
    // Returns true if that frame repeats the previous picture and can be
    // dropped.
    bool feed(size_t packet_size)
    {
        if (!packet_size)
            return false;
        const double size = static_cast<double>(packet_size);
        // Decaying peak of recent sizes: follows bitrate changes and lets a
        // long static scene (all frames tiny) count as normal again.
        large = (std::max)(size, large * 0.97); // parenthesised: windows.h defines max()
        last_size = packet_size;
        const bool tiny = size < large * kTinyRatio;
        history = (history << 1) | (tiny ? 1u : 0u);
        if (seen < 32)
            seen++;
        if (!locked) {
            // 12 frames (200 ms at 60 fps) strictly alternating.
            const uint32_t last12 = history & 0xfffu;
            if (seen >= 12 && (last12 == 0xaaau || last12 == 0x555u))
                locked = true;
        } else if ((history & 3u) == 0) {
            // Two new pictures in a row: the game runs at the stream rate.
            locked = false;
        }
        return locked && tiny;
    }

    // Stream frames per picture: 2 while every picture is repeated, else 1.
    int cadence() const { return locked ? 2 : 1; }
    size_t lastSize() const { return last_size; }
    double peakSize() const { return large; }

    void reset() { *this = RepeatCadenceDetector(); }

private:
    static constexpr double kTinyRatio = 0.12;
    double large = 0.0;
    size_t last_size = 0;
    uint32_t history = 0;
    int seen = 0;
    bool locked = false;
};

#endif // CHIAKI_GUI_REPEATCADENCE_H
