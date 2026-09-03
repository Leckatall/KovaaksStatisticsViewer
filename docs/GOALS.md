# Project Goals & Feature Roadmap

Checkable goals grouped by implementation area. Sub-goals break each feature into concrete steps.

**Difficulty:** `[Easy]` `[Medium]` `[Hard]` `[Very Hard]`
**Value:** `[Low]` `[Medium]` `[High]` `[Critical]`

---

## 1. Scenario & Run Selection

> *Core navigation — users need to browse and pick what they want to look at.*

- [x] **Select a scenario from a list in the GUI** `[Easy]` `[Critical]`
  - [x] Expose scenario list from `SessionController` via `SessionViewModel`
  - [x] Add a searchable/filterable scenario list panel in QML
  - [x] Wire scenario selection to update `SessionController`'s current state
  - [x] Show scenario metadata (run count, last played date) in the list items

- [x] **Select a specific run within a scenario** `[Medium]` `[High]`
  - [x] Expose per-scenario run list from `SessionController`
  - [x] Add a run list/dropdown in QML that populates when a scenario is selected
  - [x] Display run summary info (date, score, accuracy) in each run entry
  - [x] Wire run selection to update the graph and detail views

---

## 2. Data Ingestion & Persistence

> *Expanding what data the app can read and how it stores processed results.*

- [x] **Implement CSV input support** `[Medium]` `[Medium]`
  - [x] Define a CSV parser that produces `domain::ScenarioPerf` from Kovaaks CSV exports
  - [x] Add tests for CSV parsing edge cases (missing fields, encoding quirks)

- [ ] **Richer profile persistence and control** `[Hard]` `[High]`
  - [ ] Support multiple named profiles (e.g. different Kovaaks accounts or data sets)
  - [ ] Add UI for profile switching in settings
  - [ ] Allow import/export of profile data for backup or sharing
  - [x] Handle profile migration when the cache schema changes between versions
  - [ ] Add support for additional profile data sources
    - [x] csv files
    - [ ] Scrapping Kovaaks runtime mem
  - [ ] Allow profiles to store groupings of scenarios

---

## 3. Performance Analysis — Single Scenario

> *Understanding performance within one scenario over time.*

- [ ] **Track and display highscore data per scenario** `[Easy]` `[High]`
  - [ ] Compute and store the highscore for each scenario in `UserProfile`
  - [ ] Display the highscore prominently when a scenario is selected
  - [x] Show highscore progression over time (when each new PB was set)

- [ ] **Compare a specific run against scenario averages** `[Medium]` `[High]`
  - [ ] Compute rolling/overall average score curve for a scenario
  - [ ] Overlay the average curve on the graph alongside the selected run
  - [ ] Show delta stats (how far above/below average this run was)

- [x] **Show aggregate data alongside most recent performance** `[Medium]` `[High]`
  - [x] Compute aggregate stats (mean, median, std dev) for a scenario
  - [ ] Add a stats summary panel next to the graph
  - [ ] Highlight the most recent run's stats relative to the aggregate

- [x] **Pull top-percentile performances as a comparison baseline** `[Medium]` `[Medium]`
  - [x] Identify top N% of runs for a scenario from local data
  - [x] Compute an averaged "best performance" curve from those runs
  - [x] Allow the user to configure the percentile threshold (e.g. top 10%, top 5%)
  - [x] Overlay the baseline on the graph as a reference line

- [ ] **Fetch leaderboard percentile data per scenario** *(stretch)* `[Very Hard]` `[Medium]`
  - [ ] Reverse-engineer or find an API for Kovaaks leaderboard data
  - [ ] Fetch and cache leaderboard distributions per scenario
  - [ ] Show the user's score as a percentile rank on the leaderboard
  - [ ] Handle API rate limiting, errors, and offline fallback gracefully

---

## 4. Performance Analysis — Cross-Scenario

> *Comparing and grouping performance across multiple scenarios.*

- [ ] **Group scenarios and track averaged group score over time** `[Hard]` `[High]`
  - [ ] Define a scenario group data model (name + list of `ScenarioId`s)
  - [ ] Add UI for creating, editing, and deleting scenario groups
  - [ ] Compute a normalised average score across the group for each time period
  - [ ] Display a time-series graph of the group's averaged score
  - [ ] Persist groups in user settings or profile data

---

## 5. Playtime Tracking

> *Understanding how much time is spent practising and how that changes.*

- [x] **Read and display playtime data** `[Medium]` `[Medium]`
  - [x] Extract time total of scenario completions from `.perf` files
  - [x] Aggregate playtime per day/week/month
  - [x] Display playtime as a bar chart (time per period) or line chart (rolling window)
  - [x] Decide on visualisation approach: bar chart of time-per-period vs. rolling-window line graph (or offer both)
  - [ ] Extract non-paused playtime from Kovaaks application memory

---

## 6. In-Run / Live Analysis

> *This section was a misunderstanding there has never been any plans for live analysis.*
> *Some sections are checked off because things like "Projected final score line" was a reference to line showed after the run was completed.*

- [x] **Projected final score line** `[Medium]` `[Medium]`
  - [x] Implement a rolling projection algorithm from recent-seconds performance
  - [x] Render a dashed/styled projection line extending from the current point to scenario end
  - [x] Update projection dynamically as the user scrubs through the run timeline
  - Note: Implemented as `ExpectedFinalScore` (average pace) and `ExpectedFinalScoreRecent` (trailing 5-sec pace)

- [ ] **Recent challenge completion rate graph** `[Medium]` `[Low]`
  - [ ] Compute a windowed rate of challenge completions from kill/hit events
  - [ ] Add a secondary graph or overlay showing this rate over the run duration
  - [ ] Allow the user to configure the lookback window size

---

## 7. UI / UX Polish

> *Making the app look and feel good.*

- [x] **Settings dialog for data display and source configuration**
- [x] **Smooth curve interpolation on graphs** *(AxisModel with Heckbert's nice-number algorithm implemented)*

- [ ] **General UI polish** `[Medium]` `[High]`
  - [x] Design a consistent color scheme and typography
  - [ ] Add a dark/light theme toggle
  - [x] Polish graph styling (grid lines, axis labels, tick formatting, legend)
  - [x] Add loading indicators for profile generation and data processing
  - [x] Improve layout responsiveness for different window sizes

- [ ] **Graph interaction improvements** `[Medium]` `[Medium]`
  - [ ] Add zoom and pan to graphs
  - [x] Improve hover tooltip with more contextual data
  - [x] Add ability to toggle individual series on/off
  - [ ] Support exporting graphs as images

---

## 8. Custom Series

> *Making user-defined series easier to create, reuse, and present.*

- [ ] **Provide useful default expressions for user series** `[Easy]` `[High]`
  - [ ] Define a set of sensible default expressions for new user-created series
  - [ ] Select an appropriate default when the user adds a series

- [ ] **Customise series line rendering** `[Medium]` `[Medium]`
  - [ ] Expose rendering options for a series line, such as colour, width, and style
  - [ ] Persist the selected rendering options with the series configuration
  - [ ] Apply the options consistently in graph rendering and the legend

- [ ] **Duplicate an existing series** `[Easy]` `[Medium]`
  - [ ] Add a copy action to each user series
  - [ ] Create an editable duplicate that preserves the source series configuration

- [ ] **Add a text source editor for series expressions** `[Medium]` `[High]`
  - [ ] Display a text representation beneath `ExpressionTreeEditor`
  - [ ] Make the text representation the source of truth for the expression tree
  - [ ] Support copying and pasting expressions between series
