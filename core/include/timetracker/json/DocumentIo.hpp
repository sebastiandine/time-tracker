#pragma once

#include "timetracker/json/JsonFileError.hpp"
#include "timetracker/json/JsonResult.hpp"
#include "timetracker/ports/IFilesystem.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <filesystem>
#include <string>
#include <utility>

namespace timetracker {

/**
 * @brief Map a JSON or standard exception onto a classified file error.
 * @tparam T Success payload of the returned result.
 * @param pointer Exception caught around parse or @c get. Null becomes malformed.
 * @return A failed result. @c kind is @c Malformed, @c Type, or @c MissingKey.
 */
template <typename T>
JsonResult<T> fromJsonException(std::exception_ptr pointer) {
    try {
        if (pointer) {
            std::rethrow_exception(pointer);
        }
    } catch (const nlohmann::json::parse_error& error) {
        return JsonResult<T>::err({JsonFileErrc::Malformed, error.what()});
    } catch (const nlohmann::json::type_error& error) {
        return JsonResult<T>::err({JsonFileErrc::Type, error.what()});
    } catch (const nlohmann::json::out_of_range& error) {
        return JsonResult<T>::err({JsonFileErrc::MissingKey, error.what()});
    } catch (const std::invalid_argument& error) {
        return JsonResult<T>::err({JsonFileErrc::Type, error.what()});
    } catch (const std::out_of_range& error) {
        return JsonResult<T>::err({JsonFileErrc::Type, error.what()});
    } catch (const std::exception& error) {
        return JsonResult<T>::err({JsonFileErrc::Malformed, error.what()});
    }
    return JsonResult<T>::err({JsonFileErrc::Malformed, "unknown json failure"});
}

/**
 * @brief Pretty-print JSON with a trailing newline.
 * @param value Document already built by @c to_json.
 * @return @c dump(2) plus one newline. UTF-8, no BOM.
 */
inline std::string dumpJson(const nlohmann::json& value) {
    std::string text = value.dump(2);
    text.push_back('\n');
    return text;
}

/**
 * @brief Read and parse a JSON file into @c T.
 *
 * Include the header that declares @c from_json before calling this, so
 * argument-dependent lookup can see it. Does not read a leftover temp file.
 * @tparam T Type with @c from_json in its namespace.
 * @param files Filesystem that reads the destination path only.
 * @param path JSON file. A missing file is @c Io, not @c MissingKey.
 * @return The value, or a classified error.
 */
template <typename T>
JsonResult<T> loadJson(IFilesystem& files, const std::filesystem::path& path) {
    auto text = files.readText(path);
    if (!text) {
        return JsonResult<T>::err({JsonFileErrc::Io, path.generic_string() + ": " + text.error()});
    }
    try {
        const auto document = nlohmann::json::parse(text.value());
        return JsonResult<T>::ok(document.template get<T>());
    } catch (...) {
        return fromJsonException<T>(std::current_exception());
    }
}

/**
 * @brief Write @p value as the only contents of @p path.
 * @tparam T Type with @c to_json in its namespace.
 * @param files Filesystem whose @c replaceText performs the durable replace.
 * @param path Destination. A sibling @c .tmp file is not left behind.
 * @param value Value to serialize. Exceptions become a classified error.
 * @return Success, or a classified error. A replace failure is @c Io.
 */
template <typename T>
JsonResult<void> storeJson(IFilesystem& files, const std::filesystem::path& path, const T& value) {
    std::string text;
    try {
        text = dumpJson(nlohmann::json(value));
    } catch (...) {
        return fromJsonException<void>(std::current_exception());
    }
    auto wrote = files.replaceText(path, text);
    if (!wrote) {
        return JsonResult<void>::err(
            {JsonFileErrc::Io, path.generic_string() + ": " + wrote.error()});
    }
    return JsonResult<void>::ok();
}

}  // namespace timetracker
