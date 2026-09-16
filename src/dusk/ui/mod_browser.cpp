#include "mod_browser.hpp"

#include <borealis/update.hpp>
#include "bool_button.hpp"
#include "button.hpp"
#include "dropdown_button.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/loader/packages.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/svc/registry.hpp"
#include "dusk/mods/updates.hpp"
#include "fmt/format.h"
#include "format.hpp"
#include "icon_button.hpp"
#include "mod_updates.hpp"
#include "mods_window.hpp"
#include "nav_group.hpp"
#include "package_row.hpp"
#include "remote_texture_provider.hpp"
#include "string_button.hpp"

#include <SDL3/SDL_misc.h>
#include <borealis/http.hpp>
#include <tracy/Tracy.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <ranges>
#include <string_view>

namespace dusk::ui {
namespace {

constexpr bool kEnableEndorsements = false;

struct SortOption {
    mods::catalog::Sort value;
    std::string_view label;
};

constexpr std::array sortOptions{
    SortOption{mods::catalog::Sort::Featured, "Featured"},
    SortOption{mods::catalog::Sort::Updated, "Recently updated"},
    SortOption{mods::catalog::Sort::Downloads, "Most downloaded"},
    // SortOption{mods::catalog::Sort::Endorsements, "Most endorsed"},
    SortOption{mods::catalog::Sort::Newest, "Newest"},
    SortOption{mods::catalog::Sort::Name, "Name"},
};

std::string_view sort_label(mods::catalog::Sort sort) noexcept {
    const auto iter = std::ranges::find(sortOptions, sort, &SortOption::value);
    return iter != sortOptions.end() ? iter->label : sortOptions.front().label;
}

void add_list_markers(Rml::Element* fragment) {
    Rml::ElementList lists;
    fragment->QuerySelectorAll(lists, "ul, ol");

    for (auto* list : lists) {
        const bool ordered = list->GetTagName() == "ol";
        int ordinal = list->GetAttribute("start", 1);
        for (int index = 0; index < list->GetNumChildren(); ++index) {
            auto* item = list->GetChild(index);
            if (item->GetTagName() != "li") {
                continue;
            }

            auto marker = item->GetOwnerDocument()->CreateElement("catalog-list-marker");
            Rml::Element* insertedMarker = nullptr;
            if (auto* firstChild = item->GetFirstChild()) {
                insertedMarker = item->InsertBefore(std::move(marker), firstChild);
            } else {
                insertedMarker = item->AppendChild(std::move(marker));
            }
            append_text(insertedMarker, ordered ? fmt::format("{}.", ordinal) : "\u2022");
            if (ordered) {
                ++ordinal;
            }
        }
    }
}

std::string image_source(const mods::catalog::Image& image, uint32_t preferredWidth) {
    const auto wider = std::ranges::find_if(image.sources,
        [preferredWidth](const auto& source) { return source.width >= preferredWidth; });
    if (wider != image.sources.end()) {
        return wider->pngUrl;
    }
    return image.sources.empty() ? std::string{} : image.sources.back().pngUrl;
}

std::optional<mods::queue::Icon> queue_icon(const std::optional<mods::catalog::Image>& image) {
    if (!image) {
        return std::nullopt;
    }
    auto source = image_source(*image, 128);
    if (source.empty()) {
        return std::nullopt;
    }
    return mods::queue::Icon{
        .url = std::move(source),
        .width = image->width,
        .height = image->height,
    };
}

void set_image(Rml::Element* element, const mods::catalog::Image& image, uint32_t preferredWidth,
    std::string_view fit = "cover") {
    if (element == nullptr) {
        return;
    }
    auto source = image_source(image, preferredWidth);
    if (!source.empty()) {
        source = remote_image_source(source, image.width, image.height);
        element->SetProperty(
            "decorator", fmt::format(R"(image("{}" {} center center))", escape(source), fit));
        element->SetClass("has-image", true);
    }
}

std::string_view activation_failure(const mods::LoadedMod& mod) {
    if (!mod.failureReason.empty()) {
        return mod.failureReason;
    }
    return mod.suspendedByProvider ? "A required provider is unavailable" : "Activation failed";
}

void open_web_url(const std::string& url) {
    if (url.starts_with("https://")) {
        SDL_OpenURL(url.c_str());
    }
}

void set_icon_button_content(Button& button, std::string_view icon, const Rml::String& label) {
    clear_children(button.root());
    append_text(append(button.root(), "icon"), material_icon(icon));
    append_text_element(button.root(), "span", label);
}

void append_status(Rml::Element* parent, const Rml::String& title, const Rml::String& message) {
    append_text_element(parent, "h2", title);
    append_text_element(parent, "p", message);
}

void append_stat(Rml::Element* parent, std::string_view icon, const Rml::String& value,
    const Rml::String& suffix) {
    auto* stat = append(parent, "stat");
    if (!icon.empty()) {
        append_text(append(stat, "icon"), material_icon(icon));
    }
    append_text_element(stat, "span", value + suffix);
}

void append_detail_field(Rml::Element* list, const Rml::String& label, const Rml::String& value) {
    append_text_element(list, "dt", label);
    append_text_element(list, "dd", value);
}

class ModBrowserDetail;

class CatalogCard final : public Button {
public:
    CatalogCard(Rml::Element* parent, const mods::catalog::Mod& mod, std::function<void()> onOpen)
        : Button{parent, Props{}}, mId{mod.id}, mPackageSize{format_bytes(mod.packageSize)} {
        mRoot->SetClass("catalog-card", true);
        mRoot->SetAttribute("mod-id", mod.id);
        const auto category = mod.category ? mod.category->name : "Uncategorized";

        auto* art = append(mRoot, "catalog-card-art");
        auto* artImage = append(art, "catalog-card-art-image");
        auto* icon = append(art, "mod-icon");
        auto* iconImage = append(icon, "mod-icon-image");

        auto* body = append(mRoot, "catalog-card-body");
        append_text_element(body, "small", relative_date(mod.updatedAt));
        auto* identity = append(body, "section");
        append_text_element(identity, "b", category);
        append_text_element(identity, "h2", mod.name);
        append_text_element(identity, "small", fmt::format("by {}", mod.author.name));
        append_text_element(body, "p", snippet(mod.summary, 126));
        auto* meta = append(body, "footer");
        auto* downloads = append(meta, "stat");
        append_text(append(downloads, "icon"), material_icon("download"));
        append_text_element(downloads, "span", format_count(mod.downloads));
        if constexpr (kEnableEndorsements) {
            auto* endorsements = append(meta, "stat");
            append_text(append(endorsements, "icon"), material_icon("favorite"));
            append_text_element(endorsements, "span", format_count(mod.endorsements));
        }
        mStatus = append(meta, "small");
        mStatus->SetClass("size", true);

        if (mod.banner) {
            set_image(artImage, *mod.banner, 640);
        } else if (mod.icon) {
            set_image(artImage, *mod.icon, 256);
        }
        if (mod.icon) {
            set_image(iconImage, *mod.icon, 128);
        }
        on_pressed(std::move(onOpen));
        update();
    }

