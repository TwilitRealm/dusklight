#pragma once

#include "dusk/mods/catalog.hpp"
#include "window.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace dusk::ui {

class ModBrowser final : public Window {
public:
    ModBrowser();

    void update() override;

private:
    enum class State {
        Loading,
        Ready,
        Error,
        Unavailable,
    };
    enum class FocusTarget {
        Default,
        Search,
        Category,
        Sort,
        Device,
        Results,
        Retry,
    };

    void build_content(Rml::Element* content);
    void begin_fetch(FocusTarget focusTarget);
    void finish_fetch(mods::catalog::FetchResult result);
    void cycle_category();
    void cycle_sort();

    mods::catalog::Query mQuery;
    std::optional<mods::catalog::Page> mPage;
    borealis::Task<mods::catalog::FetchResult> mFetch;
    std::string mError;
    State mState = State::Loading;
    FocusTarget mFocusTarget = FocusTarget::Default;
    uint64_t mLoaderGeneration = 0;
    bool mRebuildRequested = false;
};

}  // namespace dusk::ui
