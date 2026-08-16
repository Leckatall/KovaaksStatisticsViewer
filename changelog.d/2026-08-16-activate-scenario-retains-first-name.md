---
type: fixed
area: Scenario & run selection
---
`ScenarioBrowserViewModel::activateScenario()` no longer overwrites `m_active_scenario_name` when reactivating an already-active hash with a different name. Previously this VM-level cache could drift from the domain's already-enforced first-name-wins-for-a-shared-hash identity policy.