    void update() override {
        const bool installed = mods::ModLoader::instance().find_mod(mId) != nullptr;
        const auto* update = mods::updates::find(mId);
        const bool hasUpdate = installed && update && update->actionable;
        const auto label = hasUpdate ? "Update available" : installed ? "Installed" : mPackageSize;
        if (mLabel != label) {
            set_text_content(mStatus, label);
            mStatus->SetClass("installed", installed && !hasUpdate);
            mStatus->SetClass("update-available", hasUpdate);
            mLabel = label;
        }
        Button::update();
    }

private:
    std::string mId;
    std::string mPackageSize;
    std::string mLabel;
    Rml::Element* mStatus = nullptr;
};

class ScreenshotViewer final : public Window {
public:
    ScreenshotViewer(std::vector<mods::catalog::Screenshot> screenshots, size_t index)
        : Window{Props{
              .tabBar = false,
              .styleSheets = {"res/rml/mod_browser.rcss"},
          }},
          mScreenshots{std::move(screenshots)}, mIndex{index} {
        mRoot->SetClass("screenshot-viewer", true);
        set_content([this](Rml::Element* content) { build_content(content); });
    }

    void update() override {
        if (mRebuildRequested) {
            mRebuildRequested = false;
            rebuild_content();
        }
        Window::update();
    }

private:
    void build_content(Rml::Element* content) {
        auto* image = append(content, "catalog-screenshot-full");
        if (mIndex < mScreenshots.size()) {
            set_image(image, mScreenshots[mIndex].image, 1280, "contain");
        }

        auto* actionsRoot = append(content, "catalog-screenshot-actions");
        auto& actions =
            add_child<NavGroup>(actionsRoot, NavGroup::Props{
                                                 .layout = NavGroup::Layout::Horizontal,
                                                 .horizontalBoundary = NavGroup::Boundary::Stop,
                                                 .verticalBoundary = NavGroup::Boundary::Stop,
                                             });
        auto& back = actions.add_item<Button>("Back");
        back.root()->SetClass("catalog-icon-action", true);
        set_icon_button_content(back, "arrow_back", "Back");
        back.on_pressed([this] { pop(); });
        auto& previous = actions.add_item<ControlledButton>(ControlledButton::Props{
            .text = "Previous",
            .isDisabled = [this] { return mIndex == 0; },
        });
        previous.on_pressed([this] {
            if (mIndex > 0) {
                --mIndex;
                mRestoreNav = -1;
                mRebuildRequested = true;
            }
        });
        actions.add_item<ControlledButton>(ControlledButton::Props{
            .text = fmt::format("{} / {}", mIndex + 1, mScreenshots.size()),
            .isDisabled = [] { return true; },
        });
        auto& next = actions.add_item<ControlledButton>(ControlledButton::Props{
            .text = "Next",
            .isDisabled = [this] { return mIndex + 1 >= mScreenshots.size(); },
        });
        next.on_pressed([this] {
            if (mIndex + 1 < mScreenshots.size()) {
                ++mIndex;
                mRestoreNav = 1;
                mRebuildRequested = true;
            }
        });
        if (mRestoreNav < 0) {
            if (!previous.focus()) {
                next.focus();
            }
        } else if (mRestoreNav > 0) {
            if (!next.focus()) {
                previous.focus();
            }
        }
        mRestoreNav = 0;
    }

