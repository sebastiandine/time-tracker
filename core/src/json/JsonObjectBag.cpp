#include "timetracker/json/JsonObjectBag.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace timetracker {

struct JsonObjectBag::Impl {
    nlohmann::json fields = nlohmann::json::object();
};

JsonObjectBag::JsonObjectBag() = default;
JsonObjectBag::~JsonObjectBag() = default;
JsonObjectBag::JsonObjectBag(JsonObjectBag&&) noexcept = default;
JsonObjectBag& JsonObjectBag::operator=(JsonObjectBag&&) noexcept = default;

JsonObjectBag::JsonObjectBag(const JsonObjectBag& other)
    : impl_(other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr) {}

JsonObjectBag& JsonObjectBag::operator=(const JsonObjectBag& other) {
    if (this == &other) {
        return *this;
    }
    impl_ = other.impl_ ? std::make_unique<Impl>(*other.impl_) : nullptr;
    return *this;
}

bool operator==(const JsonObjectBag& left, const JsonObjectBag& right) {
    const nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json& leftFields = left.impl_ ? left.impl_->fields : empty;
    const nlohmann::json& rightFields = right.impl_ ? right.impl_->fields : empty;
    return leftFields == rightFields;
}

void absorbUnknown(JsonObjectBag& bag, const nlohmann::json& object,
                   std::initializer_list<std::string_view> knownKeys) {
    nlohmann::json kept = nlohmann::json::object();
    if (object.is_object()) {
        for (auto it = object.begin(); it != object.end(); ++it) {
            const bool known = std::find(knownKeys.begin(), knownKeys.end(),
                                         std::string_view{it.key()}) != knownKeys.end();
            if (!known) {
                kept.emplace(it.key(), it.value());
            }
        }
    }
    if (!bag.impl_) {
        bag.impl_ = std::make_unique<JsonObjectBag::Impl>();
    }
    bag.impl_->fields = std::move(kept);
}

void emitUnknown(nlohmann::json& object, const JsonObjectBag& bag) {
    if (!object.is_object() || !bag.impl_ || !bag.impl_->fields.is_object()) {
        return;
    }
    for (auto it = bag.impl_->fields.begin(); it != bag.impl_->fields.end(); ++it) {
        if (!object.contains(it.key())) {
            object.emplace(it.key(), it.value());
        }
    }
}

}  // namespace timetracker
