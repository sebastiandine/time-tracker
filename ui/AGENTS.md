# ui/AGENTS.md

`time_tracker_ui` — Qt Widgets. Read the root `AGENTS.md` first.

## File pointers

- `include/timetracker/ui/MainWindow.hpp` — tabs, theme menu, open-timer restore.
- `include/timetracker/ui/LibraryPage.hpp` — category, main topic, and subtopic editing.
- `include/timetracker/ui/TrackPage.hpp` — start, stop, note, and recent entries.
- `include/timetracker/ui/StatisticsPage.hpp` — range, category filter, charts, and totals.
- `src/ShareChart.hpp` — pie and bar charts for the statistics page.
- `src/Theme.cpp` — light and dark Fusion palettes.
- `src/CatalogTree.cpp` — shared topic tree.

## Conventions

1. Qt headers are allowed here. Do not include `sqlite3.h`.
2. Do not link `time_tracker_warnings`.
3. Business rules stay in `core/`. This target calls services.
4. Headers with `Q_OBJECT` are listed in `CMakeLists.txt` so AUTOMOC runs.
5. The one-second timer updates labels only. It does not write a tick to SQLite.
6. Closing the window does not stop a running entry.
7. Statistics charts use slices that partition the filtered total. See `docs/time-tracking.md`.

## Commands

`cmake --build build --target time_tracker_ui`
