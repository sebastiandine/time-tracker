#include "timetracker/ui/TrackPage.hpp"

#include "CatalogTree.hpp"
#include "UiUtil.hpp"
#include "timetracker/services/TrackingService.hpp"
#include "timetracker/time/TimeFormat.hpp"
#include "timetracker/ui/NameNotesDialog.hpp"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace timetracker::ui {
namespace {

[[nodiscard]] QString localStamp(const std::string& utc) {
    auto parsed = QDateTime::fromString(QString::fromStdString(utc), Qt::ISODate);
    if (!parsed.isValid()) {
        return QString::fromStdString(utc);
    }
    return parsed.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

[[nodiscard]] std::optional<TimeTarget> targetFrom(const std::optional<TreeSelection>& selection) {
    if (!selection.has_value() || selection->kind == TreeNodeKind::Category) {
        return std::nullopt;
    }
    TimeTarget target;
    target.kind = selection->kind == TreeNodeKind::MainTopic ? TimeTargetKind::MainTopic
                                                             : TimeTargetKind::SubTopic;
    target.id = selection->id;
    return target;
}

}  // namespace

TrackPage::TrackPage(TrackingService& tracking, QWidget* parent)
    : QWidget(parent), tracking_(tracking) {
    buildLayout();
}

void TrackPage::buildLayout() {
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);

    stateLabel_ = new QLabel(QStringLiteral("Select a main topic or subtopic and press Start."), this);
    elapsedLabel_ = new QLabel(QStringLiteral("0:00:00"), this);
    QFont clockFont = elapsedLabel_->font();
    clockFont.setPointSize(28);
    clockFont.setBold(true);
    elapsedLabel_->setFont(clockFont);
    hintLabel_ = new QLabel(
        QStringLiteral("Time keeps counting while the app is closed or the computer is off, until you press Stop."),
        this);
    hintLabel_->setWordWrap(true);
    noteEdit_ = new QPlainTextEdit(this);
    noteEdit_->setPlaceholderText(QStringLiteral("Note for this time record"));
    startButton_ = new QPushButton(QStringLiteral("Start"), this);
    stopButton_ = new QPushButton(QStringLiteral("Stop"), this);
    auto* timerButtons = new QHBoxLayout();
    timerButtons->addWidget(startButton_);
    timerButtons->addWidget(stopButton_);
    timerButtons->addStretch();

    recentTable_ = new QTableWidget(0, 4, this);
    recentTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Start"), QStringLiteral("End"), QStringLiteral("Duration"),
         QStringLiteral("Note")});
    recentTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    recentTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recentTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    recentTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recentHint_ = new QLabel(this);
    editButton_ = new QPushButton(QStringLiteral("Edit note"), this);
    deleteButton_ = new QPushButton(QStringLiteral("Delete entry"), this);
    auto* historyButtons = new QHBoxLayout();
    historyButtons->addWidget(editButton_);
    historyButtons->addWidget(deleteButton_);
    historyButtons->addStretch();

    auto* right = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->addWidget(stateLabel_);
    rightLayout->addWidget(elapsedLabel_);
    rightLayout->addWidget(hintLabel_);
    rightLayout->addWidget(noteEdit_);
    rightLayout->addLayout(timerButtons);
    rightLayout->addWidget(recentTable_);
    rightLayout->addWidget(recentHint_);
    rightLayout->addLayout(historyButtons);

    auto* splitter = new QSplitter(this);
    splitter->addWidget(tree_);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(splitter);

    noteTimer_ = new QTimer(this);
    noteTimer_->setSingleShot(true);
    noteTimer_->setInterval(400);

    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &TrackPage::onSelectionChanged);
    connect(recentTable_, &QTableWidget::itemSelectionChanged, this, &TrackPage::refreshButtons);
    connect(startButton_, &QPushButton::clicked, this, &TrackPage::onStart);
    connect(stopButton_, &QPushButton::clicked, this, &TrackPage::onStop);
    connect(editButton_, &QPushButton::clicked, this, &TrackPage::onEditNote);
    connect(deleteButton_, &QPushButton::clicked, this, &TrackPage::onDeleteEntry);
    connect(noteEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        if (running_.has_value() && !suppressNote_) {
            noteTimer_->start();
        }
    });
    connect(noteTimer_, &QTimer::timeout, this, &TrackPage::saveRunningNote);
    refreshButtons();
}

void TrackPage::setSnapshot(const CatalogSnapshot& snapshot) {
    snapshot_ = snapshot;
    const auto current = readSelection(tree_->currentItem());
    suppressSelection_ = true;
    fillCatalogTree(tree_, snapshot_);
    if (running_.has_value()) {
        if (running_->subTopicId.has_value()) {
            selectNode(tree_, TreeNodeKind::SubTopic, *running_->subTopicId);
        } else if (running_->mainTopicId.has_value()) {
            selectNode(tree_, TreeNodeKind::MainTopic, *running_->mainTopicId);
        }
    } else if (current.has_value()) {
        selectNode(tree_, current->kind, current->id);
    }
    suppressSelection_ = false;
    reloadRecent();
    updateClock();
}

