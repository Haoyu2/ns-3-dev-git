#include "twt-scheduler.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TwtScheduler");

NS_OBJECT_ENSURE_REGISTERED(TwtScheduler);
NS_OBJECT_ENSURE_REGISTERED(EqualIntervalTwtScheduler);
NS_OBJECT_ENSURE_REGISTERED(HarmonicGreedyTwtScheduler);

/// Tolerance for treating two SP boundaries as non-overlapping (seconds)
static constexpr double TIME_EPS = 1e-12;

TypeId
TwtScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TwtScheduler").SetParent<Object>().SetGroupName("AoiTwt");
    return tid;
}

TypeId
EqualIntervalTwtScheduler::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EqualIntervalTwtScheduler")
                            .SetParent<TwtScheduler>()
                            .SetGroupName("AoiTwt")
                            .AddConstructor<EqualIntervalTwtScheduler>();
    return tid;
}

std::vector<TwtScheduleEntry>
EqualIntervalTwtScheduler::Recompute(const std::vector<TwtStationInfo>& stations)
{
    NS_LOG_FUNCTION(this << stations.size());
    std::vector<TwtScheduleEntry> entries;
    if (stations.empty())
    {
        return entries;
    }

    // Single attempt per SP; common period large enough that all SPs fit
    // back-to-back and every duty-cycle budget is respected.
    double maxDuration = 0.0;
    double minPeriod = 0.0;
    for (const auto& sta : stations)
    {
        double d = sta.attemptAirtime.GetSeconds() + sta.wakeOverhead.GetSeconds();
        maxDuration = std::max(maxDuration, d);
        minPeriod = std::max(minPeriod, d / sta.dutyCycleBudget);
    }
    double period = std::max(stations.size() * maxDuration, minPeriod);

    double slot = period / stations.size();
    for (uint32_t i = 0; i < stations.size(); ++i)
    {
        TwtScheduleEntry entry;
        entry.stationId = stations[i].id;
        entry.wakeInterval = Seconds(period);
        entry.wakeDuration =
            stations[i].attemptAirtime + stations[i].wakeOverhead;
        entry.offset = Seconds(i * slot);
        entries.push_back(entry);
    }
    return entries;
}

