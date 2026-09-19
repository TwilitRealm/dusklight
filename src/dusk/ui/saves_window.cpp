#include "saves_window.hpp"

#include "aurora/lib/window.hpp"
#include "bool_button.hpp"
#include "borealis/file_select.hpp"
#include "borealis/io.hpp"
#include "button.hpp"
#include "context_menu.hpp"
#include "dusk/data.hpp"
#include "dusk/game_mode.hpp"
#include "dusk/main.h"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/svc/save.hpp"
#include "dusk/save_manager.hpp"
#include "dusk/settings.h"
#include "dusk/utilities.hpp"
#include "format.hpp"
#include "icon_button.hpp"
#include "modal.hpp"
#include "pane.hpp"
#include "prelaunch.hpp"
#include "ui.hpp"
#include "window.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <set>

namespace dusk::ui {
namespace {

using save_manager::Artifact;
using save_manager::ModDataAction;
using save_manager::Result;
using save_manager::SaveIdentity;
using save_manager::Storage;

struct SaveOption {
    std::string saveName;
    std::string label;
};

struct Context {
    SaveIdentity identity;
    Storage storage;
};

struct ImportItem {
    Context context;
    Artifact artifact;
    bool selected = true;
};

std::atomic_uint64_t s_refreshGeneration = 1;
std::deque<std::string> s_pendingImports;
bool s_importActive = false;

std::vector<SaveOption> registered_save_options() {
    std::vector<SaveOption> modes;
    std::set<std::string> seen;
    const auto& registered = gamemode::getGameModeManager().getRegisteredGameModes();
    const auto add_mode = [&](const gamemode::GameMode& mode) {
        if (seen.insert(mode.getSaveName()).second) {
            modes.push_back({.saveName = mode.getSaveName(), .label = mode.getFullName()});
        }
    };
    if (const auto vanilla = registered.find(gamemode::kVanillaGameModeId);
        vanilla != registered.end())
    {
        add_mode(vanilla->second);
    }
    for (const auto& [id, mode] : registered) {
        if (id != gamemode::kVanillaGameModeId) {
            add_mode(mode);
        }
    }
    return modes;
}

std::string active_save_name() {
    return gamemode::getGameModeManager().getCurrentGameMode()->getSaveName();
}

bool is_registered_save(std::string_view saveName) {
    return std::ranges::any_of(registered_save_options(),
        [saveName](const SaveOption& mode) { return mode.saveName == saveName; });
}

std::string mode_label(std::string_view saveName) {
    for (const auto& mode : registered_save_options()) {
        if (mode.saveName == saveName) {
            return mode.label;
        }
    }
    return std::string{saveName};
}

save_manager::ValueResult<Context> context_for_save(const std::string& saveName) {
    const auto& state = prelaunch_state();
    if (data::is_data_path_restart_pending() ||
        (!state.activeDiscPath.empty() && state.configuredDiscPath != state.activeDiscPath))
    {
        return {{.message = "Restart required before managing saves."}, {}};
    }
    auto identity = save_manager::identity_for_disc(state.configuredDiscInfo, saveName);
    if (!state.configuredDiscCanLaunch || !identity) {
        return {{.message = "A disc must be configured before managing saves."}, {}};
    }
    const auto preferredKind = getSettings().backend.cardFileType.getValue() == 0 ?
                                   save_manager::StorageKind::RawImage :
                                   save_manager::StorageKind::GciDirectory;
    auto storage = save_manager::resolve_storage(identity->game, preferredKind);
    if (!storage) {
        return {storage.result, {}};
    }
    if (auto migrated =
            mods::svc::migrate_legacy_sidecar(storage.value, identity->maker, identity->game);
        !migrated)
    {
        return {std::move(migrated), {}};
    }
    return {
        {.ok = true},
        Context{
            .identity = std::move(*identity),
            .storage = std::move(storage.value),
        },
    };
}

void dismiss_modal(Modal& modal) {
    mDoAud_seStartMenu(kSoundWindowClose);
    modal.pop();
}

void show_message(
    std::string title, std::string body, bool error = false, std::function<void()> onClose = {}) {
    auto* host = top_document();
    if (host == nullptr) {
        if (onClose) {
            onClose();
        }
        return;
    }
    const auto close = [onClose = std::move(onClose)](Modal& modal) {
        dismiss_modal(modal);
        if (onClose) {
            onClose();
        }
    };
    host->push(std::make_unique<Modal>(Modal::Props{
        .title = std::move(title),
        .bodyText = std::move(body),
        .actions = {{.label = "OK", .onPressed = close}},
        .onDismiss = close,
        .icon = error ? "warning" : "",
    }));
}

void show_result(std::string title, std::string success, const Result& result) {
    if (result) {
        ++s_refreshGeneration;
        show_message(std::move(title), std::move(success));
    } else {
        show_message(std::move(title), result.message, true);
    }
}

bool installed_mod(std::string_view id) {
    return std::ranges::any_of(mods::ModLoader::instance().mods(),
        [id](const mods::LoadedMod& mod) { return mod.metadata.id == id; });
}

void export_artifact(save_manager::ExportArtifact artifact, std::string pattern) {
    borealis::file_select::ExportOptions options{
        .parentWindow = aurora::window::get_sdl_window(),
        .sourceLocation = borealis::io::fs_path_to_string(artifact.path),
        .suggestedName = artifact.suggestedName,
        .filters = {{"Save file", std::move(pattern)}},
    };
    borealis::file_select::export_file(
        std::move(options), [artifact = std::move(artifact)](borealis::file_select::Result result) {
            save_manager::remove_temporary_export(artifact);
            if (result.status != borealis::file_select::Status::Selected &&
                result.status != borealis::file_select::Status::Canceled)
            {
                show_message("Export Failed",
                    result.message.empty() ? "The save file could not be exported." :
                                             result.message,
                    true);
            }
        });
}

void begin_export(const std::string& saveName, bool includeModData) {
    auto context = context_for_save(saveName);
    if (!context) {
        show_message("Export Failed", context.result.message, true);
        return;
    }
    auto artifact =
        save_manager::build_export(context.value.storage, context.value.identity, includeModData);
    if (!artifact) {
        show_message("Export Failed", artifact.result.message, true);
        return;
    }
    export_artifact(std::move(artifact.value), includeModData ? "dusksave" : "gci");
}

void begin_raw_export(const std::string& saveName) {
    auto context = context_for_save(saveName);
    if (!context) {
        show_message("Export Failed", context.result.message, true);
        return;
    }
    auto artifact = save_manager::raw_card_export(context.value.storage);
    if (!artifact) {
        show_message("Export Failed", artifact.result.message, true);
        return;
    }
    export_artifact(std::move(artifact.value), "raw");
}

void process_next_import();

void finish_import_flow() {
    s_importActive = false;
    process_next_import();
}

void perform_import(
    std::shared_ptr<std::vector<ImportItem>> items, ModDataAction modDataAction, Modal& modal) {
    modal.pop();
    const bool replacingRawImage = items->front().artifact.kind == save_manager::ArtifactKind::Raw;
    Result result{.ok = true};
    size_t importedCount = 0;
    std::string importedLabel;
    for (const auto& item : *items) {
        if (!item.selected) {
            continue;
        }
        if (replacingRawImage) {
            result =
                save_manager::import_raw_image(item.context.storage, item.context.identity.game,
                    item.context.identity.maker, item.artifact, modDataAction);
        } else {
            result = save_manager::import_artifact(
                item.context.storage, item.context.identity, item.artifact, modDataAction);
        }
        if (!result) {
            break;
        }
        importedLabel = mode_label(item.context.identity.saveName);
        ++importedCount;
    }
    ++s_refreshGeneration;
    std::string message;
    if (result) {
        message = replacingRawImage  ? "The memory card image was imported." :
                  importedCount == 1 ? fmt::format("The {} save was imported.", importedLabel) :
                                       fmt::format("{} saves were imported.", importedCount);
    } else if (importedCount != 0) {
        message = fmt::format("{} save{} imported before the operation stopped: {}", importedCount,
            importedCount == 1 ? " was" : "s were", result.message);
    } else {
        message = result.message;
    }
    show_message("Save Files", std::move(message), !result, &finish_import_flow);
}

void confirm_import(Artifact artifact) {
    const bool raw = artifact.kind == save_manager::ArtifactKind::Raw;
    const bool hasBundledModData = artifact.kind == save_manager::ArtifactKind::DuskSave;
    if (!raw && !utils::is_valid_save_name(artifact.header.saveName)) {
        show_message("Import Failed", "The save contains an unsupported filename.", true,
            &finish_import_flow);
        return;
    }
    auto context =
        context_for_save(raw ? gamemode::kDefaultGameModeSaveName : artifact.header.saveName);
    if (!context) {
        show_message("Import Failed", context.result.message, true, &finish_import_flow);
        return;
    }
    if (!raw && (artifact.header.game != context.value.identity.game ||
                    artifact.header.maker != context.value.identity.maker))
    {
        show_message("Import Failed", "This save does not match the configured disc.", true,
            &finish_import_flow);
        return;
    }

    auto items = std::make_shared<std::vector<ImportItem>>();
    if (raw && context.value.storage.kind == save_manager::StorageKind::GciDirectory) {
        auto extracted = save_manager::extract_raw_saves(
            artifact, context.value.identity.game, context.value.identity.maker);
        if (!extracted) {
            show_message("Import Failed", extracted.result.message, true, &finish_import_flow);
            return;
        }
        if (extracted.value.empty()) {
            show_message("Import Failed",
                "The card image does not contain saves for the configured disc.", true,
                &finish_import_flow);
            return;
        }
        for (auto& save : extracted.value) {
            auto target = context.value;
            target.identity.saveName = save.header.saveName;
            items->push_back({.context = std::move(target), .artifact = std::move(save)});
        }
    } else {
        items->push_back({.context = context.value, .artifact = std::move(artifact)});
    }

    const bool replacingRawImage = items->front().artifact.kind == save_manager::ArtifactKind::Raw;
    const bool multiple = items->size() > 1;
    bool replacingSave = false;
    if (!replacingRawImage && !multiple) {
        const auto& target = items->front().context;
        auto info = save_manager::inspect_save(target.storage, target.identity);
        if (!info) {
            show_message("Import Failed", info.result.message, true, &finish_import_flow);
            return;
        }
        replacingSave = info.value.present;
    }

    const auto cancel = [](Modal& modal) {
        dismiss_modal(modal);
        finish_import_flow();
    };
    auto keepModData = std::make_shared<bool>(false);
    auto modal = std::make_unique<Modal>(Modal::Props{
        .title = "Import Save",
        .actions =
            {
                {.label = "Cancel", .onPressed = cancel},
                {
                    .label = "Import",
                    .onPressed =
                        [items, keepModData, hasBundledModData](Modal& modal) {
                            const auto action = hasBundledModData ? ModDataAction::Replace :
                                                *keepModData      ? ModDataAction::Keep :
                                                                    ModDataAction::Clear;
                            perform_import(items, action, modal);
                        },
                    .isDisabled =
                        [items] { return std::ranges::none_of(*items, &ImportItem::selected); },
                },
            },
        .onDismiss = cancel,
        .icon = "warning",
    });
    if (replacingRawImage) {
        modal->set_body_text(
            "Replace the entire memory card image? All saves on the current card will be "
            "replaced. Existing saves for the configured disc will be backed up first.");
    } else if (multiple) {
        modal->set_body_text("Choose the saves to import. Existing saves will be backed up first.");
        for (size_t i = 0; i < items->size(); ++i) {
            modal->content_pane().add_child<BoolButton>(BoolButton::Props{
                .key = mode_label((*items)[i].context.identity.saveName),
                .getValue = [items, i] { return (*items)[i].selected; },
                .setValue = [items, i](bool value) { (*items)[i].selected = value; },
            });
        }
    } else {
        modal->set_body(
            fmt::format("{} the <b>{}</b> save?{}", replacingSave ? "Replace" : "Import",
                escape(mode_label(items->front().context.identity.saveName)),
                replacingSave ? " A backup will be made first." : ""));
    }
    if (!replacingRawImage) {
        Rml::Element* unregistered = nullptr;
        for (const auto& item : *items) {
            if (!is_registered_save(item.context.identity.saveName)) {
                if (unregistered == nullptr) {
                    unregistered = append(modal->content_pane().root(), "text-list");
                    append_text_element(
                        unregistered, "small", "No registered game mode uses these saves.");
                }
                append_text_element(unregistered, "item", item.context.identity.saveName);
            }
        }
    }
    if (!items->front().artifact.declaredMods.empty()) {
        auto* modData = append(modal->content_pane().root(), "text-list");
        append_text_element(modData, "heading", "Included mod data:");
        for (const auto& mod : items->front().artifact.declaredMods) {
            append_text_element(modData, "item", fmt::format("{} {}", mod.id, mod.version));
        }
    }
    if (!hasBundledModData) {
        auto& pane = modal->content_pane();
        pane.add_child<BoolButton>(BoolButton::Props{
            .key = "Keep existing mod data",
            .getValue = [keepModData] { return *keepModData; },
            .setValue = [keepModData](bool value) { *keepModData = value; },
        });
    }
    if (auto* host = top_document()) {
        host->push(std::move(modal));
    } else {
        finish_import_flow();
    }
}

void import_dialog_callback(borealis::file_select::Result result) {
    if (result.status == borealis::file_select::Status::Canceled) {
        return;
    }
    if (result.status != borealis::file_select::Status::Selected || result.locations.empty()) {
        show_message("Import Failed",
            result.message.empty() ? "The save file picker could not be opened." : result.message,
            true);
        return;
    }
    import_save_location(std::move(result.locations.front()));
}

void process_next_import() {
    if (s_importActive || s_pendingImports.empty()) {
        return;
    }
    s_importActive = true;
    auto location = std::move(s_pendingImports.front());
    s_pendingImports.pop_front();
    auto artifact = save_manager::read_artifact(location);
    if (!artifact) {
        show_message("Import Failed", artifact.result.message, true, &finish_import_flow);
        return;
    }
    confirm_import(std::move(artifact.value));
}

void begin_import() {
    borealis::file_select::open_file(
        {
            .parentWindow = aurora::window::get_sdl_window(),
            .filters = {{"Save files", "gci;raw;dusksave"}},
        },
        &import_dialog_callback);
}

class SaveListHeader final : public Component {
public:
    SaveListHeader(Rml::Element* parent, bool available) : Component{append(parent, "header")} {
        append_text_element(mRoot, "section-heading", "Save Files");
        add_child<IconButton>(
            IconButton::Props{
                .icon = "sim_card_download",
                .label = "Import Save",
                .isDisabled = [available] { return !available || borealis::file_select::busy(); },
            })
            .on_pressed(&begin_import);
    }
};

void begin_delete(const std::string& saveName) {
    auto context = context_for_save(saveName);
    if (!context) {
        show_message("Delete Failed", context.result.message, true);
        return;
    }
    if (auto* host = top_document()) {
        host->push(std::make_unique<Modal>(Modal::Props{
            .title = "Delete Save",
            .bodyRml =
                fmt::format("Delete the <b>{}</b> save and mod data? A backup will be made first.",
                    escape(mode_label(saveName))),
            .actions =
                {
                    {
                        .label = "Cancel",
                        .onPressed = &dismiss_modal,
                    },
                    {
                        .label = "Delete",
                        .onPressed =
                            [context = context.value](Modal& modal) {
                                modal.pop();
                                if (const auto result = save_manager::delete_save(
                                        context.storage, context.identity);
                                    result)
                                {
                                    ++s_refreshGeneration;
                                } else {
                                    show_message("Delete Save", result.message, true);
                                }
                            },
                    },
                },
            .onDismiss = &dismiss_modal,
            .icon = "warning",
        }));
    }
}

void append_save_header(
    Rml::Element* parent, const Rml::String& title, const Rml::String& subtitle) {
    auto* header = append(parent, "save-header");
    append_text_element(header, "heading", title);
    append_text_element(header, "small", subtitle);
}

class BackupsWindow final : public Window {
public:
    explicit BackupsWindow(std::string saveName)
        : Window{Props{.tabBar = false, .styleSheets = {"res/rml/saves.rcss"}}},
          mSaveName{std::move(saveName)} {
        mRoot->SetClass("saves", true);
        mRoot->SetClass("backups", true);
        set_content([this](Rml::Element* content) { build(content); });
    }

private:
    static std::string display_name(const SaveIdentity& identity, const std::string& name) {
        const std::string prefix =
            save_manager::card_file_stem(identity.maker, identity.game, identity.saveName) + "-";
        if (!name.starts_with(prefix) || name.size() < prefix.size() + 16) {
            return name;
        }
        const std::string_view timestamp{name.data() + prefix.size(), 16};
        if (timestamp[8] != 'T' || timestamp[15] != 'Z') {
            return name;
        }
        return fmt::format("{}-{}-{} {}:{}:{} UTC", timestamp.substr(0, 4), timestamp.substr(4, 2),
            timestamp.substr(6, 2), timestamp.substr(9, 2), timestamp.substr(11, 2),
            timestamp.substr(13, 2));
    }

