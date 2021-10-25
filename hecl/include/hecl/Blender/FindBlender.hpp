#pragma once

#include "hecl/hecl.hpp"

namespace hecl::blender {
constexpr uint32_t MinBlenderMajorSearch = 2;
constexpr uint32_t MaxBlenderMajorSearch = 2;
constexpr uint32_t MinBlenderMinorSearch = 83;
constexpr uint32_t MaxBlenderMinorSearch = 93;

std::optional<std::string> FindBlender(int& major, int& minor);

} // namespace hecl::blender
