#ifndef EXTENSIONS_H_
#define EXTENSIONS_H_

#include <ostream>
#include <vector>

#include <vulkan/vulkan.h>

class ExtensionSet {
public:
    using iterator = std::vector<VkExtensionProperties>::iterator;
    using const_iterator = std::vector<VkExtensionProperties>::const_iterator;

    ExtensionSet();
    ExtensionSet(std::vector<VkExtensionProperties> extensions);
    ~ExtensionSet() = default;

    ExtensionSet(const ExtensionSet& other) = default;
    ExtensionSet(ExtensionSet&& other) noexcept = default;
    ExtensionSet& operator =(const ExtensionSet& other) = default;
    ExtensionSet& operator =(ExtensionSet&& other) noexcept = default;

    bool empty() const { return m_extensions.empty(); }
    std::size_t count() const { return m_extensions.size(); }

    bool contains(const char* name) const;
    bool contains_all(const std::vector<const char*>& names) const;
    std::vector<const char*> difference(const std::vector<const char*>& names) const;

    const_iterator begin() const { return m_extensions.begin(); }
    const_iterator end() const { return m_extensions.end(); }
    const_iterator cbegin() const { return m_extensions.cbegin(); }
    const_iterator cend() const { return m_extensions.cend(); }

    static ExtensionSet get_instance_extensions();

private:
    std::vector<VkExtensionProperties> m_extensions;
};

std::ostream& operator <<(std::ostream& stream, const VkExtensionProperties& extension);

#endif
