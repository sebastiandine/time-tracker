#include "ProcessEnvironment.hpp"
#include "ProcessFilesystem.hpp"
#include "SystemClock.hpp"

#include "timetracker/config/ConfigService.hpp"
#include "timetracker/infra/SqliteCatalogRepository.hpp"
#include "timetracker/infra/SqliteDatabase.hpp"
#include "timetracker/infra/SqliteSettingsRepository.hpp"
#include "timetracker/infra/SqliteTimeEntryRepository.hpp"
#include "timetracker/paths/DataPaths.hpp"
#include "timetracker/services/CatalogService.hpp"
#include "timetracker/services/DataLocationService.hpp"
#include "timetracker/services/SettingsService.hpp"
#include "timetracker/services/StatsService.hpp"
#include "timetracker/services/TrackingService.hpp"
#include "timetracker/ui/MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setStyle("Fusion");
    application.setApplicationName(QStringLiteral("Time Tracker"));

    auto environment = std::make_unique<timetracker::ProcessEnvironment>();
    auto files = std::make_unique<timetracker::ProcessFilesystem>();
    auto clock = std::make_unique<timetracker::SystemClock>();

    const auto executable = std::filesystem::path{QCoreApplication::applicationFilePath().toStdWString()};
    auto locations = timetracker::configLocations(*environment, executable.parent_path());
    if (!locations) {
        QMessageBox::critical(nullptr, QStringLiteral("Time Tracker"),
                              QString::fromStdString(locations.error()));
        return 1;
    }
    std::vector<std::string> arguments;
    const auto qtArguments = QCoreApplication::arguments();
    for (int index = 1; index < qtArguments.size(); ++index) {
        arguments.push_back(qtArguments.at(index).toStdString());
    }
    timetracker::ConfigService config;
    auto loaded = config.load(*environment, *files, locations.value(), arguments, std::cerr);
    if (!loaded) {
        QMessageBox::critical(nullptr, QStringLiteral("Time Tracker"),
                              QString::fromStdString(loaded.error()));
        return 1;
    }

    auto database = std::make_unique<timetracker::SqliteDatabase>();
    auto opened = timetracker::openDatabaseAt(config.dataDirectory(), *files, *database);
    if (!opened) {
        QMessageBox::critical(nullptr, QStringLiteral("Time Tracker"),
                              QString::fromStdString(opened.error()));
        return 1;
    }

    auto catalogRepo = std::make_unique<timetracker::SqliteCatalogRepository>(*database);
    auto timeRepo = std::make_unique<timetracker::SqliteTimeEntryRepository>(*database);
    auto settingsRepo = std::make_unique<timetracker::SqliteSettingsRepository>(*database);
    auto catalog = std::make_unique<timetracker::CatalogService>(*catalogRepo);
    auto tracking =
        std::make_unique<timetracker::TrackingService>(*catalogRepo, *timeRepo, *clock);
    auto stats = std::make_unique<timetracker::StatsService>(*timeRepo, *clock);
    auto settings = std::make_unique<timetracker::SettingsService>(*settingsRepo);
    auto dataLocation = std::make_unique<timetracker::DataLocationService>(config, *files, *database);

    timetracker::ui::MainWindow window(*catalog, *tracking, *stats, *settings, *dataLocation,
                                       QStringLiteral(TIME_TRACKER_APP_VERSION));
    window.show();
    return application.exec();
}
