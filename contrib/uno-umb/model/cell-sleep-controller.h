/*
 * Copyright (c) 2026 Haoyu
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef CELL_SLEEP_CONTROLLER_H
#define CELL_SLEEP_CONTROLLER_H

#include "ns3/lte-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace ns3
{

/**
 * @defgroup uno-umb UNO-UMB
 *
 * QoS-aware cell sleep control experiments for cellular RAN energy saving.
 */

/**
 * @ingroup uno-umb
 *
 * Cell sleep policy used by the controller.
 */
enum class CellSleepPolicyMode
{
    AllOn,      //!< Keep every cell active.
    Threshold,  //!< Sleep low-load cells using a UE-count threshold.
    Aggressive, //!< Sleep more cells using a looser UE-count threshold.
    Twin,       //!< Sleep only when the risk estimate is safe.
    AdaptiveTwin //!< Twin policy with online uncertainty adaptation.
};

/**
 * @ingroup uno-umb
 *
 * Risk estimate produced by the lightweight digital twin.
 */
struct CellSleepDecisionEstimate
{
    bool safe{false};                    //!< Whether the candidate sleep action is safe.
    double maxUtilization{0.0};          //!< Maximum predicted active-cell utilization.
    double minCoverageMarginDb{0.0};     //!< Minimum predicted coverage margin in dB.
    double utilizationUncertainty{0.0};  //!< Utilization uncertainty margin.
    double forecastUtilizationUncertainty{0.0}; //!< Selected forecast-error utilization margin.
    double coverageUncertaintyDb{0.0};   //!< Coverage uncertainty margin in dB.
    double latentLoadMbps{0.0};          //!< Preferred-cell load hidden by current offload state.
    double wakeRelief{0.0};              //!< Peak utilization reduction from waking the cell.
};

/**
 * @ingroup uno-umb
 *
 * Runtime configuration for the cell sleep controller.
 */
struct CellSleepControllerConfig
{
    CellSleepPolicyMode policy{CellSleepPolicyMode::Twin}; //!< Policy mode.
    std::string policyName{"twin"};                       //!< Policy name for CSV output.
    Ptr<LteHelper> lteHelper;                              //!< LTE helper used for handovers.
    NodeContainer enbNodes;                                //!< eNB nodes.
    NodeContainer ueNodes;                                 //!< UE nodes.
    NetDeviceContainer enbDevs;                            //!< eNB LTE devices.
    NetDeviceContainer ueDevs;                             //!< UE LTE devices.
    std::vector<uint32_t> servingEnb;                      //!< Current serving eNB per UE.
    std::vector<uint32_t> preferredEnb;                    //!< Preferred eNB per UE.
    std::vector<double> ueRateMbpsByUe;                    //!< Offered load per UE.
    Time simTime{Seconds(0)};                              //!< Total simulation time.
    Time controlStart{Seconds(0)};                         //!< First decision time.
    Time controlInterval{Seconds(1)};                      //!< Decision interval.
    double ueRateMbps{1.0};                                //!< Offered load per UE.
    double activeTxPowerDbm{43.0};                         //!< Active eNB Tx power.
    double sleepTxPowerDbm{-60.0};                         //!< Sleeping eNB Tx power.
    double cellCapacityMbps{18.0};                         //!< Nominal cell capacity.
    double sleepUeThreshold{2.0};                          //!< Conservative sleep threshold.
    double aggressiveSleepUeThreshold{7.0};                //!< Aggressive sleep threshold.
    uint32_t minActiveEnbs{1};                             //!< Minimum active eNBs.
    double minReliableRsrpDbm{-95.0};                      //!< Minimum reliable RSRP proxy.
    double requiredCoverageMarginDb{3.0};                  //!< Required coverage margin.
    double utilizationLimit{0.78};                         //!< Utilization safety limit.
    double uncertaintyScale{1.0};                          //!< Risk uncertainty multiplier.
    double forecastUtilizationMargin{0.0};                 //!< Base forecast-error utilization margin.
    double selectiveForecastUtilizationMargin{0.0};        //!< Near-limit forecast-error margin.
    double forecastMarginTriggerSlack{0.0};                //!< Slack for selective forecast margin.
    double forecastMarginTriggerMaxOffloadMeters{0.0};     //!< Max offload distance for trigger.
    double adaptiveMinUncertaintyScale{0.5};               //!< Minimum adaptive multiplier.
    double adaptiveMaxUncertaintyScale{2.5};               //!< Maximum adaptive multiplier.
    double adaptiveLoadShockGain{1.5};                     //!< Load-shock adaptation gain.
    double adaptiveUtilizationGain{1.0};                   //!< Utilization adaptation gain.
    double adaptiveRelaxation{0.25};                       //!< Stable-period relaxation rate.
    double adaptiveLatentLoadThreshold{4.0};               //!< UE-equivalent latent load threshold.
    double adaptiveWakeReliefThreshold{0.08};              //!< Minimum peak utilization relief.
    bool reevaluateOnDemandChange{true};                   //!< Re-run policy after load changes.
    Time handoverGuardTime{Seconds(1.25)};                 //!< Minimum time between UE handovers.
    double activePowerW{180.0};                            //!< Active eNB power draw.
    double sleepPowerW{25.0};                              //!< Sleeping eNB power draw.
};

