#ifndef VERSION_H_
#define VERSION_H_

#include <cstdint>
#include <ostream>

#include <vulkan/vulkan.h>

class VkVersion {
public:
    VkVersion(uint32_t raw_value): raw_version(raw_value) {}
    VkVersion(uint16_t major, uint16_t minor, uint16_t patch): 
        raw_version(VK_MAKE_VERSION(major, minor, patch)) {}
    uint32_t raw_version;

    uint16_t major_version() const { return (raw_version >> 22); }
    uint16_t minor_version() const { return (raw_version >> 12) & 0x2FF; }
    uint16_t patch() const { return raw_version & 0x8FF; }
};

inline std::ostream& operator <<(std::ostream& stream, VkVersion version) {
    stream << version.major_version() << "." << version.minor_version() << "." << version.patch();
    return stream;
}

#endif
