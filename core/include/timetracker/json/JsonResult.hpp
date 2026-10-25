#pragma once

#include "timetracker/json/JsonFileError.hpp"

#include <optional>
#include <utility>

namespace timetracker {

/**
 * @brief Outcome of a JSON file operation.
 *
 * Success and failure are both normal results. @c error() is meaningful only
 * when @c succeeded() is false. This is separate from @c Result, which carries
 * a string.
 * @tparam T Success payload. @c void when the operation has no payload.
 */
template <typename T>
class JsonResult {
public:
    /**
     * @brief Build a successful result.
     * @param value Payload moved into the result.
     * @return A result whose @c succeeded() is true.
     */
    static JsonResult ok(T value) {
        JsonResult result;
        result.value_ = std::move(value);
        return result;
    }

    /**
     * @brief Build a failed result.
     * @param error Classification and message. Not a secret.
     * @return A result whose @c succeeded() is false.
     */
    static JsonResult err(JsonFileError error) {
        JsonResult result;
        result.error_ = std::move(error);
        return result;
    }

    /**
     * @brief Report whether a value is present.
     * @return True when @c ok() was used.
     */
    [[nodiscard]] bool succeeded() const noexcept { return value_.has_value(); }

    /**
     * @brief Same test as @c succeeded(), for @c if statements.
     * @return True when a value is present.
     */
    explicit operator bool() const noexcept { return succeeded(); }

    /**
     * @brief Borrow the success payload.
     * @return The stored value.
     * @pre @c succeeded() is true.
     */
    [[nodiscard]] T& value() & { return value_.value(); }

    /**
     * @brief Borrow the success payload.
     * @return The stored value.
     * @pre @c succeeded() is true.
     */
    [[nodiscard]] const T& value() const& { return value_.value(); }

    /**
     * @brief Borrow the classified error.
     * @return The error. Default-constructed on success.
     */
    [[nodiscard]] const JsonFileError& error() const { return error_; }

private:
    JsonResult() = default;
    std::optional<T> value_;
    JsonFileError error_{};
};

/**
 * @brief Outcome of a JSON file operation with no payload.
 *
 * @c succeeded() is true only for @c ok(), which default-constructs the error.
 */
template <>
class JsonResult<void> {
public:
    /**
     * @brief Build a successful result.
     * @return A result whose @c succeeded() is true.
     */
    static JsonResult ok() { return JsonResult(true, {}); }

    /**
     * @brief Build a failed result.
     * @param error Classification and message. Not a secret.
     * @return A result whose @c succeeded() is false.
     */
    static JsonResult err(JsonFileError error) { return JsonResult(false, std::move(error)); }

    /**
     * @brief Report whether the operation succeeded.
     * @return True when @c ok() was used.
     */
    [[nodiscard]] bool succeeded() const noexcept { return success_; }

    /**
     * @brief Same test as @c succeeded(), for @c if statements.
     * @return True when the operation succeeded.
     */
    explicit operator bool() const noexcept { return succeeded(); }

    /**
     * @brief Borrow the classified error.
     * @return The error. Default-constructed on success.
     */
    [[nodiscard]] const JsonFileError& error() const { return error_; }

private:
    JsonResult(bool success, JsonFileError error)
        : success_(success), error_(std::move(error)) {}

    bool success_{false};
    JsonFileError error_{};
};

}  // namespace timetracker
