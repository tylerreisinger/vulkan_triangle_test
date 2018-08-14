#include "Extensions.h"

#include <cstring>

#include "Version.h"


ExtensionSet::ExtensionSet(std::vector<VkExtensionProperties> extensions):
    m_extensions(std::move(extensions))
{ }
 
bool ExtensionSet::contains(const char* name) const {
    for(std::size_t i = 0; i < m_extensions.size(); ++i) {
        const auto& extension = m_extensions[i];
        if(std::strcmp(extension.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}
 
bool ExtensionSet::contains_all(const std::vector<const char*>& names) const {
    for(std::size_t i = 0; i < names.size(); ++i) {
        const auto& name = names[i];
        if(!contains(name)) {
            return false;
        }
    } 
    return true;
}

std::vector<const char*> ExtensionSet::difference(const std::vector<const char*>& names) const {
    std::vector<const char*> output;
    for(std::size_t i = 0; i < names.size(); ++i) {
        const auto& name = names[i];
        bool found = false;
        std::size_t j = 0;
        for(j = 0; j < m_extensions.size(); ++j) {
            const auto& extension = m_extensions[j];
            if(std::strcmp(name, extension.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if(!found) {
            output.push_back(m_extensions[j].extensionName);
        }
    }
    return output;
}
ExtensionSet ExtensionSet::get_instance_extensions() {
    uint32_t property_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &property_count, nullptr);
    std::vector<VkExtensionProperties> extensions(property_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &property_count, extensions.data());

    return ExtensionSet(extensions);
}
 
std::ostream& operator<<(std::ostream& stream, const VkExtensionProperties& extension) {
    stream << extension.extensionName << ". Spec version " << VkVersion(extension.specVersion);
    return stream;
}
 
