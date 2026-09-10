#include "queue.hpp"

#include "dusk/hash.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/path.hpp"
#include "dusk/ui/ui.hpp"

#include <borealis/http.hpp>
#include <borealis/io.hpp>
#include <borealis/task.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dusk::mods::queue {
namespace {

using clock = std::chrono::steady_clock;

struct VerifyResult {
    std::string error;
    ModMetadata metadata;
    std::filesystem::path stagedPath;
    bool canceled = false;
};

enum class PendingIntent { None, Pause, Cancel };

struct QueueItem {
    std::string key;
    Request request;
    State state = State::Queued;
    std::filesystem::path partialPath;
    uint64_t completed = 0;
    uint64_t total = 0;
    std::string message;
    int retryCount = 0;
    clock::time_point retryAt{};
    borealis::Task<borealis::http::Result> task;
    borealis::Task<VerifyResult> verification;
    ModOperationHandle operation;
    PendingIntent pendingIntent = PendingIntent::None;
};

std::vector<QueueItem> queueItems;
uint64_t nextQueueKey = 1;

QueueItem* find_queue_item(std::string_view key) {
    const auto item = std::ranges::find(queueItems, key,
        [](const QueueItem& candidate) { return std::string_view{candidate.key}; });
    return item == queueItems.end() ? nullptr : &*item;
}

QueueItem* find_queue_item_by_mod_id(std::string_view id) {
    const auto item = std::ranges::find(queueItems, id,
        [](const QueueItem& candidate) { return std::string_view{candidate.request.id}; });
    return item == queueItems.end() ? nullptr : &*item;
}

const Url* url_source(const QueueItem& item) {
    return std::get_if<Url>(&item.request.source);
}

const LocalFile* local_source(const QueueItem& item) {
    return std::get_if<LocalFile>(&item.request.source);
}

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](char character) {
        return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) :
                                                      character;
    });
    return value;
}

bool valid_sha256(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
               (character >= 'A' && character <= 'F');
    });
}

std::string sha256_file(
    const std::filesystem::path& path, borealis::TaskContext& context, std::string& error) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        error = "Could not open the downloaded package";
        return {};
    }

    hash::Sha256 hash;
    std::array<uint8_t, 64 * 1024> buffer{};
    uint64_t completed = 0;
    while (input) {
        if (context.cancel_requested()) {
            error = "Canceled";
            return {};
        }
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = input.gcount();
        if (count > 0) {
            hash.update(std::span{buffer.data(), static_cast<size_t>(count)});
            completed += static_cast<uint64_t>(count);
            context.report_progress(completed);
        }
    }
    if (!input.eof()) {
        error = "Could not read the downloaded package";
        return {};
    }

    return hash.finish();
}

std::filesystem::path staging_path(
    const std::filesystem::path& stagingDir, std::string_view modId, std::string_view key) {
    return stagingDir / fmt::format("{}-{}.dusk.part", safe_filename(modId), safe_filename(key));
}

bool copy_to_staging(const std::filesystem::path& source, const std::filesystem::path& destination,
    uint64_t total, borealis::TaskContext& context, std::string& error) {
    std::error_code filesystemError;
    std::filesystem::create_directories(destination.parent_path(), filesystemError);
    if (filesystemError) {
        error =
            fmt::format("Could not create the staging directory: {}", filesystemError.message());
        return false;
    }
    std::ifstream input{source, std::ios::binary};
    std::ofstream output{destination, std::ios::binary | std::ios::trunc};
    if (!input || !output) {
        error = "Could not stage the local package";
        return false;
    }
    std::array<char, 64 * 1024> buffer{};
    uint64_t completed = 0;
    while (input) {
        if (context.cancel_requested()) {
            error = "Canceled";
            output.close();
            std::filesystem::remove(destination, filesystemError);
            return false;
        }
        input.read(buffer.data(), buffer.size());
        const auto count = input.gcount();
        if (count > 0) {
            output.write(buffer.data(), count);
            completed += static_cast<uint64_t>(count);
            context.report_progress(completed, total);
        }
    }
    output.close();
    if (!input.eof() || !output) {
        error = "Could not copy the local package";
        std::filesystem::remove(destination, filesystemError);
        return false;
    }
    return true;
}

