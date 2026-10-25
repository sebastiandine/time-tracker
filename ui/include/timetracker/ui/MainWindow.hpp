#pragma once

#include "timetracker/domain/Records.hpp"
#include "timetracker/services/CatalogService.hpp"
#include "timetracker/services/DataLocationService.hpp"
#include "timetracker/services/SettingsService.hpp"
#include "timetracker/services/StatsService.hpp"
#include "timetracker/services/TrackingService.hpp"

#include <QMainWindow>
#include <QString>

class QAction;
class QTabWidget;
class QTimer;

namespace timetracker::ui {

class LibraryPage;
class StatisticsPage;
class TrackPage;

/**
 * @brief The application window.
 *
 * Services must outlive the window. Closing it does not stop a running timer.
 */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Build the window and restore an open timer if one exists.
     * @param catalog Catalog service.
     * @param tracking Timer service.
     * @param stats Statistics service.
     * @param settings Theme store.
     * @param locations Data directory. A menu choice reopens the database.
     * @param version Version text for the title.
     * @param parent Parent widget. Normally null.
     */
    MainWindow(CatalogService& catalog, TrackingService& tracking, StatsService& stats,
               SettingsService& settings, DataLocationService& locations, QString version,
               QWidget* parent = nullptr);

private:
    void buildUi();
    void buildDataMenu();
    void buildThemeMenu();
    void loadTheme();
    void chooseDataLocation();
    void reloadCatalog();
    void restoreSession(bool switchToTrack);
    void updateStatusBar();
    void chooseTheme(ThemePreference theme);
    void syncThemeChecks();
    void onTick();

    CatalogService& catalog_;
    TrackingService& tracking_;
    StatsService& stats_;
    SettingsService& settings_;
    DataLocationService& locations_;
    QString version_;
    ThemePreference theme_{ThemePreference::System};
    CatalogSnapshot snapshot_;
    QTabWidget* tabs_{nullptr};
    LibraryPage* library_{nullptr};
    TrackPage* track_{nullptr};
    StatisticsPage* statistics_{nullptr};
    QTimer* clockTimer_{nullptr};
    QAction* systemTheme_{nullptr};
    QAction* lightTheme_{nullptr};
    QAction* darkTheme_{nullptr};
};

}  // namespace timetracker::ui
