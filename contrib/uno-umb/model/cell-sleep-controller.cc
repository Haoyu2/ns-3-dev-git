/*
 * Copyright (c) 2026 Haoyu
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "cell-sleep-controller.h"

#include "ns3/lte-enb-net-device.h"
#include "ns3/lte-enb-phy.h"
#include "ns3/mobility-model.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <utility>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("CellSleepController");

CellSleepPolicyMode
ParseCellSleepPolicy(const std::string& policy)
{
    if (policy == "all-on")
    {
        return CellSleepPolicyMode::AllOn;
    }
    if (policy == "threshold")
    {
        return CellSleepPolicyMode::Threshold;
    }
    if (policy == "aggressive")
    {
        return CellSleepPolicyMode::Aggressive;
    }
    if (policy == "twin")
    {
        return CellSleepPolicyMode::Twin;
    }
    if (policy == "adaptive-twin")
    {
        return CellSleepPolicyMode::AdaptiveTwin;
    }
    NS_ABORT_MSG("Unknown policy '" << policy
                                   << "'. Use all-on, threshold, aggressive, twin, or "
                                      "adaptive-twin.");
}

static double
DistanceMeters(Ptr<Node> a, Ptr<Node> b)
{
    return CalculateDistance(a->GetObject<MobilityModel>()->GetPosition(),
                             b->GetObject<MobilityModel>()->GetPosition());
}

static double
EstimateRsrpDbm(double txPowerDbm, double distanceMeters)
{
    const double distanceKm = std::max(distanceMeters / 1000.0, 0.01);
    const double pathLossDb = 128.1 + 37.6 * std::log10(distanceKm);
    return txPowerDbm - pathLossDb;
}

static double
FindMaxActiveUtilization(const std::vector<double>& loadMbps,
                         const std::vector<bool>& active,
                         double cellCapacityMbps)
{
    double maxUtilization = 0.0;
    for (uint32_t cell = 0; cell < loadMbps.size(); ++cell)
    {
        if (active[cell])
        {
            maxUtilization = std::max(maxUtilization, loadMbps[cell] / cellCapacityMbps);
        }
    }
    return maxUtilization;
}

CellSleepController::CellSleepController(const CellSleepControllerConfig& config,
                                         std::ofstream* eventCsv)
    : m_config(config),
      m_eventCsv(eventCsv),
      m_active(config.enbNodes.GetN(), true),
      m_preferredEnb(config.preferredEnb),
      m_ueRatesMbps(config.ueRateMbpsByUe),
      m_effectiveUncertaintyScale(config.uncertaintyScale)
{
    if (m_preferredEnb.empty())
    {
        m_preferredEnb = m_config.servingEnb;
    }
    if (m_ueRatesMbps.empty())
    {
        m_ueRatesMbps.assign(m_config.servingEnb.size(), m_config.ueRateMbps);
    }
    NS_ABORT_MSG_IF(m_preferredEnb.size() != m_config.servingEnb.size(),
                    "preferredEnb must be empty or match the number of UEs");
    NS_ABORT_MSG_IF(m_ueRatesMbps.size() != m_config.servingEnb.size(),
                    "ueRateMbpsByUe must be empty or match the number of UEs");
    NS_ABORT_MSG_IF(m_config.cellCapacityMbps <= 0.0, "cellCapacityMbps must be positive");
    NS_ABORT_MSG_IF(m_config.adaptiveMinUncertaintyScale <= 0.0,
                    "adaptiveMinUncertaintyScale must be positive");
    NS_ABORT_MSG_IF(m_config.adaptiveMaxUncertaintyScale < m_config.adaptiveMinUncertaintyScale,
                    "adaptiveMaxUncertaintyScale must be at least adaptiveMinUncertaintyScale");
    NS_ABORT_MSG_IF(m_config.adaptiveRelaxation < 0.0 || m_config.adaptiveRelaxation > 1.0,
                    "adaptiveRelaxation must be in [0, 1]");
    NS_ABORT_MSG_IF(m_config.adaptiveLatentLoadThreshold < 0.0,
                    "adaptiveLatentLoadThreshold must be non-negative");
    NS_ABORT_MSG_IF(m_config.adaptiveWakeReliefThreshold < 0.0,
                    "adaptiveWakeReliefThreshold must be non-negative");
}

void
CellSleepController::Start()
{
    Simulator::Schedule(m_config.controlStart, &CellSleepController::RunPolicy, this);
}

void
CellSleepController::FinalizeEnergy()
{
    AccumulateEnergy(m_config.simTime.GetSeconds());
}

double
CellSleepController::GetEnergyJ() const
{
    return m_energyJ;
}

double
CellSleepController::GetActiveCellSeconds() const
{
    return m_activeCellSeconds;
}

uint32_t
CellSleepController::GetUnsafeSleepActions() const
{
    return m_unsafeSleepActions;
}

uint32_t
CellSleepController::GetHandoverRequests() const
{
    return m_handoverRequests;
}

void
CellSleepController::SetUeRateMbps(uint32_t ueIndex, double rateMbps)
{
    NS_ABORT_MSG_IF(ueIndex >= m_ueRatesMbps.size(), "UE index is out of range");
    NS_ABORT_MSG_IF(rateMbps < 0.0, "UE rate must be non-negative");
    m_ueRatesMbps[ueIndex] = rateMbps;
}

double
CellSleepController::GetTotalOfferedLoadMbps() const
{
    double total = 0.0;
    for (double rate : m_ueRatesMbps)
    {
        total += rate;
    }
    return total;
}

std::string
CellSleepController::GetEventCsvHeader()
{
    return "time_s,policy,cell,action,reason,was_active,load_mbps,max_utilization,"
           "min_coverage_margin_db,utilization_uncertainty,coverage_uncertainty_db,"
           "uncertainty_scale,load_shock,max_observed_utilization,latent_load_mbps,"
           "wake_relief,twin_safe";
}

void
CellSleepController::RunPolicy()
{
    const double now = Simulator::Now().GetSeconds();
    AccumulateEnergy(now);

    std::vector<double> loadMbps = ComputeLoadMbps();
    UpdateAdaptiveUncertaintyScale(loadMbps);
    std::vector<bool> desiredActive(m_active.size(), true);

    if (m_config.policy != CellSleepPolicyMode::AllOn)
    {
        ApplySleepSelection(loadMbps, desiredActive);
    }

    for (uint32_t cell = 0; cell < desiredActive.size(); ++cell)
    {
        const double decisionLoadMbps = loadMbps[cell];
        if (desiredActive[cell] && !m_active[cell])
        {
            const CellSleepDecisionEstimate estimate =
                EstimateSleepRisk(cell, desiredActive, loadMbps);
            SetEnbTxPower(cell, m_config.activeTxPowerDbm);
            m_active[cell] = true;
            RestorePreferredCell(cell);
            WriteDecision(cell, "wake", "policy-selected-active", decisionLoadMbps, estimate);
            loadMbps = ComputeLoadMbps();
        }
        else if (!desiredActive[cell] && m_active[cell])
        {
            const CellSleepDecisionEstimate estimate =
                EstimateSleepRisk(cell, desiredActive, loadMbps);
            if (!estimate.safe && !UsesRiskGate())
            {
                ++m_unsafeSleepActions;
            }
            OffloadCell(cell, desiredActive);
            Simulator::Schedule(MilliSeconds(80),
                                &CellSleepController::SetEnbTxPower,
                                this,
                                cell,
                                m_config.sleepTxPowerDbm);
            m_active[cell] = false;
            WriteDecision(cell, "sleep", "policy-selected-sleep", decisionLoadMbps, estimate);
            loadMbps = ComputeLoadMbps();
        }
        else
        {
            WriteDecision(cell,
                          desiredActive[cell] ? "keep-active" : "keep-sleep",
                          "no-state-change",
                          decisionLoadMbps,
                          EstimateSleepRisk(cell, desiredActive, loadMbps));
        }
    }

    if (Simulator::Now() + m_config.controlInterval < m_config.simTime)
    {
        Simulator::Schedule(m_config.controlInterval, &CellSleepController::RunPolicy, this);
    }
}

bool
CellSleepController::UsesRiskGate() const
{
    return m_config.policy == CellSleepPolicyMode::Twin ||
           m_config.policy == CellSleepPolicyMode::AdaptiveTwin;
}

void
CellSleepController::UpdateAdaptiveUncertaintyScale(const std::vector<double>& loadMbps)
{
    double maxUtilization = 0.0;
    double loadShock = 0.0;
    for (uint32_t cell = 0; cell < loadMbps.size(); ++cell)
    {
        maxUtilization = std::max(maxUtilization, loadMbps[cell] / m_config.cellCapacityMbps);
        if (m_previousLoadMbps.size() == loadMbps.size())
        {
            loadShock = std::max(loadShock,
                                 std::abs(loadMbps[cell] - m_previousLoadMbps[cell]) /
                                     m_config.cellCapacityMbps);
        }
    }

    m_lastLoadShock = loadShock;
    m_lastMaxUtilization = maxUtilization;
    m_previousLoadMbps = loadMbps;

    if (m_config.policy != CellSleepPolicyMode::AdaptiveTwin)
    {
        m_effectiveUncertaintyScale = m_config.uncertaintyScale;
        return;
    }

    const double utilizationStress =
        std::max(maxUtilization / std::max(m_config.utilizationLimit, 1e-9) - 0.70, 0.0);
    const double targetScale =
        std::clamp(m_config.uncertaintyScale + m_config.adaptiveLoadShockGain * loadShock +
                       m_config.adaptiveUtilizationGain * utilizationStress,
                   m_config.adaptiveMinUncertaintyScale,
                   m_config.adaptiveMaxUncertaintyScale);

    if (targetScale > m_effectiveUncertaintyScale)
    {
        m_effectiveUncertaintyScale = targetScale;
    }
    else
    {
        m_effectiveUncertaintyScale =
            m_effectiveUncertaintyScale -
            m_config.adaptiveRelaxation * (m_effectiveUncertaintyScale - targetScale);
    }
    m_effectiveUncertaintyScale =
        std::clamp(m_effectiveUncertaintyScale,
                   m_config.adaptiveMinUncertaintyScale,
                   m_config.adaptiveMaxUncertaintyScale);
}

double
CellSleepController::GetEffectiveUncertaintyScale() const
{
    return m_effectiveUncertaintyScale;
}

void
CellSleepController::ApplySleepSelection(const std::vector<double>& loadMbps,
                                         std::vector<bool>& desiredActive)
{
    std::vector<uint32_t> order(loadMbps.size());
    for (uint32_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&loadMbps](uint32_t a, uint32_t b) {
        return loadMbps[a] < loadMbps[b];
    });

    uint32_t activeCount = desiredActive.size();
    const double threshold =
        (m_config.policy == CellSleepPolicyMode::Aggressive) ? m_config.aggressiveSleepUeThreshold
                                                             : m_config.sleepUeThreshold;

    for (uint32_t cell : order)
    {
        const double servedUes = loadMbps[cell] / m_config.ueRateMbps;
        if (servedUes > threshold || activeCount <= m_config.minActiveEnbs)
        {
            continue;
        }

        if (ShouldWakeForLatentDemand(cell, loadMbps))
        {
            continue;
        }

        if (UsesRiskGate())
        {
            CellSleepDecisionEstimate estimate = EstimateSleepRisk(cell, desiredActive, loadMbps);
            if (!estimate.safe)
            {
                continue;
            }
        }

        desiredActive[cell] = false;
        --activeCount;
    }
}

std::vector<double>
CellSleepController::ComputeLoadMbps() const
{
    std::vector<double> load(m_config.enbNodes.GetN(), 0.0);
    for (uint32_t u = 0; u < m_config.servingEnb.size(); ++u)
    {
        load[m_config.servingEnb[u]] += m_ueRatesMbps[u];
    }
    return load;
}

double
CellSleepController::ComputePreferredLoadMbps(uint32_t cell) const
{
    double load = 0.0;
    for (uint32_t u = 0; u < m_preferredEnb.size(); ++u)
    {
        if (m_preferredEnb[u] == cell)
        {
            load += m_ueRatesMbps[u];
        }
    }
    return load;
}

double
CellSleepController::EstimateLatentWakeRelief(
    uint32_t cell,
    const std::vector<double>& currentLoadMbps) const
{
    if (cell >= currentLoadMbps.size() || m_active[cell])
    {
        return 0.0;
    }

    std::vector<double> restoredLoadMbps = currentLoadMbps;
    double preferredLoadMbps = 0.0;
    for (uint32_t u = 0; u < m_preferredEnb.size(); ++u)
    {
        if (m_preferredEnb[u] != cell)
        {
            continue;
        }

        preferredLoadMbps += m_ueRatesMbps[u];
        const uint32_t source = m_config.servingEnb[u];
        if (source < restoredLoadMbps.size() && source != cell)
        {
            restoredLoadMbps[source] = std::max(0.0, restoredLoadMbps[source] - m_ueRatesMbps[u]);
        }
    }
    restoredLoadMbps[cell] = preferredLoadMbps;

    std::vector<bool> restoredActive = m_active;
    restoredActive[cell] = true;

    const double currentPeak =
        FindMaxActiveUtilization(currentLoadMbps, m_active, m_config.cellCapacityMbps);
    const double restoredPeak =
        FindMaxActiveUtilization(restoredLoadMbps, restoredActive, m_config.cellCapacityMbps);
    return std::max(currentPeak - restoredPeak, 0.0);
}

bool
CellSleepController::ShouldWakeForLatentDemand(
    uint32_t cell,
    const std::vector<double>& currentLoadMbps) const
{
    if (m_config.policy != CellSleepPolicyMode::AdaptiveTwin || m_active[cell])
    {
        return false;
    }

    const double preferredLoadMbps = ComputePreferredLoadMbps(cell);
    const double latentLoadMbps = std::max(preferredLoadMbps - currentLoadMbps[cell], 0.0);
    const double latentUes = latentLoadMbps / std::max(m_config.ueRateMbps, 1e-9);
    if (latentUes <= m_config.adaptiveLatentLoadThreshold)
    {
        return false;
    }

    return EstimateLatentWakeRelief(cell, currentLoadMbps) >=
           m_config.adaptiveWakeReliefThreshold;
}

CellSleepDecisionEstimate
CellSleepController::EstimateSleepRisk(uint32_t candidateCell,
                                       const std::vector<bool>& desiredActive,
                                       const std::vector<double>& currentLoadMbps) const
{
    CellSleepDecisionEstimate estimate;
    estimate.latentLoadMbps =
        std::max(ComputePreferredLoadMbps(candidateCell) - currentLoadMbps[candidateCell], 0.0);
    estimate.wakeRelief = EstimateLatentWakeRelief(candidateCell, currentLoadMbps);
    std::vector<bool> activeAfter = desiredActive;
    activeAfter[candidateCell] = false;

    std::vector<double> loadAfter = currentLoadMbps;
    loadAfter[candidateCell] = 0.0;
    double farthestOffloadMeters = 0.0;
    estimate.minCoverageMarginDb = std::numeric_limits<double>::max();

    for (uint32_t u = 0; u < m_config.servingEnb.size(); ++u)
    {
        if (m_config.servingEnb[u] != candidateCell)
        {
            continue;
        }

        const uint32_t target = FindNearestActiveEnb(u, activeAfter, candidateCell);
        if (target == candidateCell)
        {
            estimate.safe = false;
            estimate.minCoverageMarginDb = -std::numeric_limits<double>::infinity();
            return estimate;
        }

        loadAfter[target] += m_ueRatesMbps[u];
        const double distance =
            DistanceMeters(m_config.ueNodes.Get(u), m_config.enbNodes.Get(target));
        farthestOffloadMeters = std::max(farthestOffloadMeters, distance);
        const double rsrpDbm = EstimateRsrpDbm(m_config.activeTxPowerDbm, distance);
        estimate.minCoverageMarginDb =
            std::min(estimate.minCoverageMarginDb, rsrpDbm - m_config.minReliableRsrpDbm);
    }

    if (estimate.minCoverageMarginDb == std::numeric_limits<double>::max())
    {
        estimate.minCoverageMarginDb = 99.0;
    }

    for (uint32_t cell = 0; cell < loadAfter.size(); ++cell)
    {
        if (activeAfter[cell])
        {
            estimate.maxUtilization =
                std::max(estimate.maxUtilization, loadAfter[cell] / m_config.cellCapacityMbps);
        }
    }

    estimate.utilizationUncertainty =
        GetEffectiveUncertaintyScale() * (0.04 + 0.10 * estimate.maxUtilization +
                                          0.06 * (farthestOffloadMeters / 1000.0));
    estimate.coverageUncertaintyDb =
        GetEffectiveUncertaintyScale() * (1.0 + 2.0 * (farthestOffloadMeters / 1000.0));
    estimate.safe =
        (estimate.maxUtilization + estimate.utilizationUncertainty <= m_config.utilizationLimit) &&
        (estimate.minCoverageMarginDb - estimate.coverageUncertaintyDb >=
         m_config.requiredCoverageMarginDb);
    return estimate;
}

uint32_t
CellSleepController::FindNearestActiveEnb(uint32_t ueIndex,
                                          const std::vector<bool>& active,
                                          uint32_t avoidCell) const
{
    uint32_t bestCell = avoidCell;
    double bestDistance = std::numeric_limits<double>::max();
    for (uint32_t cell = 0; cell < active.size(); ++cell)
    {
        if (!active[cell] || cell == avoidCell)
        {
            continue;
        }

        const double distance =
            DistanceMeters(m_config.ueNodes.Get(ueIndex), m_config.enbNodes.Get(cell));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestCell = cell;
        }
    }
    return bestCell;
}

void
CellSleepController::OffloadCell(uint32_t sleepingCell, const std::vector<bool>& desiredActive)
{
    for (uint32_t u = 0; u < m_config.servingEnb.size(); ++u)
    {
        if (m_config.servingEnb[u] != sleepingCell)
        {
            continue;
        }

        const uint32_t target = FindNearestActiveEnb(u, desiredActive, sleepingCell);
        if (target == sleepingCell)
        {
            continue;
        }

        m_config.lteHelper->HandoverRequest(Simulator::Now() + MilliSeconds(10),
                                            m_config.ueDevs.Get(u),
                                            m_config.enbDevs.Get(sleepingCell),
                                            m_config.enbDevs.Get(target));
        m_config.servingEnb[u] = target;
        ++m_handoverRequests;
    }
}

void
CellSleepController::RestorePreferredCell(uint32_t activeCell)
{
    for (uint32_t u = 0; u < m_config.servingEnb.size(); ++u)
    {
        if (m_preferredEnb[u] != activeCell || m_config.servingEnb[u] == activeCell)
        {
            continue;
        }

        const uint32_t source = m_config.servingEnb[u];
        if (!m_active[source])
        {
            continue;
        }

        m_config.lteHelper->HandoverRequest(Simulator::Now() + MilliSeconds(10),
                                            m_config.ueDevs.Get(u),
                                            m_config.enbDevs.Get(source),
                                            m_config.enbDevs.Get(activeCell));
        m_config.servingEnb[u] = activeCell;
        ++m_handoverRequests;
    }
}

void
CellSleepController::SetEnbTxPower(uint32_t cell, double txPowerDbm)
{
    Ptr<LteEnbNetDevice> enb = DynamicCast<LteEnbNetDevice>(m_config.enbDevs.Get(cell));
    enb->GetPhy()->SetTxPower(txPowerDbm);
}

void
CellSleepController::AccumulateEnergy(double nowSeconds)
{
    if (nowSeconds <= m_lastEnergyUpdateSeconds)
    {
        return;
    }

    const double dt = nowSeconds - m_lastEnergyUpdateSeconds;
    for (bool active : m_active)
    {
        m_energyJ += dt * (active ? m_config.activePowerW : m_config.sleepPowerW);
        if (active)
        {
            m_activeCellSeconds += dt;
        }
    }
    m_lastEnergyUpdateSeconds = nowSeconds;
}

void
CellSleepController::WriteDecision(uint32_t cell,
                                   const std::string& action,
                                   const std::string& reason,
                                   double loadMbps,
                                   const CellSleepDecisionEstimate& estimate)
{
    if (!m_eventCsv || !m_eventCsv->is_open())
    {
        return;
    }

    *m_eventCsv << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << ","
                << m_config.policyName << "," << cell << "," << action << "," << reason << ","
                << (m_active[cell] ? 1 : 0) << "," << loadMbps << ","
                << estimate.maxUtilization << "," << estimate.minCoverageMarginDb << ","
                << estimate.utilizationUncertainty << "," << estimate.coverageUncertaintyDb << ","
                << GetEffectiveUncertaintyScale() << "," << m_lastLoadShock << ","
                << m_lastMaxUtilization << "," << estimate.latentLoadMbps << ","
                << estimate.wakeRelief << ","
                << (estimate.safe ? 1 : 0) << "\n";
}

} // namespace ns3
