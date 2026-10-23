#include "TextRules.hpp"

#include <string>

namespace timetracker::detail {
namespace {

[[nodiscard]] bool isTrimChar(unsigned char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
           character == '\f' || character == '\v';
}

[[nodiscard]] std::string trimCopy(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && isTrimChar(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && isTrimChar(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string{text.substr(begin, end - begin)};
}

[[nodiscard]] std::size_t utf8CodePoints(std::string_view text) {
    std::size_t count = 0;
    for (const unsigned char character : text) {
        if ((character & 0xC0U) != 0x80U) {
            ++count;
        }
    }
    return count;
}

}  // namespace

Result<std::string> validatedName(std::string_view name) {
    auto trimmed = trimCopy(name);
    if (trimmed.empty()) {
        return Result<std::string>::err("Name is empty.");
    }
    if (utf8CodePoints(trimmed) > kMaxNameLength) {
        return Result<std::string>::err("Name is too long.");
    }
    return Result<std::string>::ok(std::move(trimmed));
}

Result<std::string> validatedNotes(std::string_view notes) {
    auto trimmed = trimCopy(notes);
    if (utf8CodePoints(trimmed) > kMaxNotesLength) {
        return Result<std::string>::err("Notes are too long.");
    }
    return Result<std::string>::ok(std::move(trimmed));
}

Result<NameAndNotes> validatedNameAndNotes(std::string_view name, std::string_view notes) {
    auto validName = validatedName(name);
    if (!validName) {
        return Result<NameAndNotes>::err(validName.error());
    }
    auto validNotes = validatedNotes(notes);
    if (!validNotes) {
        return Result<NameAndNotes>::err(validNotes.error());
    }
    NameAndNotes fields;
    fields.name = std::move(validName.value());
    fields.notes = std::move(validNotes.value());
    return Result<NameAndNotes>::ok(std::move(fields));
}

}  // namespace timetracker::detail
