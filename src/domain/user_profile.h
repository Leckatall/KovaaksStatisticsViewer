//
// Created by Lecka on 01/08/2026.
//

#ifndef KOVAAKSSTATSVIEWER_USER_PROFILE_H
#define KOVAAKSSTATSVIEWER_USER_PROFILE_H
#include <map>
#include <string>
#include <vector>

#include "scenario_perf.h"

namespace ksv::domain {
    class UserProfile {
    public:
        explicit UserProfile(std::string name);

        void addScenarioPerf(const ScenarioPerf &perf);

        [[nodiscard]] std::vector<ScenarioId> getScenarioList() const;

    private:
        std::string m_name;
        // std::string m_kovaaks_dir;
        std::map<ScenarioId, std::vector<ScenarioPerf> > m_runs;
    };
}

#endif //KOVAAKSSTATSVIEWER_USER_PROFILE_H
