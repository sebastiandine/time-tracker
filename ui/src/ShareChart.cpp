#include "ShareChart.hpp"

#include "timetracker/time/TimeFormat.hpp"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace timetracker::ui {
namespace {

constexpr int kOuterPadding = 8;
constexpr int kTitleHeight = 22;
constexpr int kContentTopGap = 6;
constexpr int kLegendRowHeight = 22;
constexpr int kLegendGap = 12;
constexpr int kFullCircle = 360 * 16;
constexpr int kPieStart = 90 * 16;
constexpr int kBarRowHeight = 28;
constexpr int kBarPadding = 8;
constexpr int kBarValueWidth = 88;

[[nodiscard]] QColor sliceColor(int index) {
    static const QColor colors[] = {
        QColor(0x3d, 0x7e, 0xf0), QColor(0xe0, 0x7a, 0x3d), QColor(0x3c, 0xb3, 0x71),
        QColor(0xc4, 0x4e, 0x8a), QColor(0x8e, 0x6b, 0xd4), QColor(0xd4, 0xa0, 0x17),
        QColor(0x2a, 0xa1, 0xb5), QColor(0xd1, 0x54, 0x4a), QColor(0x6b, 0x8f, 0x71),
        QColor(0x5c, 0x6b, 0xc0), QColor(0xb5, 0x6b, 0x3c), QColor(0x4e, 0xa3, 0x8a),
    };
    return colors[index % static_cast<int>(sizeof(colors) / sizeof(colors[0]))];
}

[[nodiscard]] std::int64_t sliceTotal(const std::vector<ChartSlice>& slices) {
    std::int64_t total = 0;
    for (const auto& slice : slices) {
        total += slice.seconds;
    }
    return total;
}

[[nodiscard]] QString sliceTip(const ChartSlice& slice) {
    QString text = slice.label;
    text += QStringLiteral(" \u2014 ");
    text += QString::fromStdString(formatDuration(slice.seconds));
    text += QStringLiteral(" (");
    text += QString::number(slice.percent);
    text += QStringLiteral("%)");
    return text;
}

void showSliceTip(QWidget* widget, const QPoint& globalPos, const std::vector<ChartSlice>& slices,
                  int index) {
    if (index < 0 || index >= static_cast<int>(slices.size())) {
        QToolTip::hideText();
        return;
    }
    QToolTip::showText(globalPos, sliceTip(slices[static_cast<std::size_t>(index)]), widget);
}

[[nodiscard]] QRect paddedBounds(const QRect& widgetRect) {
    return widgetRect.adjusted(kOuterPadding, kOuterPadding, -kOuterPadding, -kOuterPadding);
}

[[nodiscard]] QRect belowHeading(const QRect& bounds) {
    return bounds.adjusted(0, kTitleHeight + kContentTopGap, 0, 0);
}

struct PieLayout {
    QRectF disk;
    QRect legend;
};

[[nodiscard]] PieLayout pieLayout(const QRect& content, int sliceCount) {
    const int legendHeight = std::max(sliceCount, 1) * kLegendRowHeight;
    const int roomForDisk = std::max(0, content.width() / 2);
    const int side = std::min(content.height(), roomForDisk);
    const int diskY = content.top() + std::max(0, (content.height() - side) / 2);
    PieLayout layout;
    layout.disk = QRectF(content.left(), diskY, side, side);
    const int legendLeft = content.left() + side + kLegendGap;
    const int legendWidth = std::max(0, content.left() + content.width() - legendLeft);
    const int legendTop = content.top() + std::max(0, (content.height() - legendHeight) / 2);
    layout.legend = QRect(legendLeft, legendTop, legendWidth, legendHeight);
    return layout;
}

[[nodiscard]] int wedgeSpan(std::int64_t seconds, std::int64_t total) {
    return static_cast<int>((seconds * kFullCircle + total / 2) / total);
}

void paintHeading(QPainter& painter, const QRect& bounds, const QString& title,
                  const QFont& bodyFont) {
    QFont heading = bodyFont;
    heading.setBold(true);
    painter.setFont(heading);
    painter.drawText(QRect(bounds.left(), bounds.top(), bounds.width(), kTitleHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.setFont(bodyFont);
}

void paintWedges(QPainter& painter, const QRectF& disk, const std::vector<ChartSlice>& slices) {
    const auto total = sliceTotal(slices);
    if (total <= 0 || disk.width() <= 0.0) {
        return;
    }
    // Qt measures angles counter-clockwise from 3 o'clock. A negative span
    // walks clockwise from 12 o'clock, which matches the hit test below.
    int start = kPieStart;
    int used = 0;
    for (int index = 0; index < static_cast<int>(slices.size()); ++index) {
        const bool last = index + 1 == static_cast<int>(slices.size());
        const auto seconds = slices[static_cast<std::size_t>(index)].seconds;
        int span = last ? (kFullCircle - used) : wedgeSpan(seconds, total);
        span = std::clamp(span, 0, kFullCircle - used);
        painter.setPen(Qt::NoPen);
        painter.setBrush(sliceColor(index));
        if (span >= kFullCircle) {
            painter.drawEllipse(disk);
        } else if (span > 0) {
            painter.drawPie(disk, start, -span);
        }
        start -= span;
        used += span;
    }
}

void paintLegendRow(QPainter& painter, const QRect& row, int index, const ChartSlice& slice,
                    const QColor& textColor) {
    constexpr int kSwatch = 10;
    const int swatchY = row.top() + (row.height() - kSwatch) / 2;
    painter.setPen(Qt::NoPen);
    painter.setBrush(sliceColor(index));
    painter.drawRoundedRect(QRectF(row.left(), swatchY, kSwatch, kSwatch), 2, 2);

    QString text = slice.label;
    text += QStringLiteral("  ");
    text += QString::number(slice.percent);
    text += QStringLiteral("%");
    const int textLeft = row.left() + kSwatch + 6;
    const int textWidth = std::max(0, row.left() + row.width() - textLeft);
    const QFontMetrics metrics(painter.font());
    painter.setPen(textColor);
    painter.drawText(QRect(textLeft, row.top(), textWidth, row.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(text, Qt::ElideRight, textWidth));
}

void paintLegend(QPainter& painter, const QRect& legend, const std::vector<ChartSlice>& slices,
                 const QColor& textColor) {
    for (int index = 0; index < static_cast<int>(slices.size()); ++index) {
        const QRect row(legend.left(), legend.top() + index * kLegendRowHeight, legend.width(),
                        kLegendRowHeight);
        paintLegendRow(painter, row, index, slices[static_cast<std::size_t>(index)], textColor);
    }
}

[[nodiscard]] int legendIndexAt(const QRect& legend, int sliceCount, QPointF pos) {
    if (sliceCount <= 0 || legend.width() <= 0) {
        return -1;
    }
    const QPoint point = pos.toPoint();
    if (point.x() < legend.left() || point.x() >= legend.left() + legend.width()) {
        return -1;
    }
    if (point.y() < legend.top()) {
        return -1;
    }
    const int index = (point.y() - legend.top()) / kLegendRowHeight;
    if (index >= sliceCount) {
        return -1;
    }
    return index;
}

[[nodiscard]] int wedgeIndexAt(const QRectF& disk, const std::vector<ChartSlice>& slices,
                               QPointF pos) {
    const auto total = sliceTotal(slices);
    const double radius = disk.width() / 2.0;
    if (total <= 0 || radius <= 0.0) {
        return -1;
    }
    const double dx = pos.x() - disk.center().x();
    const double dy = pos.y() - disk.center().y();
    if (dx * dx + dy * dy > radius * radius) {
        return -1;
    }
    // atan2(dx, -dy) is clockwise from 12 o'clock because screen y grows downward.
    double turns = std::atan2(dx, -dy) / (2.0 * std::numbers::pi);
    if (turns < 0.0) {
        turns += 1.0;
    }
    double cursor = 0.0;
    for (int index = 0; index < static_cast<int>(slices.size()); ++index) {
        cursor += static_cast<double>(slices[static_cast<std::size_t>(index)].seconds) /
                  static_cast<double>(total);
        if (turns <= cursor || index + 1 == static_cast<int>(slices.size())) {
            return index;
        }
    }
    return -1;
}

[[nodiscard]] int pieIndexAt(const PieLayout& layout, const std::vector<ChartSlice>& slices,
                             QPointF pos) {
    const int fromLegend = legendIndexAt(layout.legend, static_cast<int>(slices.size()), pos);
    if (fromLegend >= 0) {
        return fromLegend;
    }
    return wedgeIndexAt(layout.disk, slices, pos);
}

[[nodiscard]] std::int64_t widestSeconds(const std::vector<ChartSlice>& slices) {
    std::int64_t widest = 0;
    for (const auto& slice : slices) {
        widest = std::max(widest, slice.seconds);
    }
    return widest;
}

struct BarRowGeom {
    QRect area;
    int index{0};
    int labelWidth{0};
    int valueWidth{0};
    int trackWidth{0};
    std::int64_t maxSeconds{0};
};

void paintBarTrack(QPainter& painter, const BarRowGeom& geom, const ChartSlice& slice, int top) {
    if (geom.trackWidth <= 0) {
        return;
    }
    constexpr int kBarThickness = 12;
    const int trackLeft = geom.area.left() + geom.labelWidth + 8;
    const int barTop = top + (kBarRowHeight - kBarThickness) / 2;
    const QRect track(trackLeft, barTop, geom.trackWidth, kBarThickness);
    auto fill = sliceColor(geom.index);
    auto trackColor = fill;
    trackColor.setAlpha(48);
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, 3, 3);
    const auto width = static_cast<std::int64_t>(geom.trackWidth);
    const auto scaled = width * slice.seconds / geom.maxSeconds;
    const auto filled = static_cast<int>(scaled);
    if (filled <= 0) {
        return;
    }
    const QRect bar(trackLeft, barTop, std::min(filled, geom.trackWidth), kBarThickness);
    painter.setBrush(fill);
    painter.drawRoundedRect(bar, 3, 3);
}

void paintBarRow(QPainter& painter, const BarRowGeom& geom, const ChartSlice& slice,
                 const QColor& textColor) {
    const int top = geom.area.top() + geom.index * kBarRowHeight;
    const QFontMetrics metrics(painter.font());
    painter.setPen(textColor);
    painter.drawText(QRect(geom.area.left(), top, geom.labelWidth, kBarRowHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(slice.label, Qt::ElideRight, geom.labelWidth));
    paintBarTrack(painter, geom, slice, top);
    const int valueLeft = geom.area.left() + geom.labelWidth + 8 + geom.trackWidth + 8;
    painter.setPen(textColor);
    const auto duration = QString::fromStdString(formatDuration(slice.seconds));
    painter.drawText(QRect(valueLeft, top, geom.valueWidth, kBarRowHeight),
                     Qt::AlignRight | Qt::AlignVCenter, duration);
}

}  // namespace

class BarChartBody final : public QWidget {
public:
    explicit BarChartBody(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }

    void setSlices(std::vector<ChartSlice> slices) {
        slices_ = std::move(slices);
        const int height = slices_.empty()
                               ? 0
                               : kBarPadding * 2 + static_cast<int>(slices_.size()) * kBarRowHeight;
        setMinimumHeight(height);
        updateGeometry();
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        if (slices_.empty()) {
            return;
        }
        const auto maxSeconds = widestSeconds(slices_);
        if (maxSeconds <= 0) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRect area = rect().adjusted(kBarPadding, kBarPadding, -kBarPadding, -kBarPadding);
        const int labelWidth = std::max(72, area.width() * 2 / 5);
        const int trackWidth = std::max(0, area.width() - labelWidth - kBarValueWidth - 16);
        const auto textColor = palette().color(QPalette::WindowText);
        for (int index = 0; index < static_cast<int>(slices_.size()); ++index) {
            const BarRowGeom geom{area, index, labelWidth, kBarValueWidth, trackWidth, maxSeconds};
            paintBarRow(painter, geom, slices_[static_cast<std::size_t>(index)], textColor);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const int row = rowAt(event->position().toPoint().y());
        showSliceTip(this, event->globalPosition().toPoint(), slices_, row);
    }

    void leaveEvent(QEvent* event) override {
        QWidget::leaveEvent(event);
        QToolTip::hideText();
    }

private:
    [[nodiscard]] int rowAt(int y) const {
        if (y < kBarPadding) {
            return -1;
        }
        const int index = (y - kBarPadding) / kBarRowHeight;
        if (index >= static_cast<int>(slices_.size())) {
            return -1;
        }
        return index;
    }

    std::vector<ChartSlice> slices_;
};

PieChart::PieChart(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }

void PieChart::setSlices(std::vector<ChartSlice> slices) {
    slices_ = std::move(slices);
    update();
}

void PieChart::setTitle(const QString& title) {
    title_ = title;
    update();
}

void PieChart::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect bounds = paddedBounds(rect());
    painter.setPen(palette().color(QPalette::WindowText));
    paintHeading(painter, bounds, title_, font());
    if (slices_.empty()) {
        return;
    }
    const auto layout = pieLayout(belowHeading(bounds), static_cast<int>(slices_.size()));
    paintWedges(painter, layout.disk, slices_);
    paintLegend(painter, layout.legend, slices_, palette().color(QPalette::WindowText));
}

int PieChart::sliceAt(QPointF pos) const {
    const QRect content = belowHeading(paddedBounds(rect()));
    const auto layout = pieLayout(content, static_cast<int>(slices_.size()));
    return pieIndexAt(layout, slices_, pos);
}

void PieChart::mouseMoveEvent(QMouseEvent* event) {
    showSliceTip(this, event->globalPosition().toPoint(), slices_, sliceAt(event->position()));
}

void PieChart::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    QToolTip::hideText();
}

BarChart::BarChart(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kOuterPadding, kOuterPadding, kOuterPadding, kOuterPadding);
    layout->setSpacing(kContentTopGap);
    titleLabel_ = new QLabel(this);
    QFont heading = titleLabel_->font();
    heading.setBold(true);
    titleLabel_->setFont(heading);
    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body_ = new BarChartBody(scroll_);
    scroll_->setWidget(body_);
    layout->addWidget(titleLabel_);
    layout->addWidget(scroll_, 1);
}

void BarChart::setSlices(std::vector<ChartSlice> slices) { body_->setSlices(std::move(slices)); }

void BarChart::setTitle(const QString& title) {
    titleLabel_->setText(title);
    titleLabel_->setVisible(!title.isEmpty());
}

}  // namespace timetracker::ui
