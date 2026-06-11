#include "twt-schedule.h"

#include "ns3/assert.h"
#include "ns3/log.h"

#include <cmath>
#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TwtSchedule");

double
TwtAoiModel::SuccessProbPerSp(double p, uint32_t k)
{
    NS_ASSERT_MSG(p >= 0.0 && p <= 1.0, "Per-attempt success probability out of range");
    NS_ASSERT_MSG(k >= 1, "At least one attempt per SP is required");
    return 1.0 - std::pow(1.0 - p, static_cast<double>(k));
}

Time
TwtAoiModel::MeanAoi(Time period, double pSucc, Time delta)
{
    NS_ASSERT_MSG(pSucc > 0.0 && pSucc <= 1.0, "SP success probability out of range");
    return delta + Seconds(period.GetSeconds() * (1.0 / pSucc - 0.5));
}

Time
TwtAoiModel::PeakAoi(Time period, double pSucc, Time delta)
{
    NS_ASSERT_MSG(pSucc > 0.0 && pSucc <= 1.0, "SP success probability out of range");
    return delta + Seconds(period.GetSeconds() / pSucc);
}

uint32_t
TwtAoiModel::OptimalAttemptsPerSp(double p, double wakeOverheadRatio, uint32_t maxAttempts)
{
    NS_ASSERT_MSG(p > 0.0 && p <= 1.0, "Per-attempt success probability out of range");
    NS_ASSERT_MSG(wakeOverheadRatio >= 0.0, "Wake overhead must be non-negative");
    NS_ASSERT_MSG(maxAttempts >= 1, "At least one attempt per SP is required");

    uint32_t bestK = 1;
    double bestF = std::numeric_limits<double>::max();
    for (uint32_t k = 1; k <= maxAttempts; ++k)
    {
        // Objective in units of (s / rho); the constant factor does not
        // affect the minimizer.
        double f = (static_cast<double>(k) + wakeOverheadRatio) *
                   (1.0 / SuccessProbPerSp(p, k) - 0.5);
        if (f < bestF)
        {
            bestF = f;
            bestK = k;
        }
    }
    NS_LOG_DEBUG("Optimal attempts per SP for p=" << p << ", c=" << wakeOverheadRatio
                                                  << " is k=" << bestK);
    return bestK;
}

} // namespace ns3
