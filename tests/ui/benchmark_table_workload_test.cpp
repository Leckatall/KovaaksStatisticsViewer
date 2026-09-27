#include <gtest/gtest.h>

#include <QElapsedTimer>
#include <QLocale>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#include "presentation/benchmark_cell_input.h"
#include "presentation/benchmark_manager_vm.h"
#include "presentation/benchmark_table_model.h"
#include "fake_benchmark_manager_use_case.h"

using namespace ksv::application;
using namespace ksv::data;
using namespace ksv::domain;
using namespace ksv::presentation;
using namespace ksv::tests_support;

// Measurement, not a performance gate: timings and memory are recorded as GoogleTest XML
// properties (kept by the build wrapper under .temp/build-and-test) and no latency or memory
// threshold is asserted. Correctness of every pasted coordinate and of full Undo is asserted.

namespace {
    // The "Easier Scenarios" sheet of the supplied Viscose workbook: column C (scenario names) and
    // columns H-O (eight rank names and their thresholds), converted once to spreadsheet TSV.
    const QString kWorkbookNames = QStringLiteral(R"TSV(WhisphereRawControl Larger + Slowed
Whisphere 80%
Smoothbot Invincible Goated 75%
Leaptrack Goated 60% Larger
Controlsphere rAim Easy 90%
VT Controlsphere Intermediate S5 80%
Air Angelic 4 Voltaic Easy 80% (Good Version)
Cloverrawcontrol Easy 80% Speed
"Controlsphere Far, Far Larger 90%"
PGTI Voltaic Easy 80%
Air CELESTIAL No UFO Easy Slowed
Whisphere Small & Slow 55%
Air Voltaic Invincible 7 Easy 80%
Controlsphere OW Long Strafes 90%
Flicker Plaza rAim Easy Less Blinks
Polarized Hell Easy 40% Slower
Air Pure Intermediate Slower No UFO
Air Voltaic Easy Invincible 4 80%
Pokeball Frenzy Auto TE Wide
1w3ts Reload Larger
voxTargetSwitch 2 Large
BeanTS Larger
FloatTS Angelic Easy Larger
WaldoTS Novice
devTS Goated NR Static 5Bot
domiSwitch Easy Slower
tamTargetSwitch Smooth Easy
1wall5targets_pasu slow
B180 Voltaic Easy 92%
Controlsphere Click Easy
Popcorn MV Novice
Pasu Angelic 20% Larger 80% Speed
1w2ts Pasu Perfected Easy
1w3ts Pasu Perfected Micro Goated Larger 80%
Floating Heads Timing 400% Larger
"voxTargetSwitch Click "
)TSV");
    const QString kWorkbookRanks = QStringLiteral(R"TSV(Lemming	Hare	Ermine	Penguin	Fox	Mammoth	Orca	Seal
)TSV");
    const QString kWorkbookThresholds = QStringLiteral(R"TSV(5500	6700	7800	8700	9600	10500	11400	12500
6300	7700	9000	10000	11000	12000	13000	14500
1800	2250	2650	2900	3150	3400	3650	4000
850	1200	1500	1700	1900	2100	2250	2450
6100	6950	7700	8400	9100	9800	10500	11500
1850	2300	2700	3000	3300	3600	3850	4100
1050	1600	2000	2400	2700	3000	3300	3600
3900	4550	5200	5700	6200	6700	7200	7700
7600	8150	8700	9200	9800	10200	10900	11500
350	550	850	1100	1350	1600	1900	2250
820	835	850	861	870	878	884	890
6000	7500	9000	10000	10750	11500	12250	13500
750	1200	1600	1900	2200	2500	2800	3200
5400	6100	6700	7200	7600	8000	8300	8700
858	871	883	890	895	900	904	909
750	1100	1400	1600	1800	2000	2150	2500
860	874	886	893	901	907	911	916
1100	1600	2100	2450	2800	3150	3400	3800
650	950	1250	1500	1750	2000	2300	2700
36	43	50	58	70	82	92	102
67	78	87	95	103	110	117	123
65	78	90	100	110	120	130	142
65	74	81	88	95	101	106	111
65	78	90	100	110	120	130	140
350	400	450	500	550	600	640	680
3200	3700	4200	4600	5000	5400	5800	6300
7	11	15	18	21	24	26	28
76	88	100	110	120	130	140	150
26	38	50	58	65	72	78	87
15	21	27	33	39	45	50	55
50	100	150	190	230	270	300	330
51	58	65	72	78	84	90	97
58	69	80	87	93	99	105	110
600	700	800	900	1000	1100	1200	1300
400	700	1000	1350	1700	2050	2400	2750
49	59	67	74	81	88	94	100
)TSV");

    constexpr int kTiers = 8;
    constexpr int kEdits = 20;

