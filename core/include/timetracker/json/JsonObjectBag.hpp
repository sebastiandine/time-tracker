#pragma once

#include <nlohmann/json_fwd.hpp>

#include <initializer_list>
#include <memory>
#include <string_view>

namespace timetracker {

/**
 * @brief Unknown JSON object members kept beside a typed document.
 *
 * Copyable. Empty until @c absorbUnknown runs. Equality compares the kept
 * members. The JSON key is never the word @c extras.
 */
class JsonObjectBag {
public:
    JsonObjectBag();
    ~JsonObjectBag();
    JsonObjectBag(const JsonObjectBag&);
    JsonObjectBag& operator=(const JsonObjectBag&);
    JsonObjectBag(JsonObjectBag&&) noexcept;
    JsonObjectBag& operator=(JsonObjectBag&&) noexcept;

    /**
     * @brief Compare kept members.
     * @param left First bag. A missing implementation compares as empty.
     * @param right Second bag.
     * @return True when both bags hold the same unknown members.
     */
    friend bool operator==(const JsonObjectBag& left, const JsonObjectBag& right);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend void absorbUnknown(JsonObjectBag&, const nlohmann::json&,
                              std::initializer_list<std::string_view>);
    friend void emitUnknown(nlohmann::json&, const JsonObjectBag&);
};

/**
 * @brief Replace @p bag with object members that are not in @p knownKeys.
 * @param bag Destination. Previous members are discarded.
 * @param object JSON object to scan. Other kinds leave the bag empty.
 * @param knownKeys Keys this document writes itself, including @c schemaVersion.
 */
void absorbUnknown(JsonObjectBag& bag, const nlohmann::json& object,
                   std::initializer_list<std::string_view> knownKeys);

/**
 * @brief Add bag members that are not already on @p object.
 * @param object JSON object. Known keys already present are left alone.
 * @param bag Members captured by @c absorbUnknown.
 */
void emitUnknown(nlohmann::json& object, const JsonObjectBag& bag);

}  // namespace timetracker