    void build(Rml::Element* content) {
        auto& listPane = add_child<Pane>(content, Pane::Type::Controlled);
        listPane.root()->SetClass("list", true);
        auto& detailPane = add_child<Pane>(content, Pane::Type::Uncontrolled);
        detailPane.root()->SetClass("detail", true);
        listPane.add_section("Backups");

        auto context = context_for_save(mSaveName);
        if (!context) {
            append_text_element(detailPane.root(), "p", context.result.message);
            return;
        }
        auto backups = save_manager::list_backups(context.value.storage, context.value.identity);
        if (!backups) {
            append_text_element(detailPane.root(), "p", backups.result.message);
            return;
        }

        if (backups.value.empty()) {
            detailPane.add_section("No backups yet");
            return;
        }
        if (std::ranges::none_of(backups.value,
                [this](const save_manager::BackupInfo& backup) {
                    return backup.path == mSelectedPath;
                }))
        {
            mSelectedPath = backups.value.front().path;
        }
        for (const auto& backup : backups.value) {
            const std::string label = display_name(context.value.identity, backup.name);
            auto& entry = listPane.add_group_button({
                .text = label,
                .isSelected = [this, path = backup.path] { return mSelectedPath == path; },
            });
            listPane.register_control(
                entry, detailPane, [this, context = context.value, backup, label](Pane& pane) {
                    mSelectedPath = backup.path;
                    build_detail(pane, context, backup, label);
                });
        }
        const auto selected =
            std::ranges::find(backups.value, mSelectedPath, &save_manager::BackupInfo::path);
        const auto& backup = selected == backups.value.end() ? backups.value.front() : *selected;
        build_detail(
            detailPane, context.value, backup, display_name(context.value.identity, backup.name));
    }

