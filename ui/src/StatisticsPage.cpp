#include "timetracker/ui/StatisticsPage.hpp"

#include "ShareChart.hpp"
#include "timetracker/time/StatsRange.hpp"
#include "timetracker/time/TimeFormat.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace timetracker::ui {
namespace {

constexpr int kChartRowHeight = 260;
constexpr int kPieSliceCap = 8;

enum class ParentLabel { Omit, Include };

[[nodiscard]] QString percentText(int percent) { return QString::number(percent) + QStringLiteral("%"); }

[[nodiscard]] int percentOf(std::int64_t part, std::int64_t total) {
    if (total <= 0 || part <= 0) {
        return 0;
    }
    return static_cast<int>((part * 100 + total / 2) / total);
}

[[nodiscard]] ChartSlice makeSlice(QString label, std::int64_t seconds, std::int64_t total) {
    return ChartSlice{std::move(label), seconds, percentOf(seconds, total)};
}

void sortBySeconds(std::vector<ChartSlice>& slices) {
    auto bySeconds = [](const ChartSlice& left, const ChartSlice& right) {
        return left.seconds > right.seconds;
    };
    std::stable_sort(slices.begin(), slices.end(), bySeconds);
}

[[nodiscard]] QString sliceLabel(ParentLabel parentLabel, const std::string& parent,
                                 const std::string& name) {
    if (parentLabel == ParentLabel::Omit) {
        return QString::fromStdString(name);
    }
    return QStringLiteral("%1 \u2014 %2")
        .arg(QString::fromStdString(parent), QString::fromStdString(name));
}

[[nodiscard]] bool several(const StatsReport& report, const std::string StatsLine::* field) {
    const std::string* first = nullptr;
    for (const auto& line : report.lines) {
        if (line.kind != StatsLineKind::MainTotal) {
            continue;
        }
        if (first == nullptr) {
            first = &(line.*field);
            continue;
        }
        if (line.*field != *first) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<ChartSlice> categorySlices(const StatsReport& report) {
    std::vector<ChartSlice> slices;
    for (const auto& line : report.lines) {
        if (line.kind != StatsLineKind::MainTotal) {
            continue;
        }
        const auto label = QString::fromStdString(line.categoryName);
        const auto found = std::find_if(slices.begin(), slices.end(), [&](const ChartSlice& slice) {
            return slice.label == label;
        });
        if (found == slices.end()) {
            slices.push_back(makeSlice(label, line.seconds, report.totalSeconds));
            continue;
        }
        found->seconds += line.seconds;
        found->percent = percentOf(found->seconds, report.totalSeconds);
    }
    sortBySeconds(slices);
    return slices;
}

[[nodiscard]] std::vector<ChartSlice> mainTopicSlices(const StatsReport& report,
                                                      ParentLabel parentLabel) {
    std::vector<ChartSlice> slices;
    for (const auto& line : report.lines) {
        if (line.kind != StatsLineKind::MainTotal) {
            continue;
        }
        const auto label = sliceLabel(parentLabel, line.categoryName, line.mainTopicName);
        slices.push_back(makeSlice(label, line.seconds, report.totalSeconds));
    }
    sortBySeconds(slices);
    return slices;
}

[[nodiscard]] std::vector<ChartSlice> leafSlices(const StatsReport& report,
                                                 ParentLabel parentLabel) {
    std::vector<ChartSlice> slices;
    for (const auto& line : report.lines) {
        if (line.kind == StatsLineKind::MainTotal) {
            continue;
        }
        const auto label = sliceLabel(parentLabel, line.mainTopicName, line.detail);
        slices.push_back(makeSlice(label, line.seconds, report.totalSeconds));
    }
    sortBySeconds(slices);
    return slices;
}

void collapsePieTail(std::vector<ChartSlice>& slices, std::int64_t total) {
    // Nine slices still fit: the one past the cap keeps its name. Ten or more
    // fold the tail into Other so the pie has eight wedges.
    if (slices.size() <= static_cast<std::size_t>(kPieSliceCap + 1)) {
        return;
    }
    ChartSlice other;
    other.label = QStringLiteral("Other");
    const auto tail = static_cast<std::size_t>(kPieSliceCap - 1);
    for (std::size_t index = tail; index < slices.size(); ++index) {
        other.seconds += slices[index].seconds;
    }
    other.percent = percentOf(other.seconds, total);
    slices.resize(static_cast<std::size_t>(kPieSliceCap - 1));
    slices.push_back(std::move(other));
}

struct ChartSeries {
    QString pieTitle;
    std::vector<ChartSlice> pie;
    QString barTitle;
    std::vector<ChartSlice> bars;
};

[[nodiscard]] ChartSeries chartSeries(const StatsReport& report) {
    ChartSeries series;
    if (report.totalSeconds <= 0) {
        return series;
    }
    if (several(report, &StatsLine::categoryName)) {
        series.pieTitle = QStringLiteral("By category");
        series.pie = categorySlices(report);
        series.barTitle = QStringLiteral("By main topic");
        series.bars = mainTopicSlices(report, ParentLabel::Include);
    } else {
        series.pieTitle = QStringLiteral("By main topic");
        series.pie = mainTopicSlices(report, ParentLabel::Omit);
        series.barTitle = QStringLiteral("By subtopic");
        const auto parent =
            several(report, &StatsLine::mainTopicName) ? ParentLabel::Include : ParentLabel::Omit;
        series.bars = leafSlices(report, parent);
    }
    collapsePieTail(series.pie, report.totalSeconds);
    return series;
}

}  // namespace

StatisticsPage::StatisticsPage(StatsService& stats, QWidget* parent) : QWidget(parent), stats_(stats) {
    buildLayout();
}

void StatisticsPage::buildLayout() {
    presetCombo_ = new QComboBox(this);
    presetCombo_->addItem(QStringLiteral("Today"), static_cast<int>(StatsPreset::Today));
    presetCombo_->addItem(QStringLiteral("This week"), static_cast<int>(StatsPreset::ThisWeek));
    presetCombo_->addItem(QStringLiteral("This month"), static_cast<int>(StatsPreset::ThisMonth));
    presetCombo_->addItem(QStringLiteral("All time"), static_cast<int>(StatsPreset::AllTime));
    presetCombo_->addItem(QStringLiteral("Custom"), static_cast<int>(StatsPreset::Custom));

    fromLabel_ = new QLabel(QStringLiteral("From"), this);
    toLabel_ = new QLabel(QStringLiteral("To"), this);
    fromEdit_ = new QDateEdit(QDate::currentDate(), this);
    toEdit_ = new QDateEdit(QDate::currentDate(), this);
    fromEdit_->setCalendarPopup(true);
    toEdit_->setCalendarPopup(true);
    fromEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    toEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    categoryCombo_ = new QComboBox(this);

    auto* filters = new QHBoxLayout();
    filters->addWidget(presetCombo_);
    filters->addWidget(fromLabel_);
    filters->addWidget(fromEdit_);
    filters->addWidget(toLabel_);
    filters->addWidget(toEdit_);
    filters->addWidget(categoryCombo_);
    filters->addStretch();

    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels({QStringLiteral("Category"), QStringLiteral("Main topic"),
                                       QStringLiteral("Subtopic"), QStringLiteral("Time"),
                                       QStringLiteral("Percent")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setAlternatingRowColors(true);

    pieChart_ = new PieChart(this);
    barChart_ = new BarChart(this);
    chartsRow_ = new QWidget(this);
    auto* charts = new QHBoxLayout(chartsRow_);
    charts->setContentsMargins(0, 0, 0, 0);
    charts->addWidget(pieChart_, 1);
    charts->addWidget(barChart_, 1);
    chartsRow_->setFixedHeight(kChartRowHeight);
    chartsRow_->setVisible(false);

    totalLabel_ = new QLabel(QStringLiteral("Total: 0:00:00"), this);
    statusLabel_ = new QLabel(QStringLiteral("No time in this range."), this);
    statusLabel_->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(filters);
    layout->addWidget(chartsRow_);
    layout->addWidget(table_, 1);
    layout->addWidget(totalLabel_);
    layout->addWidget(statusLabel_);

    connect(presetCombo_, &QComboBox::currentIndexChanged, this, &StatisticsPage::onPresetChanged);
    connect(categoryCombo_, &QComboBox::currentIndexChanged, this, &StatisticsPage::refresh);
    connect(fromEdit_, &QDateEdit::dateChanged, this, &StatisticsPage::refresh);
    connect(toEdit_, &QDateEdit::dateChanged, this, &StatisticsPage::refresh);
    onPresetChanged();
}

void StatisticsPage::setSnapshot(const CatalogSnapshot& snapshot) {
    const auto selected = categoryCombo_->currentData();
    categoryCombo_->blockSignals(true);
    categoryCombo_->clear();
    categoryCombo_->addItem(QStringLiteral("All categories"), QVariant());
    for (const auto& category : snapshot.categories) {
        categoryCombo_->addItem(QString::fromStdString(category.name),
                                static_cast<qlonglong>(category.id));
    }
    const int index = categoryCombo_->findData(selected);
    categoryCombo_->setCurrentIndex(index >= 0 ? index : 0);
    categoryCombo_->blockSignals(false);
}

void StatisticsPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
}

void StatisticsPage::onPresetChanged() {
    const auto preset = static_cast<StatsPreset>(presetCombo_->currentData().toInt());
    const bool custom = preset == StatsPreset::Custom;
    fromLabel_->setVisible(custom);
    toLabel_->setVisible(custom);
    fromEdit_->setVisible(custom);
    toEdit_->setVisible(custom);
    refresh();
}

void StatisticsPage::showStatus(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setVisible(!text.isEmpty());
}

void StatisticsPage::fillTable(const StatsReport& report) {
    table_->setUpdatesEnabled(false);
    table_->setRowCount(static_cast<int>(report.lines.size()));
    for (int row = 0; row < static_cast<int>(report.lines.size()); ++row) {
        const auto& line = report.lines[static_cast<std::size_t>(row)];
        auto* category = new QTableWidgetItem(QString::fromStdString(line.categoryName));
        auto* mainTopic = new QTableWidgetItem(QString::fromStdString(line.mainTopicName));
        auto* detail = new QTableWidgetItem(QString::fromStdString(line.detail));
        auto* time = new QTableWidgetItem(QString::fromStdString(formatDuration(line.seconds)));
        auto* percent = new QTableWidgetItem(percentText(line.percent));
        if (line.kind == StatsLineKind::MainTotal) {
            QFont font = category->font();
            font.setBold(true);
            category->setFont(font);
            mainTopic->setFont(font);
            detail->setFont(font);
            time->setFont(font);
            percent->setFont(font);
        }
        table_->setItem(row, 0, category);
        table_->setItem(row, 1, mainTopic);
        table_->setItem(row, 2, detail);
        table_->setItem(row, 3, time);
        table_->setItem(row, 4, percent);
    }
    table_->setUpdatesEnabled(true);
    totalLabel_->setText(
        QStringLiteral("Total: %1").arg(QString::fromStdString(formatDuration(report.totalSeconds))));
    table_->setVisible(!report.lines.empty());
    showStatus(report.lines.empty() ? QStringLiteral("No time in this range.") : QString());
    fillCharts(report);
}

void StatisticsPage::fillCharts(const StatsReport& report) {
    auto series = chartSeries(report);
    if (series.pie.empty() && series.bars.empty()) {
        clearCharts();
        return;
    }
    pieChart_->setTitle(series.pieTitle);
    barChart_->setTitle(series.barTitle);
    pieChart_->setSlices(std::move(series.pie));
    barChart_->setSlices(std::move(series.bars));
    chartsRow_->setVisible(true);
}

void StatisticsPage::clearCharts() {
    pieChart_->setTitle({});
    pieChart_->setSlices({});
    barChart_->setTitle({});
    barChart_->setSlices({});
    chartsRow_->setVisible(false);
}

void StatisticsPage::refresh() {
    const auto preset = static_cast<StatsPreset>(presetCombo_->currentData().toInt());
    const QDate today = QDate::currentDate();
    const CivilDate localToday{today.year(), today.month(), today.day()};
    const int offset = QDateTime::currentDateTime().offsetFromUtc() / 60;
    const QDate from = fromEdit_->date();
    const QDate to = toEdit_->date();
    auto range = boundsForPreset(preset, localToday, offset, CivilDate{from.year(), from.month(), from.day()},
                                 CivilDate{to.year(), to.month(), to.day()});
    if (!range) {
        table_->setRowCount(0);
        totalLabel_->setText(QStringLiteral("Total: 0:00:00"));
        clearCharts();
        showStatus(QString::fromStdString(range.error()));
        return;
    }
    std::optional<std::int64_t> categoryId;
    if (categoryCombo_->currentData().isValid()) {
        categoryId = categoryCombo_->currentData().toLongLong();
    }
    auto report = stats_.summarize(range.value(), categoryId);
    if (!report) {
        table_->setRowCount(0);
        clearCharts();
        showStatus(QString::fromStdString(report.error()));
        return;
    }
    fillTable(report.value());
}

}  // namespace timetracker::ui
