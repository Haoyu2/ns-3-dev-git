#include "ns3/object-factory.h"
#include "ns3/test.h"
#include "ns3/twt-schedule.h"
#include "ns3/twt-scheduler.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ns3;

/**
 * @defgroup aoi-twt-tests Tests for aoi-twt
 * @ingroup aoi-twt
 * @ingroup tests
 */

/**
 * @ingroup aoi-twt-tests
 * Check that two schedule entries never overlap on the air, by expanding
 * both over their common hyperperiod.
 *
 * @param a First schedule entry.
 * @param b Second schedule entry.
 * @return true if no service periods of a and b overlap.
 */
static bool
EntriesDisjoint(const TwtScheduleEntry& a, const TwtScheduleEntry& b)
{
    double ta = a.wakeInterval.GetSeconds();
    double tb = b.wakeInterval.GetSeconds();
    double hyper = std::max(ta, tb);
    // Harmonic periods: the longer period is the hyperperiod.
    auto repsA = static_cast<uint32_t>(std::round(hyper / ta));
    auto repsB = static_cast<uint32_t>(std::round(hyper / tb));
    for (uint32_t i = 0; i < repsA; ++i)
    {
        double sa = a.offset.GetSeconds() + i * ta;
        double ea = sa + a.wakeDuration.GetSeconds();
        for (uint32_t j = 0; j < repsB; ++j)
        {
            double sb = b.offset.GetSeconds() + j * tb;
            double eb = sb + b.wakeDuration.GetSeconds();
            if (sa < eb - 1e-12 && sb < ea - 1e-12)
            {
                return false;
            }
        }
    }
    return true;
}

/**
 * @ingroup aoi-twt-tests
 * Validate the closed-form AoI expressions of TwtAoiModel.
 */
class TwtAoiModelTestCase : public TestCase
{
  public:
    TwtAoiModelTestCase()
        : TestCase("TwtAoiModel closed-form expressions")
    {
    }

  private:
    void DoRun() override
    {
        // SP success probability: 1 - (1-p)^k
        NS_TEST_ASSERT_MSG_EQ_TOL(TwtAoiModel::SuccessProbPerSp(0.5, 2),
                                  0.75,
                                  1e-12,
                                  "Wrong SP success probability");
        NS_TEST_ASSERT_MSG_EQ_TOL(TwtAoiModel::SuccessProbPerSp(1.0, 1),
                                  1.0,
                                  1e-12,
                                  "Perfect channel must give pSucc = 1");

        // Reliable channel: classic periodic-sampling age delta + T/2.
        Time meanAoi = TwtAoiModel::MeanAoi(Seconds(1.0), 1.0, Seconds(0.0));
        NS_TEST_ASSERT_MSG_EQ_TOL(meanAoi.GetSeconds(),
                                  0.5,
                                  1e-9,
                                  "Mean AoI for reliable channel must be T/2");

        // Lossy channel: delta + T (1/pSucc - 1/2).
        meanAoi = TwtAoiModel::MeanAoi(Seconds(2.0), 0.5, Seconds(0.1));
        NS_TEST_ASSERT_MSG_EQ_TOL(meanAoi.GetSeconds(),
                                  0.1 + 2.0 * 1.5,
                                  1e-9,
                                  "Mean AoI for lossy channel is wrong");

        Time peakAoi = TwtAoiModel::PeakAoi(Seconds(2.0), 0.5, Seconds(0.1));
        NS_TEST_ASSERT_MSG_EQ_TOL(peakAoi.GetSeconds(),
                                  0.1 + 4.0,
                                  1e-9,
                                  "Peak AoI is wrong");

        // Zero per-wake overhead: k* = 1 for every p, since
        // f(2) - f(1) = p / (2 (2 - p)) > 0.
        NS_TEST_ASSERT_MSG_EQ(TwtAoiModel::OptimalAttemptsPerSp(1.0, 0.0, 8),
                              1,
                              "k* must be 1 without wake overhead (p = 1)");
        NS_TEST_ASSERT_MSG_EQ(TwtAoiModel::OptimalAttemptsPerSp(0.2, 0.0, 8),
                              1,
                              "k* must be 1 without wake overhead (p = 0.2)");
        // Substantial per-wake overhead amortizes across batched retries.
        NS_TEST_ASSERT_MSG_GT(TwtAoiModel::OptimalAttemptsPerSp(0.3, 5.0, 8),
                              1,
                              "k* must exceed 1 with large wake overhead");
    }
};

