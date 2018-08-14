#include "Layers.h"

#include <cstring>
#include <utility>

std::ostream& operator<<(std::ostream& stream, const VkLayerProperties& layer) {
    stream << layer.layerName 
        << ": " << layer.description 
        << " v" << layer.implementationVersion
        << ". Spec version " << VkVersion(layer.specVersion)
        << ".";
    return stream;
}
 
 
 
LayerSet::LayerSet(std::vector<VkLayerProperties> layers):
    m_layers(std::move(layers))
{ }
 
bool LayerSet::contains(std::string_view name, uint32_t min_version) const {
    for(int i = 0; i < m_layers.size(); ++i) {
        const auto& props = m_layers[i];
        if(std::strcmp(props.layerName, name.data()) == 0) {
            if(props.implementationVersion >= min_version) {
                return true;
            }
        }
    }
    return false;
}
bool LayerSet::contains_all(const std::vector<const char*>& names) const {
    for(const auto& item: names) {
        if(!contains(item, 0)) {
            return false;
        }
    }
    return true;
}
 
bool LayerSet::contains_all_versioned(const std::vector<std::tuple<const char*, uint32_t>>& names) const {
    for(const auto& item: names) {
        if(!contains(std::get<0>(item), std::get<1>(item))) {
            return false;
        }
    }
    return true;
}

std::vector<const char*> LayerSet::difference(const std::vector<const char*>& names) const {
    std::vector<const char*> difference;
    for(int i = 0; i < names.size(); ++i) {
        if(!contains(names[i], 0)) {
            difference.push_back(names[i]);
        }
    }

    return difference;
}

std::vector<const char*> LayerSet::difference_versioned(const std::vector<std::tuple<const char*, uint32_t>>& names) const {
    std::vector<const char*> difference;
    for(int i = 0; i < names.size(); ++i) {
        const auto& layer = names[i];
        if(!contains(std::get<0>(layer), std::get<1>(layer))) {
            difference.push_back(std::get<0>(layer));
        }
    }

    return difference;
}