    void build_detail(
        Pane& pane, Context context, save_manager::BackupInfo backup, const std::string& label) {
        append_save_header(pane.root(), mode_label(context.identity.saveName), label);
        append_text_element(pane.root(), "file-path", backup.name);
        pane.add_button("Restore This Backup...").on_pressed([this, context, path = backup.path] {
            if (auto* host = top_document()) {
                host->push(std::make_unique<Modal>(Modal::Props{
                    .title = "Restore Backup",
                    .bodyText = "Replace the current save and mod data with this backup?",
                    .actions =
                        {
                            {
                                .label = "Cancel",
                                .onPressed = &dismiss_modal,
                            },
                            {
                                .label = "Restore",
                                .onPressed =
                                    [this, context, path](Modal& modal) {
                                        modal.pop();
                                        const Result result = save_manager::restore_backup(
                                            context.storage, context.identity, path);
                                        if (result) {
                                            ++s_refreshGeneration;
                                            rebuild_content();
                                        }
                                        show_message("Restore Backup",
                                            result ? "The backup was restored." : result.message,
                                            !result);
                                    },
                            },
                        },
                    .onDismiss = &dismiss_modal,
                    .icon = "warning",
                }));
            }
        });
        auto& deleteButton = pane.add_button("Delete This Backup...");
        deleteButton.root()->SetClass("danger", true);
        deleteButton.on_pressed(
            [this, storage = context.storage, path = backup.path, name = backup.name] {
                if (auto* host = top_document()) {
                    host->push(std::make_unique<Modal>(Modal::Props{
                        .title = "Delete Backup",
                        .bodyRml = fmt::format("Delete <b>{}</b>?", escape(name)),
                        .actions =
                            {
                                {
                                    .label = "Cancel",
                                    .onPressed = &dismiss_modal,
                                },
                                {
                                    .label = "Delete",
                                    .onPressed =
                                        [this, storage, path](Modal& modal) {
                                            modal.pop();
                                            const Result result =
                                                save_manager::delete_backup(storage, path);
                                            if (result) {
                                                ++s_refreshGeneration;
                                                mSelectedPath.clear();
                                                rebuild_content();
                                            } else {
                                                show_message("Delete Backup", result.message, true);
                                            }
                                        },
                                },
                            },
                        .onDismiss = &dismiss_modal,
                        .icon = "warning",
                    }));
                }
            });
    }