/**
 * @ingroup aoi-twt-tests
 * Validate that schedulers produce non-overlapping, energy-feasible
 * schedules for a heterogeneous station population.
 */
class TwtSchedulerFeasibilityTestCase : public TestCase
{
  public:
    /**
     * Constructor.
     * @param schedulerTypeId TypeId name of the scheduler under test.
     * @param name Test case name.
     */
    TwtSchedulerFeasibilityTestCase(std::string schedulerTypeId, std::string name)
        : TestCase(name),
          m_schedulerTypeId(schedulerTypeId)
    {
    }

  private:
    void DoRun() override
    {
        ObjectFactory factory(m_schedulerTypeId);
        Ptr<TwtScheduler> scheduler = factory.Create<TwtScheduler>();

        std::vector<TwtStationInfo> stations;
        for (uint32_t i = 0; i < 12; ++i)
        {
            TwtStationInfo sta;
            sta.id = i;
            sta.weight = 1.0 + (i % 3);             // heterogeneous weights
            sta.dutyCycleBudget = 0.02 + 0.01 * (i % 4);
            sta.attemptSuccessProb = 0.4 + 0.15 * (i % 4);
            sta.attemptAirtime = MilliSeconds(1 + (i % 2));
            sta.wakeOverhead = MicroSeconds(500);
            stations.push_back(sta);
        }

        auto entries = scheduler->Recompute(stations);
        NS_TEST_ASSERT_MSG_EQ(entries.size(),
                              stations.size(),
                              "All stations must be scheduled");

        // Pairwise disjoint service periods.
        for (size_t i = 0; i < entries.size(); ++i)
        {
            for (size_t j = i + 1; j < entries.size(); ++j)
            {
                NS_TEST_ASSERT_MSG_EQ(EntriesDisjoint(entries[i], entries[j]),
                                      true,
                                      "Service periods of stations "
                                          << entries[i].stationId << " and "
                                          << entries[j].stationId << " overlap");
            }
        }

        // Per-station duty-cycle budget respected.
        for (const auto& entry : entries)
        {
            const auto& sta = stations[entry.stationId];
            double dutyCycle =
                entry.wakeDuration.GetSeconds() / entry.wakeInterval.GetSeconds();
            NS_TEST_ASSERT_MSG_LT_OR_EQ(dutyCycle,
                                        sta.dutyCycleBudget + 1e-9,
                                        "Duty cycle budget violated for station "
                                            << entry.stationId);
        }
    }

    std::string m_schedulerTypeId; ///< TypeId name of the scheduler under test
};

/**
 * @ingroup aoi-twt-tests
 * TestSuite for module aoi-twt
 */
class AoiTwtTestSuite : public TestSuite
{
  public:
    AoiTwtTestSuite()
        : TestSuite("aoi-twt", Type::UNIT)
    {
        AddTestCase(new TwtAoiModelTestCase, TestCase::Duration::QUICK);
        AddTestCase(new TwtSchedulerFeasibilityTestCase(
                        "ns3::EqualIntervalTwtScheduler",
                        "EqualIntervalTwtScheduler feasibility"),
                    TestCase::Duration::QUICK);
        AddTestCase(new TwtSchedulerFeasibilityTestCase(
                        "ns3::HarmonicGreedyTwtScheduler",
                        "HarmonicGreedyTwtScheduler feasibility"),
                    TestCase::Duration::QUICK);
    }
};

/**
 * @ingroup aoi-twt-tests
 * Static variable for test initialization
 */
static AoiTwtTestSuite g_aoiTwtTestSuite;
