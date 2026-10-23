#include "Theme.hpp"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace timetracker::ui {
namespace {

[[nodiscard]] QPalette lightPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0xf4, 0xf5, 0xf7));
    palette.setColor(QPalette::WindowText, QColor(0x1c, 0x1e, 0x21));
    palette.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::AlternateBase, QColor(0xee, 0xf1, 0xf5));
    palette.setColor(QPalette::Text, QColor(0x1c, 0x1e, 0x21));
    palette.setColor(QPalette::Button, QColor(0xe6, 0xe9, 0xee));
    palette.setColor(QPalette::ButtonText, QColor(0x1c, 0x1e, 0x21));
    palette.setColor(QPalette::Highlight, QColor(0x2f, 0x6f, 0xed));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipText, QColor(0x1c, 0x1e, 0x21));
    palette.setColor(QPalette::PlaceholderText, QColor(0x6b, 0x72, 0x80));
    return palette;
}

[[nodiscard]] QPalette darkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x2b, 0x2d, 0x31));
    palette.setColor(QPalette::WindowText, QColor(0xf2, 0xf3, 0xf5));
    palette.setColor(QPalette::Base, QColor(0x1e, 0x1f, 0x22));
    palette.setColor(QPalette::AlternateBase, QColor(0x32, 0x34, 0x38));
    palette.setColor(QPalette::Text, QColor(0xf2, 0xf3, 0xf5));
    palette.setColor(QPalette::Button, QColor(0x3a, 0x3d, 0x42));
    palette.setColor(QPalette::ButtonText, QColor(0xf2, 0xf3, 0xf5));
    palette.setColor(QPalette::Highlight, QColor(0x3d, 0x7e, 0xf0));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0x3a, 0x3d, 0x42));
    palette.setColor(QPalette::ToolTipText, QColor(0xf2, 0xf3, 0xf5));
    palette.setColor(QPalette::PlaceholderText, QColor(0xa0, 0xa6, 0xb0));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x8b, 0x90, 0x98));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x8b, 0x90, 0x98));
    return palette;
}

}  // namespace

void applyTheme(ThemePreference preference) {
    auto* application = qobject_cast<QApplication*>(QApplication::instance());
    if (application == nullptr) {
        return;
    }
    auto scheme = Qt::ColorScheme::Light;
    if (preference == ThemePreference::Dark) {
        scheme = Qt::ColorScheme::Dark;
    } else if (preference == ThemePreference::System) {
        scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Unknown) {
            scheme = Qt::ColorScheme::Light;
        }
    }
    application->setPalette(scheme == Qt::ColorScheme::Dark ? darkPalette() : lightPalette());
}

}  // namespace timetracker::ui
