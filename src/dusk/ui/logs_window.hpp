#pragma once

#include "ui.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "dusk/mods/log_buffer.hpp"

#include <borealis/ui/window.hpp>

namespace dusk::ui {

class LogsWindow : public Window {
public:
    explicit LogsWindow(std::string modFilter = {});
    void update() override;

private:
    struct DisplayLine {
        mods::log::Line line;
        int rows = 1;
    };

    void build_content(Rml::Element* content);
    void rebuild_lines();
    void refresh_lines();
    bool line_visible(const mods::log::Line& line) const;
    std::string_view mod_label(const mods::log::Line& line) const;
    int count_rows(const mods::log::Line& line) const;
    void update_window();
    void materialize_range(size_t first, size_t last);
    void clear_elements();
    Rml::Element* create_line_element(const mods::log::Line& line, Rml::Element* before);
    void copy_to_clipboard();

    std::string mModFilter;
    LogLevel mMinLevel = LOG_LEVEL_DEBUG;
    std::vector<std::string> mModIds;
    uint64_t mNextSeq = 0;
    std::vector<mods::log::Line> mScratch;
    std::deque<DisplayLine> mLines;
    std::deque<Rml::Element*> mElems;
    size_t mFirst = 0;
    Rml::Element* mLinesElem = nullptr;
    Rml::Element* mTopSpacer = nullptr;
    Rml::Element* mBottomSpacer = nullptr;
    Rml::Element* mScrollElem = nullptr;
    int mColumns = 0;
    float mRowHeight = 0.0f;
    bool mStickToBottom = true;
    Uint64 mLastRefresh = 0;
};

}  // namespace dusk::ui
