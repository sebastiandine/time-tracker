#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/services/TrackingService.hpp"

#include <QWidget>

#include <optional>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QTreeWidget;

namespace timetracker::ui {

/**
 * @brief Start and stop the single timer and review recent entries.
 *
 * The tracking service must outlive this page. Closing the window does not
 * stop the timer; the open row stays in the database.
 */
class TrackPage final : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Build the track page.
     * @param tracking Timer service.
     * @param parent Parent widget.
     */
    explicit TrackPage(TrackingService& tracking, QWidget* parent = nullptr);

    /**
     * @brief Replace the topic tree. A running note is left as the user typed it.
     * @param snapshot Current catalog.
     */
    void setSnapshot(const CatalogSnapshot& snapshot);

    /**
     * @brief Show the open entry, or the idle state when nothing is running.
     * @param entry Open entry, or empty.
     */
    void showSession(const std::optional<TimeEntry>& entry);

    /**
     * @brief Refresh the elapsed label from the stored start time.
     */
    void updateClock();

signals:
    /**
     * @brief A timer was started, stopped, or a closed entry was edited or deleted.
     */
    void sessionChanged();

private:
    void buildLayout();
    void onSelectionChanged();
    void onStart();
    void onStop();
    void saveRunningNote();
    void reloadRecent();
    void refreshButtons();
    void onEditNote();
    void onDeleteEntry();
    void setNoteText(const QString& text);

    TrackingService& tracking_;
    CatalogSnapshot snapshot_;
    std::optional<TimeEntry> running_;
    bool suppressNote_{false};
    bool suppressSelection_{false};
    QTreeWidget* tree_{nullptr};
    QLabel* stateLabel_{nullptr};
    QLabel* elapsedLabel_{nullptr};
    QLabel* hintLabel_{nullptr};
    QPlainTextEdit* noteEdit_{nullptr};
    QPushButton* startButton_{nullptr};
    QPushButton* stopButton_{nullptr};
    QTableWidget* recentTable_{nullptr};
    QLabel* recentHint_{nullptr};
    QPushButton* editButton_{nullptr};
    QPushButton* deleteButton_{nullptr};
    QTimer* noteTimer_{nullptr};
};

}  // namespace timetracker::ui
