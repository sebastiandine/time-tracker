#pragma once

#include <QString>
#include <QWidget>

#include <cstdint>
#include <vector>

class QLabel;
class QScrollArea;

namespace timetracker::ui {

class BarChartBody;

/**
 * @brief One non-overlapping share of a statistics total.
 *
 * @c percent is this slice's share of the chart total, rounded, from 0 to 100.
 */
struct ChartSlice {
    QString label;
    std::int64_t seconds{0}; /**< Duration in whole seconds. */
    int percent{0};
};

/**
 * @brief Pie chart of named durations, with a legend.
 *
 * The caller passes slices that already partition one total. Hovering a wedge
 * or a legend row shows that slice's duration. An empty slice list draws only
 * the title.
 */
class PieChart final : public QWidget {
public:
    /**
     * @brief Build an empty pie chart.
     * @param parent Parent widget.
     */
    explicit PieChart(QWidget* parent = nullptr);

    /**
     * @brief Replace the wedges and repaint.
     * @param slices Shares in draw order, largest first. An empty list clears the wedges.
     */
    void setSlices(std::vector<ChartSlice> slices);

    /**
     * @brief Set the heading drawn above the pie.
     * @param title Heading text. Empty leaves the heading blank.
     */
    void setTitle(const QString& title);

protected:
    /**
     * @brief Draw the heading, wedges, and legend.
     * @param event Paint event. Unused.
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief Show a tooltip for the wedge or legend row under the pointer.
     * @param event Mouse move. The widget tracks the mouse.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief Hide the slice tooltip when the pointer leaves.
     * @param event Leave event.
     */
    void leaveEvent(QEvent* event) override;

private:
    [[nodiscard]] int sliceAt(QPointF pos) const;

    QString title_;
    std::vector<ChartSlice> slices_;
};

/**
 * @brief Horizontal bar chart of named durations.
 *
 * The longest slice fills the track. Rows scroll when they are taller than
 * the widget. Hovering a row shows that slice's duration.
 */
class BarChart final : public QWidget {
public:
    /**
     * @brief Build an empty bar chart with a fixed heading and a scrolling plot.
     * @param parent Parent widget.
     */
    explicit BarChart(QWidget* parent = nullptr);

    /**
     * @brief Replace the bars and update the scroll height.
     * @param slices Shares in draw order, longest first. An empty list clears the plot.
     */
    void setSlices(std::vector<ChartSlice> slices);

    /**
     * @brief Set the heading above the bars.
     * @param title Heading text. Empty hides the heading.
     */
    void setTitle(const QString& title);

private:
    QLabel* titleLabel_{nullptr};
    QScrollArea* scroll_{nullptr};
    BarChartBody* body_{nullptr};
};

}  // namespace timetracker::ui