    std::vector<mods::catalog::Screenshot> mScreenshots;
    size_t mIndex = 0;
    bool mRebuildRequested = false;
    int mRestoreNav = 0;
};

class DetailContent final : public NavGroup {
public:
    DetailContent(
        Rml::Element* root, ModBrowserDetail& window, const mods::catalog::Detail& detail);
};

class ScrollAnchor final : public Component {
public:
    using Component::Component;
};

class ModBrowserDetail final : public Window {
public:
    explicit ModBrowserDetail(mods::catalog::Mod mod)
        : Window{Props{.tabBar = false, .styleSheets = {"res/rml/mod_browser.rcss"}}},
          mSummary{std::move(mod)} {
        mRoot->SetClass("mod-browser-detail", true);
        set_content([this](Rml::Element* content) { build_content(content); });
        begin_fetch();
    }

    void update() override {
        ZoneScopedN("Mod browser detail update");
        if (mFetch && mFetch.ready()) {
            try {
                if (auto result = mFetch.try_take()) {
                    if (result->detail) {
                        mDetail = std::move(result->detail);
                        mError.clear();
                    } else {
                        mError = result->error.empty() ? "The mod request failed." :
                                                         std::move(result->error);
                    }
                }
            } catch (const std::exception& exception) {
                mError = fmt::format("The mod request failed: {}", exception.what());
            } catch (...) {
                mError = "The mod request failed.";
            }
            mFetch = {};
            mRebuildRequested = true;
        }
        if (mRebuildRequested) {
            mRebuildRequested = false;
            rebuild_content();
        }
        Window::update();
    }

    void show_screenshot(size_t index) {
        if (mDetail && index < mDetail->screenshots.size()) {
            push(std::make_unique<ScreenshotViewer>(mDetail->screenshots, index));
        }
    }

    void show_downloads(const std::string& id) { show_online_mods(id); }

private:
    void begin_fetch() {
        mDetail.reset();
        mError.clear();
        mFetch = mods::catalog::fetch_detail(mSummary.id);
        mRebuildRequested = true;
    }

    void build_content(Rml::Element* content) {
        if (mDetail) {
            auto* scroll = append(content, "detail-scroll");
            add_child<DetailContent>(scroll, *this, *mDetail).focus();
            return;
        }

        auto* status = append(content, "catalog-detail-status");
        if (mError.empty()) {
            append_status(status, fmt::format("Loading {}", mSummary.name),
                "Fetching mod details and images...");
            return;
        }
        append_status(status, fmt::format("Could not load {}", mSummary.name), mError);
        auto* retryRoot = append(status, "catalog-retry-actions");
        auto& retry = add_child<NavGroup>(retryRoot, NavGroup::Props{});
        retry.add_item<Button>("Retry").on_pressed([this] { begin_fetch(); });
    }

    mods::catalog::Mod mSummary;
    std::optional<mods::catalog::Detail> mDetail;
    borealis::Task<mods::catalog::DetailFetchResult> mFetch;
    std::string mError;
    bool mRebuildRequested = false;
};

class CatalogInstallButton final : public Button {
public:
    CatalogInstallButton(
        Rml::Element* parent, ModBrowserDetail& window, const mods::catalog::Detail& detail)
        : Button{parent, Props{}}, mWindow{window}, mRequest{
                                                        .id = detail.mod.id,
                                                        .name = detail.mod.name,
                                                        .version = detail.mod.version,
                                                        .source = detail.download,
                                                        .icon = queue_icon(detail.mod.icon),
                                                    } {
        mRoot->SetClass("catalog-install-action", true);
        mCaption = append(parent, "catalog-install-caption");
        on_pressed([this] { press(); });
        update();
    }

