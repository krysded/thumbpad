#pragma once

namespace tp {

// Maps a drag vector (screen coordinates, y grows downwards) to a Dir.
// Each direction owns a 45° sector centred on its axis.
int classifyDirection(double dx, double dy);

} // namespace tp