    class DefaultLocaleGuard {
    public:
        explicit DefaultLocaleGuard(const QLocale &locale) { QLocale::setDefault(locale); }
        ~DefaultLocaleGuard() { QLocale::setDefault(m_previous); }
        DefaultLocaleGuard(const DefaultLocaleGuard &) = delete;
        DefaultLocaleGuard &operator=(const DefaultLocaleGuard &) = delete;

    private:
        QLocale m_previous;
    };

    qint64 privateBytes() {
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS_EX counters{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
                                 sizeof(counters)))
            return static_cast<qint64>(counters.PrivateUsage);
#endif
        return -1;
    }

    QStringList lines(const QString &tsv) {
        QStringList result = tsv.split(u'\n');
        if (!result.isEmpty() && result.back().isEmpty()) result.pop_back();
        return result;
    }

    // `rows` workbook rows, cycling through the 36 real ones. Every 17th cell becomes non-numeric
    // text and every 23rd a blank, so retained input and clearing ride along with real values.
    struct Workload {
        QString names;
        QString thresholds;
        std::vector<std::vector<QString>> cells;
    };

    Workload makeWorkload(int rows) {
        const auto names = lines(kWorkbookNames);
        const auto values = lines(kWorkbookThresholds);
        Workload workload;
        for (int r = 0; r < rows; ++r) {
            const int source = r % static_cast<int>(names.size());
            QString name = names.at(source);
            if (r >= names.size()) name = QStringLiteral("%1 #%2").arg(name).arg(r / names.size());
            workload.names += name + u'\n';
            auto fields = values.at(source).split(u'\t');
            for (int t = 0; t < kTiers; ++t) {
                const int cell = r * kTiers + t;
                if (cell % 17 == 16) fields[t] = QStringLiteral("oops%1").arg(cell);
                else if (cell % 23 == 22) fields[t] = QString();
            }
            workload.cells.emplace_back(fields.begin(), fields.end());
            workload.thresholds += fields.join(u'\t') + u'\n';
        }
        return workload;
    }

    struct Session {
        std::shared_ptr<FakeBenchmarkManagerUseCase> uc = std::make_shared<FakeBenchmarkManagerUseCase>();
        std::unique_ptr<BenchmarkManagerViewModel> vm;

        Session() {
            Benchmark empty;
            empty.id = BenchmarkId{"workload"};
            empty.name = "Workload";
            uc->nextNewSeed = {empty, std::nullopt};
            vm = std::make_unique<BenchmarkManagerViewModel>(uc);
            vm->beginNewBenchmark();
        }

        [[nodiscard]] QAbstractItemModel *model() const { return vm->tableModel(); }
        [[nodiscard]] QVariant cell(int row, int column, int role) const {
            return model()->data(model()->index(row, column), role);
        }
        [[nodiscard]] QStringList rowIds() const {
            QStringList ids;
            for (int row = 0; row < model()->rowCount(); ++row)
                ids.push_back(cell(row, 2, BenchmarkTableModel::EntryIdRole).toString());
            return ids;
        }
    };

    // Runs one command and checks it published exactly once, with one resolution pass.
    qint64 timedCommand(Session &session, const std::function<QVariantMap()> &command, const char *label) {
        QSignalSpy published(session.vm.get(), &BenchmarkManagerViewModel::draftChanged);
        const auto resolves = session.uc->resolveCalls.size();
        QElapsedTimer timer;
        timer.start();
        const auto result = command();
        const qint64 elapsed = timer.nsecsElapsed() / 1000;
        EXPECT_TRUE(result.value("ok").toBool()) << label << ": " << result.value("error").toString().toStdString();
        EXPECT_EQ(published.count(), 1) << label;
        EXPECT_EQ(session.uc->resolveCalls.size(), resolves + 1) << label;
        return elapsed;
    }

    void expectContent(const Session &session, const Workload &workload, const QLocale &locale) {
        const int rows = static_cast<int>(workload.cells.size());
        ASSERT_EQ(session.model()->rowCount(), rows);
        ASSERT_EQ(session.model()->columnCount(), BenchmarkTableModel::FixedColumnCount + kTiers);
        int mismatches = 0;
        for (int r = 0; r < rows; ++r) {
            for (int t = 0; t < kTiers; ++t) {
                const auto &expected = workload.cells[static_cast<std::size_t>(r)][static_cast<std::size_t>(t)];
                const int column = BenchmarkTableModel::FixedColumnCount + t;
                const auto display = session.cell(r, column, BenchmarkTableModel::DisplayTextRole).toString();
                const bool hasValue = session.cell(r, column, BenchmarkTableModel::HasValueRole).toBool();
                const auto state = session.cell(r, column, BenchmarkTableModel::InputStateRole).toString();
                bool ok = false;
                if (expected.isEmpty()) ok = !hasValue && state.isEmpty();
                else if (expected.startsWith(u"oops")) ok = !hasValue && state == u"invalid" && display == expected;
                else ok = hasValue && BenchmarkCellInput::parse(display, locale).value == expected.toDouble();
                if (!ok && ++mismatches <= 5)
                    ADD_FAILURE() << "row " << r << " tier " << t << ": expected " << expected.toStdString()
                                  << ", shown " << display.toStdString();
            }
        }
        EXPECT_EQ(mismatches, 0);
    }
}

