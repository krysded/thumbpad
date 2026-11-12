#include "gesture.h"
#include "layout.h"

#include <cmath>

namespace tp {

int classifyDirection(double dx, double dy)
{
    // atan2 with y flipped: 0° = right, 90° = up.
    const double deg = std::atan2(-dy, dx) * 180.0 / M_PI;
    int sector = int(std::lround(deg / 45.0));
    sector = ((sector % 8) + 8) % 8; // 0 R, 1 TR, 2 T, 3 TL, 4 L, 5 BL, 6 B, 7 BR
    static const int toDir[8] = {R, TR, T, TL, L, BL, B, BR};
    return toDir[sector];
}

} // namespace tp