VerifyResult verify_url_package(const std::filesystem::path& path, const Request& request,
    const Url& source, const std::filesystem::path& stagingDir, std::string key,
    borealis::TaskContext& context) {
    std::error_code ec;
    const auto actualSize = std::filesystem::file_size(path, ec);
    if (ec) {
        return {.error = fmt::format("Could not read the downloaded package: {}", ec.message())};
    }
    if (actualSize != source.size) {
        return {.error = "Package size mismatch"};
    }

    std::string error;
    const auto actualHash = sha256_file(path, context, error);
    if (!error.empty()) {
        return {.error = std::move(error)};
    }
    if (context.cancel_requested()) {
        return {.canceled = true};
    }
    if (actualHash != lowercase(source.sha256)) {
        return {.error = "Package checksum mismatch"};
    }

    ModMetadata metadata;
    if (!inspect_mod_bundle(path, metadata, error)) {
        return {.error = fmt::format("Invalid mod package: {}", error)};
    }
    if (metadata.id != request.id) {
        return {.error = "Package ID does not match the catalog entry"};
    }
    if (metadata.version != request.version) {
        return {.error = "Package version does not match the catalog entry"};
    }
    if (context.cancel_requested()) {
        return {.canceled = true};
    }
    const auto stagedPath = staging_path(stagingDir, metadata.id, key);
    std::filesystem::create_directories(stagedPath.parent_path(), ec);
    if (ec) {
        return {.error = fmt::format("Could not create the staging directory: {}", ec.message())};
    }
    std::string replaceError;
    if (!borealis::io::atomic_replace(path, stagedPath, replaceError)) {
        return {.error = std::move(replaceError)};
    }
    return {.metadata = std::move(metadata), .stagedPath = stagedPath};
}

VerifyResult verify_local_package(const LocalFile& source, const std::filesystem::path& stagingDir,
    std::string key, borealis::TaskContext& context) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(source.path, ec);
    if (ec) {
        return {.error = fmt::format("Could not read the local package: {}", ec.message())};
    }
    context.report_progress(0, size);
    const auto stagedPath = stagingDir / fmt::format("{}.dusk.part", key);
    std::string error;
    if (!copy_to_staging(source.path, stagedPath, size, context, error)) {
        return {.error = std::move(error), .canceled = context.cancel_requested()};
    }
    // Validate the bytes handed to the loader; the source may change during copying.
    ModMetadata metadata;
    if (!inspect_mod_bundle(stagedPath, metadata, error)) {
        std::filesystem::remove(stagedPath, ec);
        return {.error = fmt::format("Invalid mod package: {}", error)};
    }
    return {.metadata = std::move(metadata), .stagedPath = stagedPath};
}

void remove_partial(const QueueItem& item) {
    std::error_code ec;
    if (!item.partialPath.empty()) {
        std::filesystem::remove(item.partialPath, ec);
        auto metadataPath = item.partialPath;
        metadataPath += ".borealis-resume.json";
        std::filesystem::remove(metadataPath, ec);
    }
}

void fail(QueueItem& item, std::string message, bool discardPartial) {
    item.task = {};
    item.verification = {};
    item.state = State::Failed;
    item.message = std::move(message);
    item.pendingIntent = PendingIntent::None;
    if (discardPartial) {
        remove_partial(item);
        item.completed = 0;
    }
    const char* title =
        local_source(item) != nullptr ? "Mod package failed" : "Mod download failed";
    ui::push_toast({
        .type = "warning",
        .title = title,
        .content = fmt::format("{}: {}", item.request.name, item.message),
        .duration = std::chrono::seconds{6},
    });
}

bool retryable(const borealis::http::Result& result) {
    if (result.error == borealis::http::Error::Network ||
        result.error == borealis::http::Error::Timeout)
    {
        return true;
    }
    const int status = result.response.statusCode;
    return result.error == borealis::http::Error::None &&
           (status == 408 || status == 425 || status == 429 || status >= 500);
}

void schedule_retry(QueueItem& item, std::string message) {
    ++item.retryCount;
    const int delaySeconds = std::min(30, 1 << std::min(item.retryCount, 4));
    item.retryAt = clock::now() + std::chrono::seconds{delaySeconds};
    item.state = State::Retrying;
    item.message = std::move(message);
    item.task = {};
}