/**
 * @ingroup uno-umb
 *
 * Parse a policy name.
 *
 * @param policy Policy name.
 * @return Matching policy mode.
 */
CellSleepPolicyMode ParseCellSleepPolicy(const std::string& policy);

/**
 * @ingroup uno-umb
 *
 * Lightweight QoS-aware cell sleep controller.
 *
 * The controller periodically evaluates whether low-load cells should sleep.
 * Non-twin policies apply threshold decisions directly. The twin policy first
 * predicts utilization and coverage risk, including uncertainty margins, and
 * applies only decisions that satisfy the configured safety limits.
 */
class CellSleepController
{
  public:
    /**
     * Create a controller.
     *
     * @param config Controller configuration.
     * @param eventCsv Optional event CSV output stream.
     */
    CellSleepController(const CellSleepControllerConfig& config, std::ofstream* eventCsv);

    /**
     * Schedule the first controller decision.
     */
    void Start();

    /**
     * Accumulate energy until the configured simulation end time.
     */
    void FinalizeEnergy();

    /**
     * @return Consumed energy in joules.
     */
    double GetEnergyJ() const;

    /**
     * @return Active cell-seconds accumulated by the controller.
     */
    double GetActiveCellSeconds() const;

    /**
     * @return Number of unsafe sleep actions selected by non-twin policies.
     */
    uint32_t GetUnsafeSleepActions() const;

    /**
     * @return Number of handover requests issued by the controller.
     */
    uint32_t GetHandoverRequests() const;

    /**
     * Update one UE's offered-load estimate.
     *
     * @param ueIndex UE index.
     * @param rateMbps Offered load in Mb/s.
     */
    void SetUeRateMbps(uint32_t ueIndex, double rateMbps);

    /**
     * Update the active forecast-error utilization margin.
     *
     * @param margin Utilization margin added to risk estimates.
     */
    void SetForecastUtilizationMargin(double margin);

    /**
     * Update the active base and selective forecast-error utilization margins.
     *
     * @param margin Base utilization margin added to risk estimates.
     * @param selectiveMargin Margin used for near-limit risk estimates.
     */
    void SetForecastUtilizationMargins(double margin, double selectiveMargin);

    /**
     * @return Current aggregate offered-load estimate in Mb/s.
     */
    double GetTotalOfferedLoadMbps() const;

    /**
     * @return CSV header for controller event logs.
     */
    static std::string GetEventCsvHeader();

  private:
    /**
     * Evaluate the configured policy and apply state changes.
     *
     * @param scheduleNext Whether to schedule the next periodic decision.
     */
    void RunPolicy(bool scheduleNext);

    /**
     * Run a periodic policy decision.
     */
    void RunScheduledPolicy();

    /**
     * Run an event-triggered policy decision after a demand change.
     */
    void RunDemandTriggeredPolicy();

    /**
     * Schedule an event-triggered policy decision when enabled.
     */
    void ScheduleDemandReevaluation();

    /**
     * Select cells to sleep under the current policy.
     *
     * @param loadMbps Current estimated load per cell.
     * @param desiredActive Output active-state vector.
     */
    void ApplySleepSelection(const std::vector<double>& loadMbps, std::vector<bool>& desiredActive);

    /**
     * @return Whether the configured policy gates sleep actions through risk estimates.
     */
    bool UsesRiskGate() const;

    /**
     * Update the adaptive uncertainty multiplier from current load stress.
     *
     * @param loadMbps Current estimated load per cell.
     */
    void UpdateAdaptiveUncertaintyScale(const std::vector<double>& loadMbps);

    /**
     * Compute the scale used by risk estimates in the current interval.
     *
     * @return Effective uncertainty scale.
     */
    double GetEffectiveUncertaintyScale() const;

    /**
     * Compute load from the current serving-cell map.
     *
     * @return Estimated load per cell in Mb/s.
     */
    std::vector<double> ComputeLoadMbps() const;

    /**
     * Compute offered load from UEs that prefer one cell.
     *
     * @param cell Cell index.
     * @return Preferred-cell load in Mb/s.
     */
    double ComputePreferredLoadMbps(uint32_t cell) const;

    /**
     * Estimate the peak-utilization relief from waking a sleeping cell.
     *
     * @param cell Candidate cell.
     * @param currentLoadMbps Current estimated load per cell.
     * @return Reduction in peak active-cell utilization.
     */
    double EstimateLatentWakeRelief(uint32_t cell,
                                    const std::vector<double>& currentLoadMbps) const;

    /**
     * Decide whether adaptive-twin should wake a sleeping cell for latent demand.
     *
     * @param cell Candidate cell.
     * @param currentLoadMbps Current estimated load per cell.
     * @return True when waking the cell should be preferred.
     */
    bool ShouldWakeForLatentDemand(uint32_t cell,
                                   const std::vector<double>& currentLoadMbps) const;

