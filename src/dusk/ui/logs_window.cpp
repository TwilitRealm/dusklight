#include "logs_window.hpp"

#include <algorithm>
#include <array>
#include <ctime>

#include <RmlUi/Core/ElementUtilities.h>
#include <SDL3/SDL_timer.h>
#include <borealis/ui/pane.hpp>
#include <fmt/format.h>

#include "ui.hpp"

namespace dusk::ui {
namespace {

const char* level_name(LogLevel level) {
    switch (level) {
    case LOG_LEVEL_TRACE:
        return "Trace";
    case LOG_LEVEL_DEBUG:
        return "Debug";
    case LOG_LEVEL_INFO:
        return "Info";
    case LOG_LEVEL_WARN:
        return "Warn";
    case LOG_LEVEL_ERROR:
        return "Error";
    }
    return "?";
}

const char* level_logger_name(LogLevel level) {
    switch (level) {
    case LOG_LEVEL_TRACE:
        return "TRACE";
    case LOG_LEVEL_DEBUG:
        return "DEBUG";
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARN:
        return "WARNING";
    case LOG_LEVEL_ERROR:
        return "ERROR";
    }
    return "?";
}

const char* level_class(LogLevel level) {
    switch (level) {
    case LOG_LEVEL_TRACE:
        return "lvl-trace";
    case LOG_LEVEL_DEBUG:
        return "lvl-debug";
    case LOG_LEVEL_INFO:
        return "lvl-info";
    case LOG_LEVEL_WARN:
        return "lvl-warn";
    case LOG_LEVEL_ERROR:
        return "lvl-error";
    }
    return "lvl-info";
}

std::string format_time(int64_t timeMs) {
    const auto seconds = static_cast<std::time_t>(timeMs / 1000);
    std::tm localTime{};
#if _WIN32
    localtime_s(&localTime, &seconds);
#else
    localtime_r(&seconds, &localTime);
#endif
    std::array<char, 16> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%H:%M:%S", &localTime);
    return fmt::format("{}.{:03}", buffer.data(), timeMs % 1000);
}

Rml::Element* append_log_field(Rml::Element* parent, const char* tagName, const Rml::String& text) {
    auto* field = append(parent, tagName);
    append_text(field, text);
    return field;
}

}  // namespace

LogsWindow::LogsWindow(std::string modFilter)
    : Window{Props{.tabBar = false, .styleSheets = {"res/rml/logs.rcss"}}},
      mModFilter{std::move(modFilter)} {
    mRoot->SetClass("logs", true);
    set_content([this](Rml::Element* content) { build_content(content); });
}

void LogsWindow::build_content(Rml::Element* content) {
    auto* toolbar = append(content, "log-toolbar");

    auto* title = append(toolbar, "log-title");
    append_text(title, "Logs");

    auto* modLabel = append(toolbar, "log-title-mod");
    append_text(modLabel, mModFilter.empty() ? "All mods" : mModFilter);

    append(toolbar, "log-toolbar-spacer");

    for (const LogLevel level :
        {LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR})
    {
        add_child<ControlledButton>(toolbar,
            ControlledButton::Props{
                .text = level_name(level),
                .isSelected = [this, level] { return mMinLevel <= level; },
            })
            .on_pressed([this, level] {
                mMinLevel = level;
                rebuild_lines();
            });
    }

    append(toolbar, "log-toolbar-spacer");

    add_child<Button>(toolbar, "Copy").on_pressed([this] { copy_to_clipboard(); });
    add_child<Button>(toolbar, "Clear").on_pressed([this] {
        mods::log::clear();
        rebuild_lines();
    });

    Rml::ElementList buttons;
    toolbar->QuerySelectorAll(buttons, "button");
    for (auto* button : buttons) {
        button->SetClass("compact", true);
    }

    auto& pane = add_child<Pane>(content, Pane::Type::Uncontrolled);
    pane.root()->SetClass("log-view", true);
    mScrollElem = pane.root();
    mLinesElem = append(pane.root(), "log-lines");
    mTopSpacer = append(mLinesElem, "log-spacer");
    mBottomSpacer = append(mLinesElem, "log-spacer");
    mElems.clear();
    mFirst = 0;

    listen(mScrollElem, Rml::EventId::Scroll, [this](Rml::Event&) {
        const float bottom = mScrollElem->GetScrollHeight() - mScrollElem->GetClientHeight();
        mStickToBottom = mScrollElem->GetScrollTop() >= bottom - 4.0f;
    });

    rebuild_lines();
}

void LogsWindow::update() {
    Window::update();
    if (mLinesElem == nullptr) {
        return;
    }

    const Uint64 perfFreq = SDL_GetPerformanceFrequency();
    const Uint64 now = SDL_GetPerformanceCounter();
    // Limit refreshes to ~8 per second
    const bool refresh =
        perfFreq == 0 || mLastRefresh == 0 ||
        static_cast<double>(now - mLastRefresh) >= 0.125 * static_cast<double>(perfFreq);
    if (refresh) {
        mLastRefresh = now;
        refresh_lines();
    }

    update_window();
}

void LogsWindow::refresh_lines() {
    mScratch.clear();
    const auto [firstSeq, nextSeq] = mods::log::copy_since(mNextSeq, mScratch);
    mNextSeq = nextSeq;

    // Drop displayed lines that fell out of the buffer (ring wrap or clear)
    int droppedRows = 0;
    while (!mLines.empty() && mLines.front().line.seq < firstSeq) {
        droppedRows += mLines.front().rows;
        if (mFirst > 0) {
            --mFirst;
        } else if (!mElems.empty()) {
            mLinesElem->RemoveChild(mElems.front());
            mElems.pop_front();
        }
        mLines.pop_front();
    }
    // Keep the view on the same lines
    if (droppedRows > 0 && !mStickToBottom) {
        mScrollElem->SetScrollTop(mScrollElem->GetScrollTop() - static_cast<float>(droppedRows) * mRowHeight);
    }

    if (mScratch.empty()) {
        return;
    }
    for (const auto& line : mScratch) {
        if (line.modIndex >= mModIds.size()) {
            mModIds = mods::log::ids();
            break;
        }
    }
    for (auto& line : mScratch) {
        if (line_visible(line)) {
            const int rows = count_rows(line);
            mLines.push_back({.line = std::move(line), .rows = rows});
        }
    }
}

void LogsWindow::rebuild_lines() {
    if (mLinesElem == nullptr) {
        return;
    }
    mModIds = mods::log::ids();
    mScratch.clear();
    const auto [_, nextSeq] = mods::log::copy_since(0, mScratch);
    mNextSeq = nextSeq;
    clear_elements();
    mLines.clear();
    for (auto& line : mScratch) {
        if (line_visible(line)) {
            const int rows = count_rows(line);
            mLines.push_back({.line = std::move(line), .rows = rows});
        }
    }
    mStickToBottom = true;
}

bool LogsWindow::line_visible(const mods::log::Line& line) const {
    if (line.level < mMinLevel) {
        return false;
    }
    if (mModFilter.empty()) {
        return true;
    }
    return line.modIndex < mModIds.size() && mModIds[line.modIndex] == mModFilter;
}

std::string_view LogsWindow::mod_label(const mods::log::Line& line) const {
    if (line.source == mods::log::Source::Loader) {
        return "loader";
    }
    if (line.modIndex < mModIds.size()) {
        return mModIds[line.modIndex];
    }
    return "?";
}

int LogsWindow::count_rows(const mods::log::Line& line) const {
    if (mColumns <= 0) {
        return 1;
    }
    const auto segmentRows = [this](int width) { return std::max(1, (width + mColumns - 1) / mColumns); };
    // "HH:MM:SS.mmm [mod] " prefix
    int width = static_cast<int>(mod_label(line).size()) + 16;
    int rows = 0;
    for (const char c : line.message) {
        if (c == '\n') {
            rows += segmentRows(width);
            width = 0;
        } else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++width;
        }
    }
    return rows + segmentRows(width);
}