void start_download(QueueItem& item) {
    const auto* source = url_source(item);
    if (source == nullptr) {
        fail(item, "The install source is not a URL", false);
        return;
    }
    const auto userDir = ModLoader::instance().user_mods_dir();
    if (userDir.empty()) {
        fail(item, "No writable mods directory is configured", false);
        return;
    }

    item.partialPath =
        userDir / ".downloads" / fmt::format("{}.dusk.part", safe_filename(item.request.id));
    std::error_code ec;
    std::filesystem::create_directories(item.partialPath.parent_path(), ec);
    if (ec) {
        fail(item, fmt::format("Could not create the download directory: {}", ec.message()), false);
        return;
    }

    item.pendingIntent = PendingIntent::None;
    item.message.clear();
    item.total = source->size;
    item.state = State::Downloading;
    item.task = borealis::http::start({
        .url = source->url,
        .downloadTo = item.partialPath,
        .connectTimeout = std::chrono::seconds{10},
        .idleTimeout = std::chrono::seconds{15},
        .totalTimeout = std::nullopt,
    });
}

void start_local_verification(QueueItem& item) {
    const auto* source = local_source(item);
    if (source == nullptr) {
        fail(item, "The install source is not a local file", false);
        return;
    }
    const auto userDir = ModLoader::instance().user_mods_dir();
    if (userDir.empty()) {
        fail(item, "No writable mods directory is configured", false);
        return;
    }
    item.state = State::Verifying;
    item.message.clear();
    item.completed = 0;
    std::error_code error;
    item.total = std::filesystem::file_size(source->path, error);
    const auto stagingDir = userDir / ".staging";
    const auto local = *source;
    item.verification =
        borealis::spawn([local, stagingDir, key = item.key](borealis::TaskContext& context) {
            return verify_local_package(local, stagingDir, key, context);
        });
}

void finish_download(QueueItem& item) {
    const auto progress = item.task.progress();
    item.completed = std::max(item.completed, progress.completed);

    std::optional<borealis::http::Result> completed;
    std::string taskError;
    bool taskFailed = false;
    try {
        completed = item.task.try_take();
    } catch (const std::exception& exception) {
        taskError = exception.what();
        taskFailed = true;
    } catch (...) {
        taskError = "The download failed";
        taskFailed = true;
    }
    if (!completed && !taskFailed) {
        return;
    }
    item.task = {};

    switch (std::exchange(item.pendingIntent, PendingIntent::None)) {
    case PendingIntent::Cancel:
        remove_partial(item);
        item.completed = 0;
        item.state = State::Canceled;
        return;
    case PendingIntent::Pause:
        return;
    case PendingIntent::None:
        break;
    }

    if (taskFailed) {
        schedule_retry(item, std::move(taskError));
        return;
    }

    if (completed->error != borealis::http::Error::None || completed->response.statusCode < 200 ||
        completed->response.statusCode >= 300)
    {
        const auto message = !completed->message.empty() ? completed->message :
                             completed->response.statusCode != 0 ?
                                                           fmt::format("Server returned HTTP {}",
                                                               completed->response.statusCode) :
                                                           "The download failed";
        if (retryable(*completed)) {
            schedule_retry(item, message);
        } else {
            fail(item, message, true);
        }
        return;
    }

    const auto* source = url_source(item);
    if (source == nullptr) {
        fail(item, "The install source changed", true);
        return;
    }
    item.completed = source->size;
    item.state = State::Verifying;
    item.message.clear();
    const auto stagingDir = ModLoader::instance().user_mods_dir() / ".staging";
    item.verification =
        borealis::spawn([path = item.partialPath, request = item.request, source = *source,
                            stagingDir, key = item.key](borealis::TaskContext& context) {
            return verify_url_package(path, request, source, stagingDir, key, context);
        });
}

void finish_verification(QueueItem& item) {
    VerifyResult result;
    try {
        auto completed = item.verification.try_take();
        if (!completed) {
            return;
        }
        result = std::move(*completed);
    } catch (const std::exception& exception) {
        result.error = exception.what();
    } catch (...) {
        result.error = "Package verification failed";
    }
    item.verification = {};
    if (result.canceled || item.pendingIntent == PendingIntent::Cancel) {
        item.pendingIntent = PendingIntent::None;
        if (!result.stagedPath.empty()) {
            std::error_code error;
            std::filesystem::remove(result.stagedPath, error);
        }
        remove_partial(item);
        item.state = State::Canceled;
        item.completed = 0;
        return;
    }
    if (!result.error.empty()) {
        fail(item, std::move(result.error), true);
        return;
    }
    remove_partial(item);
    item.partialPath = result.stagedPath;
    if (const auto duplicate = find_queue_item_by_mod_id(result.metadata.id);
        duplicate != nullptr && duplicate != &item && !is_terminal(duplicate->state))
    {
        fail(item, "This mod already has an active install", true);
        return;
    }
    if (local_source(item) != nullptr && !item.request.id.empty() &&
        (item.request.id != result.metadata.id || item.request.version != result.metadata.version))
    {
        fail(item, "The local package changed after confirmation", true);
        return;
    }
    item.request.id = result.metadata.id;
    item.request.name = result.metadata.name;
    item.request.version = result.metadata.version;
    item.completed = item.total;
    item.state = State::Handoff;
    item.operation = ModLoader::instance().request_install(std::move(result.stagedPath));
}

