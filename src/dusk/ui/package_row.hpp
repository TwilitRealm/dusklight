#pragma once

#include "component.hpp"
#include "dusk/mods/queue.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace dusk::ui {

const char* queue_state_class(mods::queue::State state);
std::string state_label(const mods::queue::Item& item);

class PackageRow : public Component {
public:
    explicit PackageRow(Rml::Element* parent);

    void set_package(std::string name, std::string version, std::string status, std::string detail,
        std::string stateClass, std::optional<float> progress = {});
    void set_icon(std::string source);
    Rml::Element* actions_root();

private:
    Rml::Element* mIcon = nullptr;
    Rml::Element* mName = nullptr;
    Rml::Element* mVersion = nullptr;
    Rml::Element* mState = nullptr;
    Rml::Element* mProgress = nullptr;
    Rml::Element* mDetail = nullptr;
    Rml::Element* mFooter = nullptr;
    Rml::Element* mActions = nullptr;
    std::string mIconSource;
};

}  // namespace dusk::ui
