#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/services/StatsService.hpp"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QTableWidget;

namespace timetracker::ui {

class BarChart;
class PieChart;

/**
 * @brief Show time spent as a pie chart, a bar chart, and a totals table.
 *
 * The statistics service must outlive this page. Day boundaries use the
 * computer's local time zone. Chart slices partition the filtered total;
 * the selection rules are in docs/time-tracking.md.
 */
class StatisticsPage final : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Build the statistics page.
     * @param stats Statistics service.
     * @param parent Parent widget.
     */
    explicit StatisticsPage(StatsService& stats, QWidget* parent = nullptr);

    /**
     * @brief Refresh the category filter from the catalog.
     * @param snapshot Current catalog.
     */
    void setSnapshot(const CatalogSnapshot& snapshot);

    /**
     * @brief Recompute the charts and table for the selected range.
     */
    void refresh();

protected:
    /**
     * @brief Refresh when the page becomes visible.
     * @param event Show event.
     */
    void showEvent(QShowEvent* event) override;

private:
    void buildLayout();
    void onPresetChanged();
    void fillTable(const StatsReport& report);

    /**
     * @brief Replace the pie and bar charts from one report.
     * @param report Totals for the selected range. An empty total hides the charts.
     */
    void fillCharts(const StatsReport& report);

    /**
     * @brief Remove chart slices and hide the chart row.
     */
    void clearCharts();

    void showStatus(const QString& text);

    StatsService& stats_;
    QComboBox* presetCombo_{nullptr};
    QDateEdit* fromEdit_{nullptr};
    QDateEdit* toEdit_{nullptr};
    QLabel* fromLabel_{nullptr};
    QLabel* toLabel_{nullptr};
    QComboBox* categoryCombo_{nullptr};
    PieChart* pieChart_{nullptr};
    BarChart* barChart_{nullptr};
    QWidget* chartsRow_{nullptr};
    QTableWidget* table_{nullptr};
    QLabel* totalLabel_{nullptr};
    QLabel* statusLabel_{nullptr};
};

}  // namespace timetracker::ui