TypeId
HarmonicGreedyTwtScheduler::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::HarmonicGreedyTwtScheduler")
            .SetParent<TwtScheduler>()
            .SetGroupName("AoiTwt")
            .AddConstructor<HarmonicGreedyTwtScheduler>()
            .AddAttribute("DensityTarget",
                          "Maximum schedule density sum(d_i/T_i) before harmonic rounding",
                          DoubleValue(0.9),
                          MakeDoubleAccessor(&HarmonicGreedyTwtScheduler::m_densityTarget),
                          MakeDoubleChecker<double>(0.0, 1.0))
            .AddAttribute("MaxAttemptsPerSp",
                          "Maximum number of transmission attempts per service period",
                          UintegerValue(8),
                          MakeUintegerAccessor(&HarmonicGreedyTwtScheduler::m_maxAttempts),
                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

std::vector<double>
HarmonicGreedyTwtScheduler::AssignPeriods(const std::vector<double>& minPeriods,
                                          const std::vector<double>& durations,
                                          const std::vector<double>& sqrtCoeffs) const
{
    auto periodsFor = [&](double lambda) {
        std::vector<double> periods(minPeriods.size());
        for (size_t i = 0; i < minPeriods.size(); ++i)
        {
            periods[i] = std::max(minPeriods[i], std::sqrt(lambda * sqrtCoeffs[i]));
        }
        return periods;
    };
    auto densityOf = [&](const std::vector<double>& periods) {
        double density = 0.0;
        for (size_t i = 0; i < periods.size(); ++i)
        {
            density += durations[i] / periods[i];
        }
        return density;
    };

    // Energy-tight periods already meet the target: no relaxation needed.
    if (densityOf(periodsFor(0.0)) <= m_densityTarget)
    {
        return periodsFor(0.0);
    }

    // Density is monotone decreasing in lambda: grow an upper bracket,
    // then bisect.
    double hi = 1.0;
    while (densityOf(periodsFor(hi)) > m_densityTarget)
    {
        hi *= 2.0;
        NS_ABORT_MSG_IF(hi > 1e18, "Density target unreachable");
    }
    double lo = 0.0;
    for (int iter = 0; iter < 100; ++iter)
    {
        double mid = (lo + hi) / 2.0;
        if (densityOf(periodsFor(mid)) > m_densityTarget)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    return periodsFor(hi);
}

std::vector<TwtScheduleEntry>
HarmonicGreedyTwtScheduler::Recompute(const std::vector<TwtStationInfo>& stations)
{
    NS_LOG_FUNCTION(this << stations.size());
    std::vector<TwtScheduleEntry> entries;
    if (stations.empty())
    {
        return entries;
    }

    // Step 1: per-station SP sizing (energy-tight optimal attempt count).
    size_t n = stations.size();
    std::vector<double> durations(n);
    std::vector<double> minPeriods(n);
    std::vector<double> sqrtCoeffs(n);
    for (size_t i = 0; i < n; ++i)
    {
        const auto& sta = stations[i];
        double airtime = sta.attemptAirtime.GetSeconds();
        double overhead = sta.wakeOverhead.GetSeconds();
        uint32_t k = TwtAoiModel::OptimalAttemptsPerSp(sta.attemptSuccessProb,
                                                       overhead / airtime,
                                                       m_maxAttempts);
        // The wake overhead is counted as part of the SP: it consumes both
        // energy and schedule space (the station is awake, the AP defers).
        durations[i] = k * airtime + overhead;
        minPeriods[i] = durations[i] / sta.dutyCycleBudget;
        double pSucc = TwtAoiModel::SuccessProbPerSp(sta.attemptSuccessProb, k);
        double ageCoeff = 1.0 / pSucc - 0.5;
        sqrtCoeffs[i] = durations[i] / (sta.weight * ageCoeff);
    }

    // Step 2: square-root-law period assignment under the density target.
    std::vector<double> ideal = AssignPeriods(minPeriods, durations, sqrtCoeffs);

    // Step 3: round periods up to the harmonic grid T0 * 2^j.
    double t0 = *std::min_element(ideal.begin(), ideal.end());
    std::vector<double> periods(n);
    for (size_t i = 0; i < n; ++i)
    {
        double exponent = std::ceil(std::log2(ideal[i] / t0) - 1e-9);
        periods[i] = t0 * std::pow(2.0, std::max(0.0, exponent));
    }

    // Step 3b: re-expand SPs. Rounding the period up leaves unused energy
    // and schedule budget; spending it on extra in-SP attempts raises the
    // SP success probability at zero AoI cost (at fixed T, more attempts
    // never hurt). Cap the expansion so each station keeps both its duty
    // cycle d/T <= rho and its allocated density share d/T_ideal.
    for (size_t i = 0; i < n; ++i)
    {
        const auto& sta = stations[i];
        double airtime = sta.attemptAirtime.GetSeconds();
        double overhead = sta.wakeOverhead.GetSeconds();
        double maxDuration = std::min(durations[i] * periods[i] / ideal[i],
                                      sta.dutyCycleBudget * periods[i]);
        auto k = static_cast<uint32_t>(std::floor((maxDuration - overhead) / airtime));
        k = std::min(k, m_maxAttempts);
        durations[i] = std::max(durations[i], k * airtime + overhead);
    }

    // Step 4: greedy offset packing in increasing period order.
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return periods[a] < periods[b];
    });

    double hyperperiod = *std::max_element(periods.begin(), periods.end());
    // Occupied SP intervals over [0, hyperperiod), kept sorted by start.
    std::vector<std::pair<double, double>> occupied;

    auto isFree = [&](double start, double end) {
        for (const auto& [s, e] : occupied)
        {
            if (start < e - TIME_EPS && s < end - TIME_EPS)
            {
                return false;
            }
        }
        return true;
    };

    for (size_t idx : order)
    {
        double period = periods[idx];
        double duration = durations[idx];
        auto reps = static_cast<uint32_t>(std::round(hyperperiod / period));

        // Candidate offsets: zero, plus every occupied-interval end folded
        // into [0, period).
        std::vector<double> candidates{0.0};
        for (const auto& [s, e] : occupied)
        {
            candidates.push_back(std::fmod(e, period));
        }
        std::sort(candidates.begin(), candidates.end());

        bool placed = false;
        for (double phi : candidates)
        {
            if (phi + duration > period + TIME_EPS)
            {
                continue;
            }
            bool feasible = true;
            for (uint32_t j = 0; j < reps && feasible; ++j)
            {
                feasible = isFree(phi + j * period, phi + j * period + duration);
            }
            if (feasible)
            {
                for (uint32_t j = 0; j < reps; ++j)
                {
                    occupied.emplace_back(phi + j * period, phi + j * period + duration);
                }
                std::sort(occupied.begin(), occupied.end());

                TwtScheduleEntry entry;
                entry.stationId = stations[idx].id;
                entry.wakeInterval = Seconds(period);
                entry.wakeDuration = Seconds(duration);
                entry.offset = Seconds(phi);
                entries.push_back(entry);
                placed = true;
                break;
            }
        }
        if (!placed)
        {
            NS_LOG_WARN("Could not place station " << stations[idx].id
                                                   << " (period=" << period
                                                   << "s, duration=" << duration << "s)");
        }
    }
    return entries;
}

} // namespace ns3
