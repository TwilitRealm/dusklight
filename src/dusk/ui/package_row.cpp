#include "package_row.hpp"

#include "fmt/format.h"

namespace dusk::ui {
namespace {

Rml::Element* create_row(Rml::Element* parent) {
    auto element = parent->GetOwnerDocument()->CreateElement("package-row");
    return parent->AppendChild(std::move(element));
}

}  // namespace

const char* queue_state_class(mods::queue::State state) {
    using enum mods::queue::State;
    switch (state) {
    case Downloading:
        return "downloading";
    case Paused:
        return "paused";
    case Retrying:
        return "retrying";
    case Verifying:
        return "installing";
    case Handoff:
        return "installing";
    case Installed:
        return "installed";
    case InstallFailed:
        return "failed";
    case Failed:
        return "failed";
    case Canceled:
        return "canceled";
    case Queued:
        return "queued";
    }
    return "queued";
}

std::string state_label(const mods::queue::Item& item) {
    using enum mods::queue::State;
    switch (item.state) {
    case Queued:
        return "Queued";
    case Downloading:
        return "Downloading";
    case Paused:
        return "Paused";
    case Retrying:
        return fmt::format("Retrying in {}s", item.retrySeconds);
    case Verifying:
        return "Verifying";
    case Handoff:
        return "Installing";
    case Installed:
        return "Installed";
    case InstallFailed:
        return "Failed";
    case Failed:
        return "Failed";
    case Canceled:
        return "Canceled";
    }
    return {};
}

PackageRow::PackageRow(Rml::Element* parent) : Component{create_row(parent)} {
    mIcon = append(mRoot, "mod-icon");
    auto* info = append(mRoot, "section");
    auto* heading = append(info, "header");
    auto* identity = append(heading, "h3");
    mName = append(identity, "span");
    mVersion = append(identity, "small");
    mState = append(heading, "small");
    mProgress = append(info, "progress");
    mFooter = append(info, "footer");
    mDetail = append(mFooter, "small");
}

void PackageRow::set_package(std::string name, std::string version, std::string status,
    std::string detail, std::string stateClass, std::optional<float> progress) {
    mRoot->SetClassNames(stateClass);
    set_text_content(mName, name);
    set_text_content(mVersion, fmt::format("v{}", version));
    mVersion->SetProperty("display", version.empty() ? "none" : "block");
    set_text_content(mState, status);
    set_text_content(mDetail, detail);
    if (progress) {
        mProgress->SetProperty("display", "block");
        mProgress->SetAttribute("value", *progress);
    } else {
        mProgress->SetProperty("display", "none");
    }
}

void PackageRow::set_icon(std::string source) {
    mIcon->SetClass("visible", true);
    if (mIconSource == source) {
        return;
    }
    mIconSource = std::move(source);
    if (mIconSource.empty()) {
        mIcon->RemoveProperty("decorator");
        mIcon->SetClass("has-image", false);
        return;
    }
    mIcon->SetProperty(
        "decorator", fmt::format(R"(image("{}" cover center center))", escape(mIconSource)));
    mIcon->SetClass("has-image", true);
}

Rml::Element* PackageRow::actions_root() {
    if (mActions == nullptr) {
        mActions = append(mFooter, "nav");
    }
    return mActions;
}

}  // namespace dusk::ui