TEST(BenchmarkTableWorkload, RepresentativeTransferAndHistory) {
    const QLocale locale{QLocale::English, QLocale::UnitedKingdom};
    const DefaultLocaleGuard guard{locale};

    for (const int rows: {36, 200, 2000}) {
        SCOPED_TRACE(rows);
        const auto workload = makeWorkload(rows);
        const std::string prefix = "rows_" + std::to_string(rows) + "_";
        Session session;
        const qint64 memoryStart = privateBytes();

        const qint64 headerUs = timedCommand(session, [&] {
            return session.vm->pasteText(QVariantMap{{"kind", "rankHeader"}}, kWorkbookRanks);
        }, "rank names");
        const qint64 namesUs = timedCommand(session, [&] {
            return session.vm->pasteText(QVariantMap{{"kind", "append"}}, workload.names);
        }, "scenario names");
        const QStringList ids = session.rowIds();
        ASSERT_EQ(ids.size(), rows);
        const QString firstTier = session.model()->headerData(BenchmarkTableModel::FixedColumnCount, Qt::Horizontal,
                                                              BenchmarkTableModel::TierIdRole).toString();
        const qint64 thresholdsUs = timedCommand(session, [&] {
            return session.vm->pasteText(QVariantMap{{"kind", "threshold"}, {"entryId", ids.front()},
                                                     {"tierId", firstTier}}, workload.thresholds);
        }, "threshold block");
        expectContent(session, workload, locale);
        EXPECT_EQ(session.cell(0, 2, BenchmarkTableModel::DisplayTextRole).toString(),
                  lines(kWorkbookNames).front());
        EXPECT_EQ(session.model()->headerData(BenchmarkTableModel::FixedColumnCount + kTiers - 1, Qt::Horizontal,
                                              Qt::DisplayRole).toString(), QStringLiteral("Seal"));

        qint64 editsUs = 0;
        for (int i = 0; i < kEdits; ++i) {
            const int row = (i * 7) % rows;
            const QString tier = session.model()->headerData(BenchmarkTableModel::FixedColumnCount + i % kTiers,
                                                             Qt::Horizontal, BenchmarkTableModel::TierIdRole).toString();
            editsUs += timedCommand(session, [&] {
                return session.vm->editThresholdText(ids.at(row), tier, QString::number(100000 + i));
            }, "manual edit");
        }
        const qint64 memoryWithHistory = privateBytes();
        EXPECT_EQ(session.rowIds(), ids) << "edits never reallocate row identity";

        QElapsedTimer undoTimer;
        undoTimer.start();
        int undone = 0;
        while (session.vm->canUndo()) {
            ASSERT_TRUE(session.vm->undo()["ok"].toBool());
            ++undone;
            if (undone == kEdits) {
                EXPECT_EQ(session.rowIds(), ids) << "undoing the edits restores the same IDs";
                expectContent(session, workload, locale);
            }
        }
        const qint64 undoUs = undoTimer.nsecsElapsed() / 1000;
        EXPECT_EQ(undone, kEdits + 3) << "each paste and each edit is one history step";
        EXPECT_EQ(session.model()->rowCount(), 0);
        EXPECT_EQ(session.model()->columnCount(), BenchmarkTableModel::FixedColumnCount);

        // A successful save is a history boundary; memory is observed again after it.
        session.uc->nextSaveOutcome = {BenchmarkEditToken{BenchmarkId{"workload"}, "workload.json", "d1"},
                                       std::nullopt};
        ASSERT_TRUE(session.vm->save()["ok"].toBool());
        const qint64 memoryAfterReset = privateBytes();

        testing::Test::RecordProperty(prefix + "tiers", kTiers);
        testing::Test::RecordProperty(prefix + "history_depth", kEdits + 3);
        testing::Test::RecordProperty(prefix + "paste_rank_names_us", std::to_string(headerUs));
        testing::Test::RecordProperty(prefix + "paste_scenario_names_us", std::to_string(namesUs));
        testing::Test::RecordProperty(prefix + "paste_thresholds_us", std::to_string(thresholdsUs));
        testing::Test::RecordProperty(prefix + "manual_edits_total_us", std::to_string(editsUs));
        testing::Test::RecordProperty(prefix + "undo_all_us", std::to_string(undoUs));
        testing::Test::RecordProperty(prefix + "private_bytes_start", std::to_string(memoryStart));
        testing::Test::RecordProperty(prefix + "private_bytes_with_history", std::to_string(memoryWithHistory));
        testing::Test::RecordProperty(prefix + "private_bytes_after_reset", std::to_string(memoryAfterReset));
    }
}
