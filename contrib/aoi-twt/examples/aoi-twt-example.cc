/**
 * @file
 *
 * Compute and print a TWT schedule for a heterogeneous station population,
 * comparing the equal-interval baseline against the harmonic-greedy
 * AoI-aware scheduler. Pure schedule computation; no simulation is run.
 */

#include "ns3/command-line.h"
#include "ns3/object-factory.h"
#include "ns3/twt-schedule.h"
#include "ns3/twt-scheduler.h"

#include <cmath>
#include <iomanip>
#include <iostream>

using namespace ns3;

/**
 * Print one schedule with its per-station analytical mean AoI.
 *
 * @param label Human-readable scheduler name.
 * @param entries The computed schedule.
 * @param stations The scheduler inputs (indexed by station id).
 */
static void
PrintSchedule(const std::string& label,
              const std::vector<TwtScheduleEntry>& entries,
              const std::vector<TwtStationInfo>& stations)
{
    std::cout << "\n=== " << label << " ===\n";
    std::cout << std::setw(4) << "sta" << std::setw(12) << "T [ms]" << std::setw(12)
              << "d [ms]" << std::setw(12) << "phi [ms]" << std::setw(14) << "E[AoI] [ms]"
              << "\n";

    double weightedAoi = 0.0;
    double totalWeight = 0.0;
    for (const auto& entry : entries)
    {
        const auto& sta = stations[entry.stationId];
        auto attempts = static_cast<uint32_t>(
            std::round((entry.wakeDuration - sta.wakeOverhead).GetSeconds() /
                       sta.attemptAirtime.GetSeconds()));
        double pSucc = TwtAoiModel::SuccessProbPerSp(sta.attemptSuccessProb, attempts);
        Time meanAoi = TwtAoiModel::MeanAoi(entry.wakeInterval, pSucc, sta.attemptAirtime);

        weightedAoi += sta.weight * static_cast<double>(meanAoi.GetMilliSeconds());
        totalWeight += sta.weight;

        std::cout << std::setw(4) << entry.stationId << std::setw(12) << std::fixed
                  << std::setprecision(2)
                  << entry.wakeInterval.GetSeconds() * 1e3 << std::setw(12)
                  << entry.wakeDuration.GetSeconds() * 1e3 << std::setw(12)
                  << entry.offset.GetSeconds() * 1e3 << std::setw(14)
                  << meanAoi.GetSeconds() * 1e3 << "\n";
    }
    std::cout << "Weighted mean AoI: " << weightedAoi / totalWeight << " ms\n";
}

int
main(int argc, char* argv[])
{
    uint32_t nStations = 10;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations to schedule", nStations);
    cmd.Parse(argc, argv);

    std::vector<TwtStationInfo> stations;
    for (uint32_t i = 0; i < nStations; ++i)
    {
        TwtStationInfo sta;
        sta.id = i;
        sta.weight = 1.0 + (i % 3);
        sta.dutyCycleBudget = 0.02 + 0.01 * (i % 4);
        sta.attemptSuccessProb = 0.4 + 0.15 * (i % 4);
        sta.attemptAirtime = MilliSeconds(2);
        sta.wakeOverhead = MilliSeconds(1);
        stations.push_back(sta);
    }

    for (const auto& typeId :
         {std::string("ns3::EqualIntervalTwtScheduler"),
          std::string("ns3::HarmonicGreedyTwtScheduler")})
    {
        ObjectFactory factory(typeId);
        Ptr<TwtScheduler> scheduler = factory.Create<TwtScheduler>();
        PrintSchedule(typeId, scheduler->Recompute(stations), stations);
    }

    return 0;
}
