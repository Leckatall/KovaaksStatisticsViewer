---
type: fixed
area: Graphing
user: The graph now shows your most recent run as soon as the app opens, instead of staying blank until you interact with something.
---
`SessionController` loads the latest perf in its own constructor
(`SessionController::SessionController`, [session_controller.cpp](src/app/session_controller.cpp)),
which runs before `App::App()` wires `IGraphUseCase::onCurrentPerfChanged` to
`GraphViewModel::fetchData()` ([app.cpp](src/app/app.cpp)). That first `currentPerfChanged`
emission was never observed by the graph VM, so the dashboard opened empty. `App::App()` now
calls `m_graphVm->fetchData()` once, immediately after the connection is made, to pick up
whatever `SessionController` already loaded.
