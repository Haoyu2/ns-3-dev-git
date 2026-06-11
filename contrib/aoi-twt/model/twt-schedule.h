#ifndef TWT_SCHEDULE_H
#define TWT_SCHEDULE_H

#include "ns3/nstime.h"

#include <cstdint>
#include <vector>

/**
 * @defgroup aoi-twt AoI-aware TWT scheduling
 *
 * Algorithms and analytical models for Age-of-Information-aware
 * 802.11ax Target Wake Time (TWT) schedule design.
 */

namespace ns3
{

/**
 * @ingroup aoi-twt
 *
 * Per-station input to a TWT schedule computation.
 *
 * Describes the traffic, channel and energy characteristics of one station
 * that the AP-side scheduler needs in order to assign a TWT agreement.
 */
struct TwtStationInfo
{
    uint32_t id{0};                ///< Station identifier (scheduler-scoped)
    double weight{1.0};            ///< AoI weight w_i (relative importance)
    double dutyCycleBudget{1.0};   ///< Max awake fraction rho_i in (0, 1]
    double attemptSuccessProb{1.0}; ///< Per-attempt delivery probability p_i in (0, 1]
    Time attemptAirtime;           ///< Airtime s_i of one transmission attempt
    Time wakeOverhead;             ///< Per-wake awake overhead c_w (ramp-up, sync)
};

/**
 * @ingroup aoi-twt
 *
 * One computed TWT agreement: the (period, offset, duration) triple
 * assigned to a station.
 */
struct TwtScheduleEntry
{
    uint32_t stationId{0}; ///< Station identifier (matches TwtStationInfo::id)
    Time wakeInterval;     ///< TWT wake interval (period) T_i
    Time wakeDuration;     ///< Service period duration d_i
    Time offset;           ///< First wake time offset phi_i in [0, T_i)
};

/**
 * @ingroup aoi-twt
 *
 * Analytical Age-of-Information model for a single station under a
 * periodic TWT schedule (renewal-reward analysis).
 *
 * Model assumptions: generate-at-will sampling at service period (SP)
 * start, up to k transmission attempts per SP each succeeding i.i.d.
 * with probability p, and AoI reset to the in-SP delivery delay on
 * success.
 */
class TwtAoiModel
{
  public:
    /**
     * Probability that at least one of k attempts in an SP succeeds.
     *
     * @param p Per-attempt success probability in [0, 1].
     * @param k Number of attempts the SP can hold (k >= 1).
     * @return SP-level success probability 1 - (1 - p)^k.
     */
    static double SuccessProbPerSp(double p, uint32_t k);

    /**
     * Time-average AoI under a periodic schedule.
     *
     * E[A] = delta + T * (1 / pSucc - 1/2), where successful SPs form a
     * renewal process with geometric inter-success period count.
     *
     * @param period Wake interval T.
     * @param pSucc SP-level success probability in (0, 1].
     * @param delta Mean in-SP delivery delay.
     * @return Time-average age of information.
     */
    static Time MeanAoi(Time period, double pSucc, Time delta);

    /**
     * Expected peak AoI under a periodic schedule.
     *
     * E[A_peak] = delta + T / pSucc.
     *
     * @param period Wake interval T.
     * @param pSucc SP-level success probability in (0, 1].
     * @param delta Mean in-SP delivery delay.
     * @return Expected peak age of information.
     */
    static Time PeakAoi(Time period, double pSucc, Time delta);

    /**
     * Energy-tight optimal number of attempts per SP for one station.
     *
     * Minimizes f(k) = ((k + c) * s / rho) * (1 / (1 - (1-p)^k) - 1/2),
     * i.e. the mean AoI when the wake interval is set to the smallest value
     * allowed by the duty-cycle budget (T = (k*s + c_w) / rho), where
     * c = c_w / s is the per-wake overhead (radio ramp-up, synchronization)
     * expressed in units of attempt airtime.
     *
     * Without overhead (c = 0) the single attempt k* = 1 is provably
     * optimal for every p, since f(2) - f(1) = p / (2 (2 - p)) > 0; only a
     * positive per-wake cost ever makes batching retries worthwhile.
     *
     * @param p Per-attempt success probability in (0, 1].
     * @param wakeOverheadRatio Per-wake overhead divided by attempt
     *                          airtime, c = c_w / s (>= 0).
     * @param maxAttempts Upper bound on attempts to consider (>= 1).
     * @return The minimizing attempt count k* in [1, maxAttempts].
     */
    static uint32_t OptimalAttemptsPerSp(double p,
                                         double wakeOverheadRatio,
                                         uint32_t maxAttempts);
};

} // namespace ns3

#endif // TWT_SCHEDULE_H
