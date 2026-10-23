#pragma once

#include <optional>
#include <string>
#include <utility>

namespace timetracker {

/**
 * @brief Outcome of an operation that either holds a value or an error string.
 *
 * Success and failure are both normal results. @c succeeded() is true when a
 * value is present, including a successful empty @c std::optional. Call
 * @c value() only after @c succeeded() is true.
 */
template <typename T>
class Result {
public:
    /**
     * @brief Build a successful result.
     * @param value Payload moved into the result.
     * @return A result whose @c succeeded() is true.
     */
    static Result ok(T value) {
        Result result;
        result.value_ = std::move(value);
        return result;
    }

    /**
     * @brief Build a failed result.
     * @param error Text the caller can show. Not a secret.
     * @return A result whose @c succeeded() is false.
     */
    static Result err(std::string error) {
        Result result;
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
     * @brief Borrow the error text.
     * @return The error, or an empty string on success.
     */
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    Result() = default;
    std::optional<T> value_;
    std::string error_;
};

/**
 * @brief Outcome of an operation that has no payload.
 *
 * @c succeeded() is true only for @c ok().
 */
template <>
class Result<void> {
public:
    /**
     * @brief Build a successful result.
     * @return A result whose @c succeeded() is true.
     */
    static Result ok() { return Result(true, {}); }

    /**
     * @brief Build a failed result.
     * @param error Text the caller can show. Not a secret.
     * @return A result whose @c succeeded() is false.
     */
    static Result err(std::string error) { return Result(false, std::move(error)); }

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
     * @brief Borrow the error text.
     * @return The error, or an empty string on success.
     */
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    Result(bool success, std::string error) : success_(success), error_(std::move(error)) {}

    bool success_{false};
    std::string error_;
};

}  // namespace timetracker
