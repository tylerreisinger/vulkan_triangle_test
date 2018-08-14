#ifndef LAYERS_H_
#define LAYERS_H_

#include <string_view>
#include <ostream>
#include <tuple>
#include <vector>

#include <vulkan/vulkan.h>

#include "Version.h"

class LayerSet {
public:
    using iterator = std::vector<VkLayerProperties>::iterator;
    using const_iterator = std::vector<VkLayerProperties>::const_iterator;

    LayerSet();
    LayerSet(std::vector<VkLayerProperties> properties);
    ~LayerSet() = default;

    LayerSet(const LayerSet& other) = default;
    LayerSet(LayerSet&& other) noexcept = default;
    LayerSet& operator =(const LayerSet& other) = default;
    LayerSet& operator =(LayerSet&& other) noexcept = default;

    bool empty() const { return m_layers.empty(); }
    std::size_t count() const { return m_layers.size(); }

    bool contains(std::string_view name, uint32_t min_version = 0) const;
    bool contains_all(const std::vector<const char*>& names) const;
    bool contains_all_versioned(const std::vector<std::tuple<const char*, uint32_t>>& names) const;
    std::vector<const char*> difference(const std::vector<const char*>& names) const;
    std::vector<const char*> difference_versioned(const std::vector<std::tuple<const char*, uint32_t>>& names) const;

    const_iterator begin() const { return m_layers.begin(); }
    const_iterator end() const { return m_layers.end(); }
    const_iterator cbegin() const { return m_layers.cbegin(); }
    const_iterator cend() const { return m_layers.cend(); }

private:
    std::vector<VkLayerProperties> m_layers;
};

std::ostream& operator <<(std::ostream& stream, const VkLayerProperties& layer);


#endif