Item snapshot(const QueueItem& item) {
    Item result{
        .id = item.key,
        .modId = item.request.id,
        .name = item.request.name,
        .version = item.request.version,
        .state = item.state,
        .completed = item.completed,
        .total = item.total,
        .message = item.message,
        .local = local_source(item) != nullptr,
        .icon = item.request.icon,
    };
    if (item.task) {
        result.completed = std::max(result.completed, item.task.progress().completed);
    }
    if (item.verification) {
        const auto progress = item.verification.progress();
        result.completed = progress.completed;
        if (progress.total) {
            result.total = *progress.total;
        }
    }
    if (item.state == State::Retrying) {
        const auto remaining = item.retryAt - clock::now();
        result.retrySeconds = std::max(
            0, static_cast<int>(std::chrono::ceil<std::chrono::seconds>(remaining).count()));
    }
    return result;
}

}  // namespace

bool enqueue(Request request, std::string* keyOut) {
    const auto* source = std::get_if<Url>(&request.source);
    const auto* local = std::get_if<LocalFile>(&request.source);
    if (source != nullptr) {
        if (request.id.empty() || request.version.empty() || source->size == 0 ||
            !source->url.starts_with("https://") || !valid_sha256(source->sha256))
        {
            return false;
        }
    } else if (local == nullptr || request.id.empty() || request.version.empty()) {
        return false;
    }
    const auto total = source == nullptr ? 0 : source->size;
    if (request.name.empty()) {
        request.name = request.id;
    }

    if (auto* existing = find_queue_item_by_mod_id(request.id)) {
        if (!is_terminal(existing->state)) {
            return false;
        }
        existing->request = std::move(request);
        existing->state = State::Queued;
        existing->completed = 0;
        existing->total = total;
        existing->partialPath.clear();
        existing->message.clear();
        existing->retryCount = 0;
        existing->operation.reset();
        existing->pendingIntent = PendingIntent::None;
        if (keyOut != nullptr) {
            *keyOut = existing->key;
        }
        return true;
    }

    const auto key = fmt::format("queue-{}", nextQueueKey++);
    if (keyOut != nullptr) {
        *keyOut = key;
    }
    queueItems.push_back({.key = key, .request = std::move(request), .total = total});
    return true;
}

void update() {
    for (auto item = queueItems.begin(); item != queueItems.end();) {
        if (item->task && item->task.ready()) {
            finish_download(*item);
        }
        if (item->state == State::Verifying && item->verification && item->verification.ready()) {
            finish_verification(*item);
        }
        if (item->state == State::Handoff && item->operation &&
            item->operation->state != ModOperation::State::Pending)
        {
            item->message = item->operation->message;
            item->state = item->operation->state == ModOperation::State::Succeeded ?
                              State::Installed :
                              State::InstallFailed;
            item->operation.reset();
        }
        ++item;
    }

    for (auto& item : queueItems) {
        if (item.task || item.verification || item.operation) {
            return;
        }
        if (is_terminal(item.state) || item.state == State::Paused) {
            continue;
        }
        if (item.state == State::Downloading || item.state == State::Verifying) {
            return;
        }
        if (item.state == State::Retrying && clock::now() < item.retryAt) {
            return;
        }
        if (item.state == State::Queued || item.state == State::Retrying) {
            if (local_source(item) != nullptr) {
                start_local_verification(item);
            } else {
                start_download(item);
            }
        }
        return;
    }
}

void shutdown() noexcept {
    for (auto& item : queueItems) {
        if (item.task) {
            item.task.cancel();
        }
        if (item.verification) {
            item.verification.cancel();
        }
    }
    queueItems.clear();
}

std::vector<Item> items() {
    std::vector<Item> result;
    result.reserve(queueItems.size());
    for (const auto& item : queueItems) {
        result.push_back(snapshot(item));
    }
    return result;
}

