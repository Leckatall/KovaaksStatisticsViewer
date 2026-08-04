# Project Goals & Feature Notes

Living notes for planned features and ideas. Organized by theme for easier reference — not a commitment/priority order unless noted.

## Data Ingestion

- [ ] Read total non-paused playtime from Kovaaks and render how playtime fluctuates over time.
  - Open question: bar graph of time played per period (x-axis = time buckets) vs. a continuous line graph of rolling time-played-in-last-x-period. Need to decide which represents the data better.
- [ ] Implement reading the CSV outputs from Kovaaks (in addition to whatever source is currently used).
- [ ] Implement a richer profile persistence and control system.

## Scenario & Run Selection

- [ ] Select a scenario, then a specific run, through the GUI — without having to manually pick the specific file.

## Performance Analysis & Comparison

- [ ] Compare how scores fluctuate on average during a scenario vs. a specific run's performance.
- [ ] Show aggregate data alongside most recent performance, to see "this run vs. average."
- [ ] Track highscore data for each scenario.
- [ ] Pull top-percentile performances for a scenario and aggregate them as a comparison baseline.
- [ ] Group scenarios together and show how the averaged score across the group has changed over time.
- [ ] Stretch goal: fetch leaderboard percentile data for each scenario.

## In-Run / Live Graphing

- [ ] Add a line showing the expected final score, projected from performance in the last few seconds.
- [ ] Add a graph showing how much time worth of challenge completions has happened recently.

## UI / UX

- [ ] Settings dialog to customize how data is displayed and where it's sourced from.
- [ ] General UI polish — make it look good, especially the graphs.
- [ ] Smoother curve interpolation on graphs (replace the current rigid straight-line joins).