    void update() override {
        auto queued = matching_queue_item();
        const auto* local = mods::ModLoader::instance().find_mod(mRequest.id);
        const bool activationPending =
            mActivationOperation != nullptr &&
            mActivationOperation->state == mods::ModOperation::State::Pending;
        if (mActivationOperation != nullptr && !activationPending) {
            mActivationOperation.reset();
        }
        std::string_view icon = "file_download";
        std::string label;
        std::string caption = format_bytes(package_size());
        std::string state = "idle";
        float progress = 0.0f;
        bool disabled = false;
        mAction = Action::Install;

        if (queued && queued->state != mods::queue::State::Canceled) {
            using enum mods::queue::State;
            mAction = Action::OpenQueue;
            mQueueId = queued->id;
            state = queue_state_class(queued->state);
            progress = queued->total == 0 ? 0.0f :
                                            std::clamp(static_cast<float>(queued->completed) /
                                                           static_cast<float>(queued->total),
                                                0.0f, 1.0f);
            switch (queued->state) {
            case Queued:
                icon = "schedule";
                label = "Queued";
                if (const auto ahead = mods::queue::active_items_ahead(mRequest.id); ahead != 0) {
                    caption = fmt::format("{} ahead · view downloads", ahead);
                } else {
                    caption = "Next · view downloads";
                }
                break;
            case Downloading:
                label = fmt::format(
                    "{} / {}", format_bytes(queued->completed), format_bytes(queued->total));
                caption = "View downloads & installs";
                break;
            case Paused:
                mAction = Action::Resume;
                icon = "play_arrow";
                label = "Resume";
                caption = fmt::format("{} kept on disk", format_bytes(queued->completed));
                break;
            case Retrying:
                icon = "warning";
                label = fmt::format("Retrying in {}s", queued->retrySeconds);
                caption = "Network error · keeps retrying itself";
                break;
            case Verifying:
                icon = "schedule";
                label = "Verifying…";
                caption = "Checking package integrity";
                break;
            case Handoff:
                icon = "schedule";
                label = "Installing…";
                caption = "Applying package";
                progress = 1.0f;
                disabled = true;
                break;
            case Installed:
            case InstallFailed:
                break;
            case Failed:
                mAction = Action::RetryDownload;
                icon = "refresh";
                label = queued->local ? "Retry package" : "Retry download";
                caption = queued->message.empty() ? "Package preparation failed" : queued->message;
                progress = 1.0f;
                break;
            case Canceled:
                break;
            }
        }

        if (label.empty()) {
            mAction = Action::Install;
            const int versionOrder =
                local != nullptr ?
                    mods::compare_package_versions(mRequest.version, local->metadata.version) :
                    1;
            const bool current = local != nullptr && versionOrder == 0;
            const bool updateable = local != nullptr && versionOrder > 0 &&
                                    mods::ModLoader::instance().can_update(*local);
            if (activationPending) {
                icon = "schedule";
                label = "Activating…";
                caption = "Retrying mod activation";
                state = "installing";
                progress = 1.0f;
                disabled = true;
            } else if (current && local->activation_failed()) {
                mAction = Action::RetryActivation;
                icon = "refresh";
                label = "Retry activation";
                caption = activation_failure(*local);
                state = "failed";
                progress = 1.0f;
            } else if (current || (local != nullptr && !updateable)) {
                mAction = Action::OpenManager;
                icon = "check_circle";
                label = "Installed";
                caption = fmt::format("Installed · {} · {}", format_bytes(package_size()),
                    local != nullptr && local->active ? "enabled" : "disabled");
                state = "installed";
                progress = 1.0f;
            } else {
                label = "Install";
                if (updateable) {
                    const auto* update = mods::updates::find(mRequest.id);
                    if (!borealis::update::parse_version(local->metadata.version)) {
                        label = "Unavailable";
                        caption = "The installed version cannot be compared.";
                        disabled = true;
                    } else if (update && !update->queueKey.empty()) {
                        label = "View download";
                        mAction = Action::OpenQueue;
                        mQueueId = update->queueKey;
                    } else if (update && update->result.target && update->actionable) {
                        const bool sameVersion = update->result.target->version == mRequest.version;
                        label = sameVersion ? "Update" : "View compatible update";
                        mAction = sameVersion ? Action::Update : Action::OpenUpdates;
                    } else if (update && !update->reason.empty() &&
                               mods::updates::state() == mods::updates::State::Ready)
                    {
                        label = "Unavailable";
                        caption = update->reason;
                        disabled = true;
                    } else {
                        const bool checking =
                            mods::updates::state() == mods::updates::State::Checking;
                        label = checking ? "Checking…" : "Check for updates";
                        mAction = Action::CheckUpdates;
                        disabled = checking;
                    }
                }
            }
        }
        if (disabled) {
            mAction = Action::None;
        }

        if (mLabel != label || mIcon != icon) {
            ui::clear_children(mRoot);
            append_text(append(mRoot, "icon"), material_icon(icon));
            append_text_element(mRoot, "span", label);
            mProgress = append(mRoot, "progress");
            mLabel = std::move(label);
            mIcon = icon;
        }
        set_text_content(mCaption, caption);
        for (const auto* candidate : {"idle", "queued", "downloading", "paused", "retrying",
                 "installing", "installed", "failed"})
        {
            mRoot->SetClass(candidate, state == candidate);
            mCaption->SetClass(candidate, state == candidate);
        }
        if (mProgress != nullptr) {
            mProgress->SetAttribute("value", progress);
            set_display(mProgress, state == "idle" || state == "installed" ?
                                       Rml::Style::Display::None :
                                       Rml::Style::Display::Block);
        }
        set_disabled(disabled);
        Button::update();
    }

private:
    enum class Action {
        Update,
        CheckUpdates,
        OpenUpdates,
        Install,
        OpenQueue,
        Resume,
        RetryDownload,
        RetryActivation,
        OpenManager,
        None
    };

    std::optional<mods::queue::Item> matching_queue_item() const {
        auto item = mods::queue::find_by_mod_id(mRequest.id);
        if (!item || item->version != mRequest.version ||
            mods::queue::is_install_result(item->state))
        {
            return std::nullopt;
        }
        return item;
    }