    std::string mSaveName;
    std::filesystem::path mSelectedPath;
};

void open_backups(std::string saveName) {
    if (auto* host = top_document()) {
        host->push(std::make_unique<BackupsWindow>(std::move(saveName)));
    }
}

void create_backup(const std::string& saveName) {
    auto context = context_for_save(saveName);
    if (!context) {
        show_message("Create Backup", context.result.message, true);
        return;
    }
    show_result("Create Backup", "Backup successfully created.",
        save_manager::create_backup(context.value.storage, context.value.identity));
}

void open_save_folder(const std::string& saveName) {
    auto context = context_for_save(saveName);
    if (!context) {
        show_message("Open Save Folder", context.result.message, true);
        return;
    }
    const auto folder = context.value.storage.kind == save_manager::StorageKind::GciDirectory ?
                            context.value.storage.path :
                            context.value.storage.path.parent_path();
    if (!data::manager().open_folder(folder)) {
        show_message(
            "Open Save Folder", "The save folder could not be opened in the file browser.", true);
    }
}

void confirm_delete_mod_data(Context context, std::string id) {
    if (auto* host = top_document()) {
        host->push(std::make_unique<Modal>(Modal::Props{
            .title = "Delete Mod Data",
            .bodyRml = fmt::format("Delete saved data for <b>{}</b>? If a game save exists, a "
                                   "backup will be made first.",
                escape(id)),
            .actions =
                {
                    {
                        .label = "Cancel",
                        .onPressed = &dismiss_modal,
                    },
                    {
                        .label = "Delete",
                        .onPressed =
                            [context = std::move(context), id = std::move(id)](Modal& modal) {
                                modal.pop();
                                show_result("Delete Mod Data", "The mod data was deleted.",
                                    save_manager::delete_mod_data(
                                        context.storage, context.identity, id));
                            },
                    },
                },
            .onDismiss = &dismiss_modal,
            .icon = "warning",
        }));
    }
}

class ModDataRow final : public Component {
public:
    ModDataRow(Rml::Element* parent, const Context& context, const save_manager::ModFileInfo& mod)
        : Component{append(parent, "save-mod")} {
        auto* info = append(mRoot, "details");
        append_text_element(info, "heading", mod.id);
        append_text_element(info, "small",
            fmt::format("{} · {}", format_bytes(mod.size),
                installed_mod(mod.id) ? "Installed" : "Not installed"));
        mDelete = &add_child<IconButton>(IconButton::Props{
            .icon = "delete",
            .label = fmt::format("Delete {} data", mod.id),
            .isDisabled = [] { return borealis::file_select::busy(); },
        });
        mDelete->root()->SetClass("danger", true);
        mDelete->on_pressed([context, id = mod.id] { confirm_delete_mod_data(context, id); });
    }