void TrackPage::showSession(const std::optional<TimeEntry>& entry) {
    running_ = entry;
    noteTimer_->stop();
    if (entry.has_value()) {
        setNoteText(QString::fromStdString(entry->notes));
        suppressSelection_ = true;
        if (entry->subTopicId.has_value()) {
            selectNode(tree_, TreeNodeKind::SubTopic, *entry->subTopicId);
        } else if (entry->mainTopicId.has_value()) {
            selectNode(tree_, TreeNodeKind::MainTopic, *entry->mainTopicId);
        }
        suppressSelection_ = false;
    } else {
        setNoteText(QString());
    }
    updateClock();
    refreshButtons();
    reloadRecent();
}

void TrackPage::setNoteText(const QString& text) {
    suppressNote_ = true;
    noteEdit_->setPlainText(text);
    suppressNote_ = false;
}

void TrackPage::updateClock() {
    if (!running_.has_value()) {
        elapsedLabel_->setText(QStringLiteral("0:00:00"));
        stateLabel_->setText(QStringLiteral("Select a main topic or subtopic and press Start."));
        return;
    }
    auto elapsed = tracking_.elapsedSeconds();
    const auto text = elapsed ? formatDuration(elapsed.value()) : std::string("0:00:00");
    elapsedLabel_->setText(QString::fromStdString(text));
    stateLabel_->setText(QStringLiteral("Recording %1").arg(topicPath(snapshot_, *running_)));
}

void TrackPage::onSelectionChanged() {
    if (suppressSelection_) {
        return;
    }
    reloadRecent();
    refreshButtons();
    if (!running_.has_value()) {
        setNoteText(QString());
    }
}

void TrackPage::refreshButtons() {
    const auto target = targetFrom(readSelection(tree_->currentItem()));
    startButton_->setEnabled(!running_.has_value() && target.has_value());
    stopButton_->setEnabled(running_.has_value());
    const bool row = recentTable_->currentRow() >= 0;
    editButton_->setEnabled(row);
    deleteButton_->setEnabled(row);
}

void TrackPage::onStart() {
    const auto target = targetFrom(readSelection(tree_->currentItem()));
    if (!target.has_value()) {
        return;
    }
    noteTimer_->stop();
    auto started = tracking_.start(*target, noteEdit_->toPlainText().toStdString());
    if (!started) {
        showError(this, started.error());
        return;
    }
    emit sessionChanged();
}

void TrackPage::onStop() {
    noteTimer_->stop();
    auto stopped = tracking_.stop(noteEdit_->toPlainText().toStdString());
    if (!stopped) {
        showError(this, stopped.error());
        return;
    }
    emit sessionChanged();
}

void TrackPage::saveRunningNote() {
    if (!running_.has_value()) {
        return;
    }
    auto saved = tracking_.updateRunningNote(noteEdit_->toPlainText().toStdString());
    if (!saved) {
        showError(this, saved.error());
    }
}

void TrackPage::reloadRecent() {
    recentTable_->setRowCount(0);
    recentHint_->clear();
    const auto target = targetFrom(readSelection(tree_->currentItem()));
    if (!target.has_value()) {
        refreshButtons();
        return;
    }
    auto page = tracking_.listRecent(*target, kRecentPageSize, std::nullopt);
    if (!page) {
        showError(this, page.error());
        return;
    }
    recentTable_->setRowCount(static_cast<int>(page.value().size()));
    for (int row = 0; row < static_cast<int>(page.value().size()); ++row) {
        const auto& entry = page.value()[static_cast<std::size_t>(row)];
        auto* start = new QTableWidgetItem(localStamp(entry.startedAt));
        start->setData(Qt::UserRole, static_cast<qlonglong>(entry.id));
        recentTable_->setItem(row, 0, start);
        const auto endText = entry.endedAt.has_value() ? localStamp(*entry.endedAt) : QString();
        recentTable_->setItem(row, 1, new QTableWidgetItem(endText));
        QString duration = QStringLiteral("0:00:00");
        if (entry.endedAt.has_value()) {
            auto seconds = secondsBetweenUtc(entry.startedAt, *entry.endedAt);
            if (seconds) {
                duration = QString::fromStdString(formatDuration(seconds.value()));
            }
        }
        recentTable_->setItem(row, 2, new QTableWidgetItem(duration));
        recentTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(entry.notes)));
    }
    if (page.value().size() == static_cast<std::size_t>(kRecentPageSize)) {
        recentHint_->setText(QStringLiteral("Showing the latest 50 entries."));
    }
    refreshButtons();
}

void TrackPage::onEditNote() {
    const int row = recentTable_->currentRow();
    if (row < 0 || recentTable_->item(row, 0) == nullptr) {
        return;
    }
    const auto id = recentTable_->item(row, 0)->data(Qt::UserRole).toLongLong();
    const auto current = recentTable_->item(row, 3) != nullptr ? recentTable_->item(row, 3)->text()
                                                               : QString();
    auto note = NameNotesDialog::askNote(this, current);
    if (!note.has_value()) {
        return;
    }
    auto updated = tracking_.updateClosedNote(id, note->toStdString());
    if (!updated) {
        showError(this, updated.error());
        return;
    }
    emit sessionChanged();
}

void TrackPage::onDeleteEntry() {
    const int row = recentTable_->currentRow();
    if (row < 0 || recentTable_->item(row, 0) == nullptr) {
        return;
    }
    if (!confirmDelete(this, QStringLiteral("this time entry"))) {
        return;
    }
    const auto id = recentTable_->item(row, 0)->data(Qt::UserRole).toLongLong();
    auto deleted = tracking_.deleteEntry(id);
    if (!deleted) {
        showError(this, deleted.error());
        return;
    }
    emit sessionChanged();
}

}  // namespace timetracker::ui