    void press() {
        update();
        switch (mAction) {
        case Action::Update:
            enqueue_mod_update(mRequest.id);
            return;
        case Action::CheckUpdates:
            mods::updates::request_check();
            return;
        case Action::OpenUpdates:
            show_online_mods();
            return;
        case Action::OpenQueue:
            mWindow.show_downloads(mQueueId);
            return;
        case Action::Resume:
            mods::queue::resume(mQueueId);
            return;
        case Action::RetryDownload:
            mods::queue::retry(mQueueId);
            return;
        case Action::RetryActivation:
            mActivationOperation = mods::ModLoader::instance().request_reactivate(mRequest.id);
            return;
        case Action::OpenManager:
            pop_to_or_push<ModsWindow>(
                [id = mRequest.id](ModsWindow& window) { window.select_mod(id); });
            return;
        case Action::None:
            return;
        case Action::Install:
            break;
        }
        if (!mods::queue::enqueue(mRequest)) {
            push_toast({
                .type = "warning",
                .title = "Could not start download",
                .content = "The catalog download descriptor is invalid.",
                .duration = std::chrono::seconds{5},
            });
        }
    }

    ModBrowserDetail& mWindow;
    mods::queue::Request mRequest;
    Rml::Element* mCaption = nullptr;
    Rml::Element* mProgress = nullptr;
    std::string mLabel;
    std::string mIcon;
    std::string mQueueId;
    Action mAction = Action::None;
    mods::ModOperationHandle mActivationOperation;

