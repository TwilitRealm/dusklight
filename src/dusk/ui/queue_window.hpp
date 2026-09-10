#pragma once

#include "modal.hpp"

#include <string>
#include <vector>

namespace dusk::ui {

class QueueWindow final : public Modal {
public:
    explicit QueueWindow(std::string focusId = {});

    void update() override;

private:
    void refresh_queue();

    std::string mFocusId;
    std::vector<std::string> mItemIds;
};

}  // namespace dusk::ui