std::optional<Item> find(std::string_view id) {
    const auto* item = find_queue_item(id);
    return item == nullptr ? std::nullopt : std::optional<Item>{snapshot(*item)};
}

std::optional<Item> find_by_mod_id(std::string_view id) {
    const auto* item = find_queue_item_by_mod_id(id);
    return item == nullptr ? std::nullopt : std::optional<Item>{snapshot(*item)};
}

bool has_active_items() {
    return std::ranges::any_of(
        queueItems, [](const QueueItem& item) { return !is_terminal(item.state); });
}

size_t item_count() noexcept {
    return queueItems.size();
}

size_t active_count() noexcept {
    return static_cast<size_t>(std::ranges::count_if(
        queueItems, [](const QueueItem& item) { return !is_terminal(item.state); }));
}

std::optional<Item> first_active() {
    const auto item = std::ranges::find_if(
        queueItems, [](const QueueItem& candidate) { return !is_terminal(candidate.state); });
    return item == queueItems.end() ? std::nullopt : std::optional<Item>{snapshot(*item)};
}

size_t active_items_ahead(std::string_view id) noexcept {
    size_t result = 0;
    for (const auto& item : queueItems) {
        if (item.request.id == id) {
            break;
        }
        if (!is_terminal(item.state)) {
            ++result;
        }
    }
    return result;
}

void pause(std::string_view id) {
    auto* item = find_queue_item(id);
    if (item == nullptr || local_source(*item) != nullptr ||
        item->pendingIntent == PendingIntent::Cancel)
    {
        return;
    }
    if (item->state == State::Queued || item->state == State::Retrying) {
        item->state = State::Paused;
        return;
    }
    if (item->state == State::Downloading && item->task) {
        item->completed = std::max(item->completed, item->task.progress().completed);
        item->pendingIntent = PendingIntent::Pause;
        item->state = State::Paused;
        item->task.cancel();
    }
}

void resume(std::string_view id) {
    auto* item = find_queue_item(id);
    if (item == nullptr || item->state != State::Paused ||
        item->pendingIntent == PendingIntent::Cancel)
    {
        return;
    }
    item->state = State::Queued;
}

void retry(std::string_view id) {
    auto* item = find_queue_item(id);
    if (item == nullptr) {
        return;
    }
    if (item->state == State::InstallFailed) {
        auto* mod = ModLoader::instance().find_mod(item->request.id);
        if (mod != nullptr) {
            if (mod->activation_failed()) {
                item->message.clear();
                item->state = State::Handoff;
                item->operation = ModLoader::instance().request_reactivate(item->request.id);
            } else {
                item->message.clear();
                item->state = State::Installed;
            }
            return;
        }
    } else if (item->state != State::Failed) {
        return;
    }
    remove_partial(*item);
    item->completed = 0;
    item->retryCount = 0;
    item->message.clear();
    item->state = State::Queued;
}

void cancel(std::string_view id) {
    auto* item = find_queue_item(id);
    if (item == nullptr || item->state == State::Handoff || is_terminal(item->state)) {
        return;
    }
    if (item->task) {
        item->pendingIntent = PendingIntent::Cancel;
        item->message = "Canceling...";
        item->task.cancel();
        return;
    }
    if (item->verification) {
        item->pendingIntent = PendingIntent::Cancel;
        item->message = "Canceling...";
        item->verification.cancel();
        return;
    }
    remove_partial(*item);
    item->pendingIntent = PendingIntent::None;
    item->completed = 0;
    item->state = State::Canceled;
}

void clear(std::string_view id) {
    const auto item = std::ranges::find(
        queueItems, id, [](const QueueItem& candidate) { return std::string_view{candidate.key}; });
    if (item != queueItems.end() && is_terminal(item->state)) {
        queueItems.erase(item);
    }
}

void remove_by_mod_id(std::string_view id) {
    std::erase_if(queueItems,
        [id](const QueueItem& item) { return std::string_view{item.request.id} == id; });
}

void pause_all() {
    std::vector<std::string> ids;
    for (const auto& item : queueItems) {
        if (item.state == State::Queued || item.state == State::Retrying ||
            item.state == State::Downloading)
        {
            ids.push_back(item.key);
        }
    }
    for (const auto& id : ids) {
        pause(id);
    }
}

void clear_finished() {
    std::erase_if(queueItems, [](const QueueItem& item) { return is_terminal(item.state); });
}

}  // namespace dusk::mods::queue