    uint64_t package_size() const { return std::get<mods::Download>(mRequest.source).size; }
};

DetailContent::DetailContent(
    Rml::Element* root, ModBrowserDetail& window, const mods::catalog::Detail& detail)
    : NavGroup{root, Props{
                         .layout = Layout::Vertical,
                         .horizontalBoundary = Boundary::Stop,
                         .verticalBoundary = Boundary::Stop,
                     }} {
    auto* hero = append(mRoot, "catalog-detail-hero");
    auto* heroImage = append(hero, "catalog-detail-hero-image");
    if (detail.mod.banner) {
        set_image(heroImage, *detail.mod.banner, 1280);
    } else if (detail.mod.icon) {
        set_image(heroImage, *detail.mod.icon, 512);
    }

    auto* actionsRoot = append(hero, "catalog-detail-actions");
    auto& actions =
        add_existing_item<NavGroup>(actionsRoot, Props{
                                                     .layout = Layout::Horizontal,
                                                     .horizontalBoundary = Boundary::Bubble,
                                                     .verticalBoundary = Boundary::Bubble,
                                                 });
    auto& back = actions.add_item<Button>("Back");
    back.root()->SetClass("catalog-icon-action", true);
    set_icon_button_content(back, "arrow_back", "Back");
    back.on_pressed([&window] { window.pop(); });
    auto& open = actions.add_item<Button>("Open in browser");
    open.root()->SetClass("catalog-icon-action", true);
    set_icon_button_content(open, "open_in_new", "Open in browser");
    open.on_pressed([url = detail.siteUrl] { open_web_url(url); });

    auto* identity = append(hero, "catalog-detail-identity");
    auto* detailIcon = append(identity, "mod-icon");
    auto* detailIconImage = append(detailIcon, "mod-icon-image");
    auto* detailHeading = append(identity, "header");
    append_text_element(
        detailHeading, "b", detail.mod.category ? detail.mod.category->name : "Uncategorized");
    auto* title = append(detailHeading, "h1");
    append_text(title, detail.mod.name);
    append_text_element(title, "small", fmt::format("v{}", detail.mod.version));
    auto* author = append(detailHeading, "p");
    append_text(author, fmt::format("by {} ", detail.mod.author.name));
    if (detail.mod.author.official) {
        append_text_element(author, "catalog-official-badge", "Official");
    }
    if (detail.mod.icon) {
        set_image(detailIconImage, *detail.mod.icon, 256);
    }
    auto* installRoot = append(identity, "catalog-install-control");
    auto& installControl =
        add_existing_item<NavGroup>(installRoot, Props{
                                                     .layout = Layout::Vertical,
                                                     .horizontalBoundary = Boundary::Bubble,
                                                     .verticalBoundary = Boundary::Bubble,
                                                 });
    installControl.add_item<CatalogInstallButton>(window, detail);

    auto* stats = append(mRoot, "catalog-detail-stats");
    append_stat(stats, "download", format_count(detail.mod.downloads), " downloads");
    if constexpr (kEnableEndorsements) {
        append_stat(stats, "favorite", format_count(detail.mod.endorsements), " endorsements");
    }

    auto* body = append(mRoot, "catalog-detail-body");
    auto* main = append(body, "main");
    auto* sidebar = append(body, "aside");

    auto* description = append(main, "section");
    description->SetClass("catalog-scroll-anchor", true);
    add_existing_item<ScrollAnchor>(description);
    auto* descriptionFragment = append(description, "catalog-fragment");
    if (detail.descriptionHtml.empty()) {
        append_text_element(descriptionFragment, "p", "No description provided.");
    } else {
        descriptionFragment->SetInnerRML(detail.descriptionHtml);
        add_list_markers(descriptionFragment);
    }

    if (!detail.screenshots.empty()) {
        auto* section = append(main, "section");
        append_text(append(section, "h2"), "Screenshots");
        auto* galleryRoot = append(section, "catalog-gallery");
        auto& gallery =
            add_existing_item<NavGroup>(galleryRoot, Props{
                                                         .layout = Layout::Horizontal,
                                                         .horizontalBoundary = Boundary::Bubble,
                                                         .verticalBoundary = Boundary::Bubble,
                                                     });
        const size_t shown = std::min<size_t>(detail.screenshots.size(), 3);
        for (size_t index = 0; index < shown; ++index) {
            auto& screenshot = gallery.add_item<Button>(Button::Props{});
            screenshot.root()->SetClass("catalog-screenshot", true);
            screenshot.root()->SetClass("primary", index == 0);
            auto* image = append(screenshot.root(), "catalog-screenshot-image");
            set_image(image, detail.screenshots[index].image, index == 0 ? 1280 : 640);
            if (index == 2 && detail.screenshots.size() > shown) {
                auto* more = append(screenshot.root(), "catalog-screenshot-more");
                append_text_element(
                    more, "span", fmt::format("+{}", detail.screenshots.size() - shown));
            }
            screenshot.on_pressed([&window, index] { window.show_screenshot(index); });
        }
    }

    if (std::ranges::any_of(
            detail.serviceImports, [](const auto& import) { return !import.optional; }))
    {
        auto* dependencies = append(main, "section");
        dependencies->SetClass("catalog-scroll-anchor", true);
        add_existing_item<ScrollAnchor>(dependencies);
        append_text(append(dependencies, "h2"), "Dependencies");
        auto* dependencyList = append(dependencies, "catalog-dependencies");
        size_t requiredDusklight = 0;
        std::vector<std::string> dusklightProblems;
        for (const auto& import : detail.serviceImports) {
            if (import.optional) {
                continue;
            }
            const bool available = mods::svc::find_service(
                                       import.id.c_str(), import.major, import.minMinor) != nullptr;
            if (import.id.starts_with(DUSKLIGHT_SERVICE_ID_PREFIX)) {
                ++requiredDusklight;
                if (!available) {
                    dusklightProblems.push_back(mods::svc::describe_missing_service(
                        import.id.c_str(), import.major, import.minMinor));
                }
                continue;
            }
            auto* row = append(dependencyList, "catalog-dependency");
            append_text_element(row, "catalog-dependency-name", import.id);
            append_text_element(row, "catalog-dependency-status",
                fmt::format("v{}.{}+ · {}", import.major, import.minMinor,
                    available ? "Available" : "Not available"));
            row->SetClass("missing", !available);
        }
        if (requiredDusklight != 0) {
            auto* row = append(dependencyList, "catalog-dependency");
            append_text_element(row, "catalog-dependency-name", "Dusklight services");
            append_text_element(row, "catalog-dependency-status",
                dusklightProblems.empty() ?
                    fmt::format("{} required · Available", requiredDusklight) :
                    fmt::format("{} required", requiredDusklight));
            for (const auto& problem : dusklightProblems) {
                append_text_element(row, "catalog-dependency-status", problem);
            }
            row->SetClass("missing", !dusklightProblems.empty());
        }
    }

    auto* changelog = append(main, "section");
    changelog->SetClass("catalog-scroll-anchor", true);
    add_existing_item<ScrollAnchor>(changelog);
    auto* changelogTitle = append(changelog, "h2");
    append_text(changelogTitle, "Changelog ");
    append_text_element(changelogTitle, "small",
        fmt::format("v{} · {}", detail.mod.version, display_date(detail.mod.updatedAt)));
    auto* changelogFragment = append(changelog, "catalog-fragment");
    if (detail.changelogHtml.empty()) {
        append_text_element(changelogFragment, "p", "No changelog was provided.");
    } else {
        changelogFragment->SetInnerRML(detail.changelogHtml);
        add_list_markers(changelogFragment);
    }

    auto* detailList = append(sidebar, "dl");
    append_detail_field(detailList, "Version", detail.mod.version);
    append_detail_field(detailList, "Last updated", display_date(detail.mod.updatedAt));
    append_detail_field(
        detailList, "Category", detail.mod.category ? detail.mod.category->name : "Uncategorized");
    if (detail.license && !detail.license->empty()) {
        append_detail_field(detailList, "License", *detail.license);
    }
}

}  // namespace

ModBrowser::ModBrowser()
    : Window{Props{.tabBar = false, .styleSheets = {"res/rml/mod_browser.rcss"}}} {
    mRoot->SetClass("mod-browser", true);
    mLoaderGeneration = mods::ModLoader::instance().generation();
    mState = borealis::http::available() ? State::Loading : State::Unavailable;
    set_content([this](Rml::Element* content) { build_content(content); });
    if (mState == State::Loading) {
        mFetch = mods::catalog::fetch_page(mQuery);
    }
}

void ModBrowser::build_content(Rml::Element* content) {
    auto* filtersRoot = append(content, "catalog-filters");
    auto& filters =
        add_child<NavGroup>(filtersRoot, NavGroup::Props{
                                             .layout = NavGroup::Layout::Vertical,
                                             .horizontalBoundary = NavGroup::Boundary::Bubble,
                                             .verticalBoundary = NavGroup::Boundary::Stop,
                                         });
    append_text(append(filtersRoot, "h1"), "Browse Mods");
    auto& search = filters.add_item<StringButton>(StringButton::Props{
        .key = "Search",
        .getValue = [this] { return mQuery.search; },
        .setValue =
            [this](Rml::String value) {
                if (mQuery.search != value) {
                    mQuery.search = std::move(value);
                    mQuery.page = 1;
                    begin_fetch(FocusTarget::Search);
                }
            },
        .maxLength = 100,
    });
    std::vector<DropdownButton::Option> categoryOptions{{"All"}};
    if (mPage) {
        for (const auto& category : mPage->categories) {
            categoryOptions.push_back({category.name});
        }
    }
    auto& category = filters.add_item<DropdownButton>(DropdownButton::Props{
        .key = "Category",
        .options = std::move(categoryOptions),
        .getValue =
            [this] {
                if (mQuery.category.empty() || !mPage) {
                    return 0;
                }
                const auto iter = std::ranges::find(
                    mPage->categories, mQuery.category, &mods::catalog::Category::slug);
                return iter == mPage->categories.end() ?
                           0 :
                           static_cast<int>(iter - mPage->categories.begin()) + 1;
            },
        .setValue =
            [this](int index) {
                mQuery.category = index == 0 ? std::string{} : mPage->categories[index - 1].slug;
                mQuery.page = 1;
                begin_fetch(FocusTarget::Category);
            },
        .isDisabled = [this] { return !mPage || mPage->categories.empty(); },
    });
    std::vector<DropdownButton::Option> sortLabels;
    for (const auto& option : sortOptions) {
        sortLabels.push_back({Rml::String{option.label}});
    }
    auto& sort = filters.add_item<DropdownButton>(DropdownButton::Props{
        .key = "Sort by",
        .options = std::move(sortLabels),
        .getValue =
            [this] {
                const auto iter = std::ranges::find(sortOptions, mQuery.sort, &SortOption::value);
                return iter == sortOptions.end() ? 0 : static_cast<int>(iter - sortOptions.begin());
            },
        .setValue =
            [this](int index) {
                mQuery.sort = sortOptions[index].value;
                mQuery.page = 1;
                begin_fetch(FocusTarget::Sort);
            },
    });
    auto& device = filters.add_item<BoolButton>(BoolButton::Props{
        .key = "Compatible only",
        .getValue = [this] { return mQuery.thisDevice; },
        .setValue =
            [this](bool value) {
                if (mQuery.thisDevice != value) {
                    mQuery.thisDevice = value;
                    mQuery.page = 1;
                    begin_fetch(FocusTarget::Device);
                }
            },
    });

    auto* resultsRoot = append(content, "catalog-results");
    auto& results =
        add_child<NavGroup>(resultsRoot, NavGroup::Props{
                                             .layout = NavGroup::Layout::Vertical,
                                             .horizontalBoundary = NavGroup::Boundary::Bubble,
                                             .verticalBoundary = NavGroup::Boundary::Stop,
                                         });
    auto* heading = append(resultsRoot, "header");
    const std::string categoryName = [&] {
        if (mQuery.category.empty() || !mPage) {
            return std::string{"All mods"};
        }
        const auto iter =
            std::ranges::find(mPage->categories, mQuery.category, &mods::catalog::Category::slug);
        return iter == mPage->categories.end() ? std::string{"All mods"} : iter->name;
    }();
    const uint64_t total = mPage ? mPage->pagination.total : 0;
    append_text_element(heading, "h1", categoryName);
    append_text_element(
        heading, "small", fmt::format("{} mods · sorted by {}", total, sort_label(mQuery.sort)));

    Component* resultFocus = nullptr;
    Component* retryFocus = nullptr;
    auto* viewport = append(resultsRoot, "catalog-viewport");

    if (mState == State::Ready && mPage && !mPage->mods.empty()) {
        auto* gridRoot = append(viewport, "catalog-grid");
        auto& grid = results.add_existing_item<NavGroup>(
            gridRoot, NavGroup::Props{
                          .layout = NavGroup::Layout::Grid,
                          .columns = 2,
                          .horizontalBoundary = NavGroup::Boundary::Bubble,
                          .verticalBoundary = NavGroup::Boundary::Bubble,
                      });
        for (const auto& mod : mPage->mods) {
            auto& card = grid.add_item<CatalogCard>(
                mod, [this, mod] { push(std::make_unique<ModBrowserDetail>(mod)); });
            if (resultFocus == nullptr) {
                resultFocus = &card;
            }
        }

        if (mPage->pagination.pageCount > 1) {
            auto* paginationRoot = append(resultsRoot, "catalog-pagination");
            auto& pagination = results.add_existing_item<NavGroup>(
                paginationRoot, NavGroup::Props{
                                    .layout = NavGroup::Layout::Horizontal,
                                    .horizontalBoundary = NavGroup::Boundary::Stop,
                                    .verticalBoundary = NavGroup::Boundary::Bubble,
                                });
            pagination
                .add_item<ControlledButton>(ControlledButton::Props{
                    .text = "Previous",
                    .isDisabled = [this] { return mQuery.page <= 1; },
                })
                .on_pressed([this] {
                    if (mQuery.page > 1) {
                        --mQuery.page;
                        begin_fetch(FocusTarget::Results);
                    }
                });
            auto* label = append(paginationRoot, "catalog-pagination-label");
            append_text(label,
                fmt::format("Page {} of {}", mPage->pagination.page, mPage->pagination.pageCount));
            pagination
                .add_item<ControlledButton>(ControlledButton::Props{
                    .text = "Next",
                    .isDisabled =
                        [this] { return !mPage || mQuery.page >= mPage->pagination.pageCount; },
                })
                .on_pressed([this] {
                    if (mPage && mQuery.page < mPage->pagination.pageCount) {
                        ++mQuery.page;
                        begin_fetch(FocusTarget::Results);
                    }
                });
        }
    } else {
        auto* status = append(viewport, "catalog-results-status");
        switch (mState) {
        case State::Loading:
            // TODO better loading state
            append_status(status, "Loading catalog", "Fetching published mods...");
            break;
        case State::Unavailable:
            append_status(status, "Catalog unavailable", "This build has no HTTP backend.");
            break;
        case State::Error: {
            append_status(status, "Could not load mods", mError);
            auto* retryRoot = append(status, "catalog-retry-actions");
            auto& retryGroup = results.add_existing_item<NavGroup>(retryRoot, NavGroup::Props{});
            auto& retry = retryGroup.add_item<Button>("Retry");
            retry.on_pressed([this] { begin_fetch(FocusTarget::Retry); });
            retryFocus = &retry;
            break;
        }
        case State::Ready:
            append_status(status, "No mods found", "Try changing the search or category.");
            break;
        }
    }

    Component* focus = nullptr;
    switch (mFocusTarget) {
    case FocusTarget::Search:
        focus = &search;
        break;
    case FocusTarget::Category:
        focus = &category;
        break;
    case FocusTarget::Sort:
        focus = &sort;
        break;
    case FocusTarget::Device:
        focus = &device;
        break;
    case FocusTarget::Results:
        focus = resultFocus;
        break;
    case FocusTarget::Retry:
        focus = retryFocus;
        break;
    case FocusTarget::Default:
        break;
    }
    if (mState != State::Loading) {
        mFocusTarget = FocusTarget::Default;
    }
    if (focus != nullptr) {
        focus->focus();
    }
}

void ModBrowser::begin_fetch(FocusTarget focusTarget) {
    mFocusTarget = focusTarget;
    if (!borealis::http::available()) {
        mState = State::Unavailable;
        mFetch = {};
    } else {
        mState = State::Loading;
        mError.clear();
        mFetch = mods::catalog::fetch_page(mQuery);
    }
    mRebuildRequested = true;
}

void ModBrowser::finish_fetch(mods::catalog::FetchResult result) {
    if (result.page) {
        mPage = std::move(result.page);
        mQuery.page = std::max(mPage->pagination.page, 1);
        mState = State::Ready;
        mError.clear();
        if (mFocusTarget == FocusTarget::Retry) {
            mFocusTarget = FocusTarget::Results;
        }
    } else {
        mState = State::Error;
        mError = result.error.empty() ? "The catalog request failed." : std::move(result.error);
    }
    if (mFocusTarget == FocusTarget::Default) {
        mFocusTarget = FocusTarget::Search;
    }
    mRebuildRequested = true;
}

void ModBrowser::update() {
    ZoneScopedN("Mod browser update");
    const auto loaderGeneration = mods::ModLoader::instance().generation();
    if (loaderGeneration != mLoaderGeneration) {
        mLoaderGeneration = loaderGeneration;
        mRebuildRequested = true;
    }
    if (mFetch && mFetch.ready()) {
        try {
            if (auto result = mFetch.try_take()) {
                finish_fetch(std::move(*result));
            }
        } catch (const std::exception& exception) {
            finish_fetch(
                {.error = fmt::format("The catalog request failed: {}", exception.what())});
        } catch (...) {
            finish_fetch({.error = "The catalog request failed."});
        }
        mFetch = {};
    }
    if (mRebuildRequested) {
        ZoneScopedN("Mod browser rebuild");
        mRebuildRequested = false;
        auto* viewport = mContentRoot->QuerySelector("catalog-viewport");
        const float scrollTop = viewport ? viewport->GetScrollTop() : 0;
        rebuild_content();
        if (mState == State::Ready) {
            mDocument->UpdateDocument();
            if (auto* restored = mContentRoot->QuerySelector("catalog-viewport")) {
                restored->SetScrollTop(scrollTop);
            }
        }
    }
    Window::update();
}

}  // namespace dusk::ui