    bool focus() override { return mDelete->focus(); }

private:
    IconButton* mDelete = nullptr;
};

void build_save_detail(Pane& pane, const std::string& saveName) {
    const std::string modeLabel = mode_label(saveName);
    auto context = context_for_save(saveName);
    if (!context) {
        append_save_header(pane.root(), modeLabel, "Unavailable");
        append_text_element(pane.root(), "small", context.result.message);
        return;
    }

    const auto& storage = context.value.storage;
    const std::string storageLabel =
        storage.kind == save_manager::StorageKind::GciDirectory ? "GCI folder" : "Raw memory card";
    append_save_header(pane.root(), modeLabel, fmt::format("{} · Card A", storageLabel));
    const bool registered = is_registered_save(saveName);
    if (!registered) {
        auto* association = append(pane.root(), "p");
        auto* icon =
            append_text_element(association, "icon", material_icon("indeterminate_question_box"));
        icon->SetAttribute("aria-hidden", "true");
        append_text(association, " No registered game mode uses this save.");
    }

    auto info = save_manager::inspect_save(storage, context.value.identity);
    if (!info) {
        append_text_element(pane.root(), "small", info.result.message);
    } else {
        auto* overview = append(pane.root(), "save-overview");
        overview->SetClass(info.value.present ? "present" : "empty", true);
        append_text_element(
            overview, "heading", info.value.present ? "Save present" : "No save file");
        const auto detail = info.value.present ?
                                fmt::format("{} · Modified {}", format_bytes(info.value.size),
                                    save_manager::format_gc_time(info.value.modifiedTime)) :
                            registered ? "Import a save or start this mode to create one." :
                                         "Restore a backup or import this save to use it again.";
        append_text_element(overview, "small", detail);
    }

    const bool savePresent = info && info.value.present;
    pane.add_section("Transfer");
    append_text_element(pane.root(), "small",
        "Export a portable Dusklight archive with mod data, or a standard GCI for other tools.");
    auto& exportButton = pane.add_button(ControlledButton::Props{
        .text = "Export Save...",
        .isDisabled = [savePresent] { return !savePresent || borealis::file_select::busy(); },
    });
    exportButton.on_pressed([anchor = exportButton.root(), saveName] {
        push_document(std::make_unique<ContextMenu>(
            anchor, std::vector<ContextMenu::Item>{
                        {
                            .text = "Save + mod data (.dusksave)",
                            .icon = "folder_open",
                            .onPressed = [saveName] { begin_export(saveName, true); },
                        },
                        {
                            .text = "Save only (.gci)",
                            .icon = "description",
                            .onPressed = [saveName] { begin_export(saveName, false); },
                        },
                    }));
    });

    if (info && !info.value.mods.empty()) {
        pane.add_section(fmt::format("Mod Data ({})", info.value.mods.size()));
        for (const auto& mod : info.value.mods) {
            pane.add_child<ModDataRow>(context.value, mod);
        }
    }

    pane.add_section("Storage and Recovery");
    append_text_element(pane.root(), "small",
        fmt::format(
            "The {} most recent backups are preserved.", save_manager::kDefaultBackupRetention));
    auto& backupButton = pane.add_button(ControlledButton::Props{
        .text = "Create Backup",
        .isDisabled = [savePresent] { return !savePresent || borealis::file_select::busy(); },
    });
    backupButton.on_pressed([saveName] { create_backup(saveName); });
    pane.add_button("View Backups").on_pressed([saveName] { open_backups(saveName); });
    if (storage.kind == save_manager::StorageKind::RawImage) {
        pane.add_button(ControlledButton::Props{
                            .text = "Export Full Card Image (.raw)",
                            .isDisabled =
                                [path = storage.path] {
                                    std::error_code ec;
                                    return borealis::file_select::busy() ||
                                           !std::filesystem::is_regular_file(path, ec);
                                },
                        })
            .on_pressed([saveName] { begin_raw_export(saveName); });
    }
    if (data::manager().capabilities().canOpenFolder) {
        pane.add_button("Open Save Folder").on_pressed([saveName] { open_save_folder(saveName); });
    }
    append_text_element(pane.root(), "file-path", data::abbreviated_path_string(storage.path));

    if (savePresent) {
        pane.add_section("Danger Zone");
        auto& deleteButton = pane.add_button(ControlledButton::Props{
            .text = "Delete Save...",
            .isDisabled = [] { return borealis::file_select::busy(); },
        });
        deleteButton.root()->SetClass("danger", true);
        deleteButton.on_pressed([saveName] { begin_delete(saveName); });
    }
}

}  // namespace

SavesWindow::SavesWindow()
    : Window{Props{.tabBar = false, .styleSheets = {"res/rml/saves.rcss"}}},
      mSaveName{active_save_name()} {
    mRoot->SetClass("saves", true);
    set_content([this](Rml::Element* content) { build_content(content); });
}

void SavesWindow::build_content(Rml::Element* content) {
    mGeneration = s_refreshGeneration.load();

    auto& listPane = add_child<Pane>(content, Pane::Type::Controlled);
    listPane.root()->SetClass("list", true);
    auto& detailPane = add_child<Pane>(content, Pane::Type::Uncontrolled);
    detailPane.root()->SetClass("detail", true);

    auto context = context_for_save(mSaveName);
    listPane.add_child<SaveListHeader>(context.result.ok);
    if (!context) {
        append_text_element(detailPane.root(), "small", context.result.message);
        return;
    }
    auto saves = save_manager::list_saves(
        context.value.storage, context.value.identity.game, context.value.identity.maker);
    if (!saves) {
        append_text_element(detailPane.root(), "small", saves.result.message);
        return;
    }
    auto options = registered_save_options();
    for (const auto& identity : saves.value) {
        if (std::ranges::none_of(options,
                [&](const SaveOption& option) { return option.saveName == identity.saveName; }))
        {
            options.push_back({.saveName = identity.saveName, .label = identity.saveName});
        }
    }
    if (std::ranges::none_of(
            options, [this](const SaveOption& option) { return option.saveName == mSaveName; }))
    {
        mSaveName = active_save_name();
    }
    for (const auto& option : options) {
        auto& entry = listPane.add_group_button({
            .text = option.label,
            .isSelected = [this, saveName = option.saveName] { return mSaveName == saveName; },
            .isDisabled = [] { return borealis::file_select::busy(); },
        });
        if (!is_registered_save(option.saveName)) {
            auto* key = entry.root()->QuerySelector("key");
            auto icon = key->GetOwnerDocument()->CreateElement("icon");
            icon->SetAttribute("aria-hidden", "true");
            auto* insertedIcon = key->InsertBefore(std::move(icon), key->GetFirstChild());
            append_text(insertedIcon, material_icon("indeterminate_question_box"));
        }
        listPane.register_control(
            entry, detailPane, [this, saveName = option.saveName](Pane& pane) {
                if (mSaveName != saveName) {
                    mSaveName = saveName;
                    mDoAud_seStartMenu(kSoundItemChange);
                }
                build_save_detail(pane, mSaveName);
            });
    }
    build_save_detail(detailPane, mSaveName);
}

void SavesWindow::update() {
    if (mGeneration != s_refreshGeneration.load()) {
        rebuild_content();
    }
    Window::update();
}

void add_save_files_control(Pane& leftPane, Pane& rightPane) {
    auto& button = leftPane.add_button(ControlledButton::Props{
        .text = "Open Save Manager",
        .isDisabled =
            [] {
                const auto& state = prelaunch_state();
                return !state.configuredDiscCanLaunch || data::is_data_path_restart_pending() ||
                       (!state.activeDiscPath.empty() &&
                           state.configuredDiscPath != state.activeDiscPath);
            },
    });
    leftPane.register_control(button.on_pressed([] {
        if (auto* host = top_document()) {
            host->push(std::make_unique<SavesWindow>());
        }
    }),
        rightPane, [](Pane& pane) {
            pane.add_text("Import, export, back up, and remove saves for the configured disc.");
        });
}

void import_save_location(std::string location) {
    if (!is_prelaunch_open()) {
        push_toast({
            .type = "warning",
            .title = "Save Import",
            .content = "Reset to the main menu before importing saves.",
            .duration = std::chrono::seconds{4},
        });
        return;
    }
    s_pendingImports.push_back(std::move(location));
    process_next_import();
}

}  // namespace dusk::ui