    /**
     * Estimate the risk of sleeping a candidate cell.
     *
     * @param candidateCell Candidate sleeping cell.
     * @param desiredActive Current desired active-state vector.
     * @param currentLoadMbps Current estimated load per cell.
     * @return Risk estimate for the candidate action.
     */
    CellSleepDecisionEstimate EstimateSleepRisk(uint32_t candidateCell,
                                                const std::vector<bool>& desiredActive,
                                                const std::vector<double>& currentLoadMbps) const;

    /**
     * Find the nearest active eNB for a UE.
     *
     * @param ueIndex UE index.
     * @param active Active-state vector.
     * @param avoidCell Cell to avoid.
     * @return Index of the nearest active eNB, or @p avoidCell if none exists.
     */
    uint32_t FindNearestActiveEnb(uint32_t ueIndex,
                                  const std::vector<bool>& active,
                                  uint32_t avoidCell) const;

    /**
     * Offload UEs from a sleeping cell to active neighbors.
     *
     * @param sleepingCell Cell being moved to sleep state.
     * @param desiredActive Desired active-state vector.
     * @return True when all served UEs could be offloaded.
     */
    bool OffloadCell(uint32_t sleepingCell, const std::vector<bool>& desiredActive);

    /**
     * Move preferred UEs back to a newly active cell.
     *
     * @param activeCell Newly active cell.
     */
    void RestorePreferredCell(uint32_t activeCell);

    /**
     * Retry restoring UEs to an active preferred cell.
     *
     * @param activeCell Active cell to retry.
     */
    void RunRestoreRetry(uint32_t activeCell);

    /**
     * Schedule a bounded restore retry for one active cell.
     *
     * @param activeCell Active cell to retry.
     * @param delay Retry delay.
     */
    void ScheduleRestoreRetry(uint32_t activeCell, Time delay);

    /**
     * @return Whether a UE can receive another handover request now.
     *
     * @param ueIndex UE index.
     */
    bool CanRequestHandover(uint32_t ueIndex) const;

    /**
     * @return Remaining wait time before a UE can be handed over.
     *
     * @param ueIndex UE index.
     */
    Time GetHandoverWait(uint32_t ueIndex) const;

    /**
     * Request one UE handover when allowed by the guard interval.
     *
     * @param ueIndex UE index.
     * @param source Source cell.
     * @param target Target cell.
     * @return True when a handover was requested.
     */
    bool RequestHandover(uint32_t ueIndex, uint32_t source, uint32_t target);

    /**
     * Set eNB transmit power.
     *
     * @param cell Cell index.
     * @param txPowerDbm Transmit power in dBm.
     */
    void SetEnbTxPower(uint32_t cell, double txPowerDbm);

    /**
     * Accumulate analytical energy use.
     *
     * @param nowSeconds Current simulation time in seconds.
     */
    void AccumulateEnergy(double nowSeconds);

    /**
     * Write one controller decision row.
     *
     * @param cell Cell index.
     * @param action Controller action name.
     * @param reason Controller reason name.
     * @param loadMbps Estimated cell load in Mb/s.
     * @param estimate Risk estimate.
     */
    void WriteDecision(uint32_t cell,
                       const std::string& action,
                       const std::string& reason,
                       double loadMbps,
                       const CellSleepDecisionEstimate& estimate);

    CellSleepControllerConfig m_config; //!< Controller configuration.
    std::ofstream* m_eventCsv;          //!< Optional event CSV stream.
    std::vector<bool> m_active;         //!< Current active state per eNB.
    std::vector<uint32_t> m_preferredEnb; //!< Preferred eNB per UE.
    std::vector<double> m_ueRatesMbps;  //!< Current offered load per UE.
    std::vector<double> m_previousLoadMbps; //!< Previous interval load estimate.
    std::vector<double> m_lastHandoverRequestSeconds; //!< Last handover request per UE.
    std::vector<bool> m_restoreRetryPending; //!< Pending restore retry per eNB.
    double m_effectiveUncertaintyScale{1.0}; //!< Current uncertainty scale.
    double m_forecastUtilizationMargin{0.0}; //!< Current base forecast-error margin.
    double m_selectiveForecastUtilizationMargin{0.0}; //!< Current near-limit forecast margin.
    double m_lastLoadShock{0.0};             //!< Last normalized load shock.
    double m_lastMaxUtilization{0.0};        //!< Last observed max utilization.
    double m_lastPolicyRunSeconds{-1.0};     //!< Last policy evaluation time.
    double m_lastEnergyUpdateSeconds{0.0}; //!< Last energy integration time.
    double m_energyJ{0.0};                  //!< Integrated energy use.
    double m_activeCellSeconds{0.0};        //!< Integrated active cell-seconds.
    uint32_t m_unsafeSleepActions{0};       //!< Unsafe non-twin sleep action count.
    uint32_t m_handoverRequests{0};         //!< Handover request count.
    bool m_demandReevaluationPending{false}; //!< Whether an immediate decision is queued.
};

} // namespace ns3

#endif // CELL_SLEEP_CONTROLLER_H
