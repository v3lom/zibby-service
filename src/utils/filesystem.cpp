#include "utils/filesystem.h"

#include <boost/filesystem.hpp>

namespace zibby::utils {

std::string normalizePath(const std::string& input) {
    return boost::filesystem::path(input).lexically_normal().string();
}

} // namespace zibby::utils
