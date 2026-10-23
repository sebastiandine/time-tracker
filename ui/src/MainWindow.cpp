#include "timetracker/ui/MainWindow.hpp"

#include "CatalogTree.hpp"
#include "Theme.hpp"
#include "UiUtil.hpp"
#include "timetracker/time/TimeFormat.hpp"
#include "timetracker/ui/LibraryPage.hpp"
#include "timetracker/ui/StatisticsPage.hpp"
#include "timetracker/ui/TrackPage.hpp"

#include <QAction>
#include <QActionGroup>
#include <QGuiApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyleHints>
#include <QTabWidget>
#include <QSignalBlocker>
#include <QTimer>

#include <string>
#include <utility>

namespace timetracker::ui {

MainWindow::MainWindow(CatalogService& catalog, TrackingService& tracking, StatsService& stats,
                       SettingsService& settings, QString version, QWidget* parent)
    : QMainWindow(parent),
      catalog_(catalog),
      tracking_(tracking),
      stats_(stats),
      settings_(settings),
      version_(std::move(version)) {
    buildUi();
    buildThemeMenu();
    auto stored = settings_.theme();
    if (!stored) {
        showError(this, stored.error());
        theme_ = ThemePreference::System;
    } else {
        theme_ = stored.value();
    }
    applyTheme(theme_);
    syncThemeChecks();
    reloadCatalog();
    restoreSession(true);

    // The tick only refreshes labels. The open row's start time is already stored,
    // so closing this window does not stop the timer.
    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(1000);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::onTick);
    clockTimer_->start();

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
                if (theme_ == ThemePreference::System) {
                    applyTheme(theme_);
                }
            });
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Time Tracker %1").arg(version_));
    resize(980, 680);
    tabs_ = new QTabWidget(this);
    library_ = new LibraryPage(catalog_, tabs_);
    track_ = new TrackPage(tracking_, tabs_);
    statistics_ = new StatisticsPage(stats_, tabs_);
    tabs_->addTab(track_, QStringLiteral("Track"));
    tabs_->addTab(statistics_, QStringLiteral("Statistics"));
    tabs_->addTab(library_, QStringLiteral("Library"));
    setCentralWidget(tabs_);
    connect(library_, &LibraryPage::catalogChanged, this, &MainWindow::reloadCatalog);
    connect(track_, &TrackPage::sessionChanged, this, [this]() {
        restoreSession(false);
        updateStatusBar();
        if (tabs_->currentWidget() == statistics_) {
            statistics_->refresh();
        }
    });
}

void MainWindow::buildThemeMenu() {
    auto* view = menuBar()->addMenu(QStringLiteral("View"));
    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    systemTheme_ = view->addAction(QStringLiteral("System theme"));
    lightTheme_ = view->addAction(QStringLiteral("Light"));
    darkTheme_ = view->addAction(QStringLiteral("Dark"));
    for (auto* action : {systemTheme_, lightTheme_, darkTheme_}) {
        action->setCheckable(true);
        group->addAction(action);
    }
    connect(systemTheme_, &QAction::triggered, this,
            [this]() { chooseTheme(ThemePreference::System); });
    connect(lightTheme_, &QAction::triggered, this,
            [this]() { chooseTheme(ThemePreference::Light); });
    connect(darkTheme_, &QAction::triggered, this, [this]() { chooseTheme(ThemePreference::Dark); });
}

void MainWindow::reloadCatalog() {
    auto loaded = catalog_.loadAll();
    if (!loaded) {
        showError(this, loaded.error());
        return;
    }
    snapshot_ = std::move(loaded.value());
    library_->setSnapshot(snapshot_);
    track_->setSnapshot(snapshot_);
    statistics_->setSnapshot(snapshot_);
    if (tabs_->currentWidget() == statistics_) {
        statistics_->refresh();
    }
    updateStatusBar();
}

void MainWindow::restoreSession(bool switchToTrack) {
    auto running = tracking_.current();
    if (!running) {
        showError(this, running.error());
        return;
    }
    track_->showSession(running.value());
    if (switchToTrack && running.value().has_value()) {
        tabs_->setCurrentWidget(track_);
    }
}

void MainWindow::updateStatusBar() {
    auto running = tracking_.current();
    if (!running || !running.value().has_value()) {
        statusBar()->showMessage(QStringLiteral("Not recording"));
        return;
    }
    auto elapsed = tracking_.elapsedSeconds();
    const auto text = elapsed ? formatDuration(elapsed.value()) : std::string("0:00:00");
    statusBar()->showMessage(
        QStringLiteral("Recording %1 — %2. Time keeps counting if this app closes.")
            .arg(topicPath(snapshot_, *running.value()), QString::fromStdString(text)));
}

void MainWindow::chooseTheme(ThemePreference theme) {
    auto saved = settings_.setTheme(theme);
    if (!saved) {
        showError(this, saved.error());
        syncThemeChecks();
        return;
    }
    theme_ = theme;
    applyTheme(theme_);
    syncThemeChecks();
}

void MainWindow::syncThemeChecks() {
    const QSignalBlocker blockSystem(systemTheme_);
    const QSignalBlocker blockLight(lightTheme_);
    const QSignalBlocker blockDark(darkTheme_);
    systemTheme_->setChecked(theme_ == ThemePreference::System);
    lightTheme_->setChecked(theme_ == ThemePreference::Light);
    darkTheme_->setChecked(theme_ == ThemePreference::Dark);
}

void MainWindow::onTick() {
    track_->updateClock();
    updateStatusBar();
    if (tabs_->currentWidget() != statistics_) {
        return;
    }
    auto running = tracking_.current();
    if (running && running.value().has_value()) {
        statistics_->refresh();
    }
}

}  // namespace timetracker::ui
