#ifndef TWT_SCHEDULER_H
#define TWT_SCHEDULER_H

#include "twt-schedule.h"

#include "ns3/object.h"

#include <vector>

namespace ns3
{

/**
 * @ingroup aoi-twt
 *
 * Abstract base class for AP-side TWT schedule policies.
 *
 * A scheduler maps per-station characteristics (AoI weight, energy budget,
 * channel quality) to a set of TWT agreements, i.e. (period, offset,
 * duration) triples. Concrete subclasses implement specific policies; the
 * MAC-layer mechanism that installs the agreements is intentionally kept
 * separate so that policies can be developed and unit-tested standalone.
 */
class TwtScheduler : public Object
{
  public:
    /**
     * Register this type.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Compute a TWT schedule for a set of stations.
     *
     * @param stations Per-station inputs.
     * @return One schedule entry per station that could be scheduled;
     *         stations that could not be placed are omitted (a warning is
     *         logged).
     */
    virtual std::vector<TwtScheduleEntry> Recompute(
        const std::vector<TwtStationInfo>& stations) = 0;
};

/**
 * @ingroup aoi-twt
 *
 * Naive baseline: all stations share one wake interval with offsets
 * staggered uniformly.
 *
 * The common wake interval is chosen as the smallest value that respects
 * every station's duty-cycle budget and fits all service periods
 * back-to-back.
 */
class EqualIntervalTwtScheduler : public TwtScheduler
{
  public:
    /**
     * Register this type.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    std::vector<TwtScheduleEntry> Recompute(
        const std::vector<TwtStationInfo>& stations) override;
};

/**
 * @ingroup aoi-twt
 *
 * Weighted AoI-aware scheduler: square-root-law period assignment with
 * harmonic rounding and greedy non-overlapping offset packing.
 *
 * Algorithm:
 * 1. Per station, pick the energy-tight optimal SP length (attempt count)
 *    via TwtAoiModel::OptimalAttemptsPerSp.
 * 2. Assign ideal periods T_i = max(d_i / rho_i, sqrt(lambda d_i / (w_i a_i)))
 *    where a_i = 1/pSucc_i - 1/2, with lambda found by bisection so that the
 *    schedule density sum_i d_i / T_i meets the configured target.
 * 3. Round periods up to the harmonic grid T0 * 2^j (T0 = smallest ideal
 *    period), preserving energy feasibility and guaranteeing packability.
 * 4. Place offsets greedily in increasing period order, checking conflicts
 *    over the hyperperiod.
 */
class HarmonicGreedyTwtScheduler : public TwtScheduler
{
  public:
    /**
     * Register this type.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    std::vector<TwtScheduleEntry> Recompute(
        const std::vector<TwtStationInfo>& stations) override;

  private:
    /**
     * Find lambda such that the density sum_i d_i / T_i(lambda) is at most
     * the target, by bisection on the monotone density function.
     *
     * @param minPeriods Per-station energy-tight minimum periods (seconds).
     * @param durations Per-station SP durations (seconds).
     * @param sqrtCoeffs Per-station coefficients d_i / (w_i a_i) of the
     *                   square-root law.
     * @return Per-station ideal (unrounded) periods in seconds.
     */
    std::vector<double> AssignPeriods(const std::vector<double>& minPeriods,
                                      const std::vector<double>& durations,
                                      const std::vector<double>& sqrtCoeffs) const;

    double m_densityTarget{0.9}; ///< Max schedule density before rounding
    uint32_t m_maxAttempts{8};   ///< Max attempts per SP considered
};

} // namespace ns3

#endif // TWT_SCHEDULER_H