void LogsWindow::update_window() {
    const float viewHeight = mScrollElem->GetClientHeight();
    const float rowHeight = mLinesElem->GetLineHeight();
    const int charWidth = Rml::ElementUtilities::GetStringWidth(mLinesElem, "0");
    if (viewHeight <= 0.0f || rowHeight <= 0.0f || charWidth <= 0) {
        return;
    }
    mRowHeight = rowHeight;
    int topRow = static_cast<int>((mScrollElem->GetScrollTop() - mLinesElem->GetOffsetTop()) / rowHeight);
    const int columns = std::max(1, static_cast<int>(mLinesElem->GetClientWidth()) / charWidth);
    if (columns != mColumns) {
        size_t topLine = 0;
        int row = 0;
        while (topLine < mLines.size() && row + mLines[topLine].rows <= topRow) {
            row += mLines[topLine++].rows;
        }
        mColumns = columns;
        for (auto& line : mLines) {
            line.rows = count_rows(line.line);
        }
        if (!mStickToBottom && topLine < mLines.size()) {
            const int lineRow = topRow - row;
            topRow = std::min(lineRow, mLines[topLine].rows - 1);
            for (size_t i = 0; i < topLine; ++i) {
                topRow += mLines[i].rows;
            }
            mScrollElem->SetScrollTop(mLinesElem->GetOffsetTop() + static_cast<float>(topRow) * rowHeight);
        }
    }

    int totalRows = 0;
    for (const auto& line : mLines) {
        totalRows += line.rows;
    }
    const int viewRows = static_cast<int>(viewHeight / rowHeight) + 1;
    if (mStickToBottom) {
        topRow = totalRows - viewRows;
    }

    size_t first = 0;
    int firstRow = 0;
    while (first < mLines.size() && firstRow + mLines[first].rows <= topRow - viewRows / 2) {
        firstRow += mLines[first++].rows;
    }
    size_t last = first;
    int lastRow = firstRow;
    while (last < mLines.size() && lastRow < topRow + viewRows + viewRows / 2) {
        lastRow += mLines[last++].rows;
    }
    materialize_range(first, last);

    const auto setHeight = [rowHeight](Rml::Element* spacer, int rows) {
        const Rml::Property value{static_cast<float>(rows) * rowHeight, Rml::Unit::PX};
        const auto* current = spacer->GetLocalProperty(Rml::PropertyId::Height);
        if (current == nullptr || *current != value) {
            spacer->SetProperty(Rml::PropertyId::Height, value);
        }
    };
    setHeight(mTopSpacer, firstRow);
    setHeight(mBottomSpacer, totalRows - lastRow);

    if (mStickToBottom) {
        mScrollElem->SetScrollTop(mScrollElem->GetScrollHeight() - mScrollElem->GetClientHeight());
    }
}

