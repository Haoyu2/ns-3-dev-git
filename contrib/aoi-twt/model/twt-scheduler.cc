#include "twt-scheduler.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("DyadicReservations",
                          "Place SPs via small-first best-fit over dyadic reservations "
                          "(the variant covered by the approximation analysis; combine "
                          "with DensityTarget 0.25). If false, use greedy first-fit "
                          "placement of the true SP durations.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&HarmonicGreedyTwtScheduler::m_dyadicReservations),
                          MakeBooleanChecker());
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

    // Step 3: round periods up to a harmonic grid t0 * 2^j. The choice of
    // the grid anchor t0 matters: each ideal period, folded by powers of
    // two into (tmin/2, tmin], is a candidate anchor (it makes that
    // station's rounding lossless); pick the candidate minimizing the
    // weighted AoI estimate of the rounded periods.
    double tmin = *std::min_element(ideal.begin(), ideal.end());
    std::vector<double> ageCoeffs(n);
    for (size_t i = 0; i < n; ++i)
    {
        ageCoeffs[i] = sqrtCoeffs[i] > 0 ? durations[i] / sqrtCoeffs[i] : 0;
    }
    auto roundedPeriods = [&](double t0) {
        std::vector<double> periods(n);
        for (size_t i = 0; i < n; ++i)
        {
            double exponent = std::ceil(std::log2(ideal[i] / t0) - 1e-9);
            periods[i] = t0 * std::pow(2.0, std::max(0.0, exponent));
        }
        return periods;
    };

    double bestAnchor = tmin;
    double bestCost = std::numeric_limits<double>::max();
    for (size_t c = 0; c < n; ++c)
    {
        double fold = std::ceil(std::log2(ideal[c] / tmin) - 1e-9);
        double anchor = ideal[c] / std::pow(2.0, fold);
        double cost = 0.0;
        auto periods = roundedPeriods(anchor);
        for (size_t i = 0; i < n; ++i)
        {
            // weighted AoI estimate: w_i * a_i * T_i (constant terms omitted)
            cost += ageCoeffs[i] * periods[i];
        }
        if (cost < bestCost)
        {
            bestCost = cost;
            bestAnchor = anchor;
        }
    }
    std::vector<double> periods = roundedPeriods(bestAnchor);

    // Best-of safeguard: the rounding loss can make the multi-level harmonic
    // schedule worse than the best single-period (uniform) schedule, which
    // is always feasible if all SPs fit back-to-back; never return a
    // schedule worse than uniform.
    double uniformPeriod = std::max(*std::max_element(minPeriods.begin(), minPeriods.end()),
                                    std::accumulate(durations.begin(), durations.end(), 0.0));
    double uniformCost = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        uniformCost += ageCoeffs[i] * uniformPeriod;
    }
    bool uniformChosen = false;
    if (uniformCost < bestCost)
    {
        NS_LOG_DEBUG("Uniform schedule (T=" << uniformPeriod
                                            << "s) beats the harmonic assignment");
        periods.assign(n, uniformPeriod);
        ideal = periods; // no rounding slack to re-expand
        uniformChosen = true;
    }

    // Step 3b: re-expand SPs. Rounding the period up leaves unused energy
    // and schedule budget; spending it on extra in-SP attempts raises the
    // SP success probability at zero AoI cost (at fixed T, more attempts
    // never hurt). Cap the expansion so each station keeps both its duty
    // cycle d/T <= rho and its allocated density share d/T_ideal.
    for (size_t i = 0; i < n; ++i)
    {
        const auto& sta = stations[i];
        if (sta.attemptSuccessProb >= 1.0)
        {
            // extra attempts cannot raise the SP success probability:
            // expanding the SP would only waste energy
            continue;
        }
        double airtime = sta.attemptAirtime.GetSeconds();
        double overhead = sta.wakeOverhead.GetSeconds();
        double maxDuration = std::min(durations[i] * periods[i] / ideal[i],
                                      sta.dutyCycleBudget * periods[i]);
        auto k = static_cast<uint32_t>(std::floor((maxDuration - overhead) / airtime));
        k = std::min(k, m_maxAttempts);
        durations[i] = std::max(durations[i], k * airtime + overhead);
    }

    // Step 4: offset packing in increasing period order.
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return periods[a] < periods[b];
    });

    if (m_dyadicReservations && !uniformChosen)
    {
        // Small-first best-fit over dyadic reservations: the placement
        // analyzed in the approximation theorem. Each station reserves the
        // smallest dyadic divisor of the grid base that fits its SP; it
        // transmits only for its true duration inside the reservation.
        // (A uniform schedule packs back-to-back with true durations and
        // needs no reservations.)
        double base = *std::min_element(periods.begin(), periods.end());
        if (std::all_of(durations.begin(), durations.end(), [&](double d) {
                return d <= base + TIME_EPS;
            }))
        {
            struct FreeBlock
            {
                double pos;  ///< block start
                double size; ///< block length
            };

            std::vector<FreeBlock> blocks{{0.0, base}};
            double window = base;
            for (size_t idx : order)
            {
                while (periods[idx] > window * (1 + 1e-9))
                {
                    size_t m = blocks.size();
                    for (size_t b = 0; b < m; ++b)
                    {
                        blocks.push_back({blocks[b].pos + window, blocks[b].size});
                    }
                    window *= 2;
                }
                // smallest dyadic divisor of the base that fits the SP
                double dres =
                    base /
                    std::pow(2.0, std::floor(std::log2(base / durations[idx]) + 1e-9));

                auto best = blocks.end();
                for (auto it = blocks.begin(); it != blocks.end(); ++it)
                {
                    if (it->size >= dres - TIME_EPS &&
                        (best == blocks.end() || it->size < best->size))
                    {
                        best = it;
                    }
                }
                if (best == blocks.end())
                {
                    NS_LOG_WARN("Dyadic packing failed for station "
                                << stations[idx].id << " (reservation=" << dres << "s)");
                    continue;
                }

                TwtScheduleEntry entry;
                entry.stationId = stations[idx].id;
                entry.wakeInterval = Seconds(periods[idx]);
                entry.wakeDuration = Seconds(durations[idx]);
                entry.offset = Seconds(best->pos);
                entries.push_back(entry);

                // carve the reservation from the left end; return the
                // staircase remainder (sizes dres, 2 dres, ..., size/2)
                double pos = best->pos;
                double size = best->size;
                blocks.erase(best);
                double off = pos + dres;
                double s = dres;
                while (off < pos + size - TIME_EPS)
                {
                    blocks.push_back({off, s});
                    off += s;
                    s *= 2;
                }
            }
            return entries;
        }
        NS_LOG_WARN("An SP exceeds the grid base; falling back to greedy placement");
    }

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