void LogsWindow::materialize_range(size_t first, size_t last) {
    if (last <= mFirst || first >= mFirst + mElems.size()) {
        clear_elements();
        mFirst = first;
    }
    for (; mFirst < first && !mElems.empty(); ++mFirst) {
        mLinesElem->RemoveChild(mElems.front());
        mElems.pop_front();
    }
    while (mFirst + mElems.size() > last) {
        mLinesElem->RemoveChild(mElems.back());
        mElems.pop_back();
    }
    if (mElems.empty()) {
        mFirst = first;
    }
    while (mFirst > first) {
        --mFirst;
        mElems.push_front(create_line_element(mLines[mFirst].line, mElems.empty() ? mBottomSpacer : mElems.front()));
    }
    while (mFirst + mElems.size() < last) {
        mElems.push_back(create_line_element(mLines[mFirst + mElems.size()].line, mBottomSpacer));
    }
}

void LogsWindow::clear_elements() {
    for (auto* elem : mElems) {
        mLinesElem->RemoveChild(elem);
    }
    mElems.clear();
    mFirst = 0;
}

Rml::Element* LogsWindow::create_line_element(const mods::log::Line& line, Rml::Element* before) {
    auto* elem = mLinesElem->InsertBefore(mLinesElem->GetOwnerDocument()->CreateElement("log-line"), before);
    elem->SetClass(level_class(line.level), true);

    constexpr const char* kNbsp = "\xc2\xa0";
    append_log_field(elem, "log-time", format_time(line.timeMs));
    append_text(elem, kNbsp);
    append_log_field(elem, "log-mod", fmt::format("[{}]", mod_label(line)));
    append_text(elem, kNbsp);
    append_log_field(elem, "log-msg", line.message);

    return elem;
}

void LogsWindow::copy_to_clipboard() {
    mModIds = mods::log::ids();
    std::vector<mods::log::Line> lines;
    mods::log::copy_since(0, lines);

    std::string text;
    for (const auto& line : lines) {
        if (!line_visible(line)) {
            continue;
        }
        const std::string_view modId =
            line.modIndex < mModIds.size() ? std::string_view{mModIds[line.modIndex]} : "?";
        text += fmt::format("{} [{}] [{}] {}\n", format_time(line.timeMs),
            level_logger_name(line.level), modId, line.message);
    }
    Rml::GetSystemInterface()->SetClipboardText(text);
    push_toast({.content = "Copied to clipboard", .duration = std::chrono::seconds(2)});
}

}  // namespace dusk::ui
