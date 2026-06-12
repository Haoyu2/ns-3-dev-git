/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TWT_POWER_SAVE_MANAGER_H
#define TWT_POWER_SAVE_MANAGER_H

#include "power-save-manager.h"

#include "ns3/mac48-address.h"

#include <map>

namespace ns3
{

/**
 * @ingroup wifi
 *
 * A PowerSaveManager that implements the wake/doze pattern of an individual
 * implicit unannounced non-trigger-based Target Wake Time (TWT) agreement
 * (IEEE 802.11ax). The STA wakes at the start of each TWT service period
 * (SP), stays awake for the nominal wake duration, and dozes in between.
 * Frames generated while dozing are held until the next SP: unlike
 * DefaultPowerSaveManager, a channel access request outside an SP does not
 * wake the PHY.
 *
 * In announced mode (default), the STA signals SP boundaries to the AP by
 * toggling its Power Management bit (a QoS Null with PM=0 at SP start and
 * PM=1 at SP end), so that the AP delivers frames buffered for the STA
 * (e.g., ADDBA Responses, downlink data) during the SP using the standard
 * power-save delivery rules. In unannounced mode the STA silently
 * wakes/dozes; downlink frames then remain buffered at the AP until it is
 * made TWT-aware by other means.
 *
 * Current simplifications (documented for upstreaming):
 * - The TWT agreement is installed programmatically through attributes
 *   rather than negotiated via TWT Setup action frames.
 * - The first SP is anchored to the time of association (plus the
 *   FirstSpOffset attribute) instead of a TSF-based Target Wake Time.
 * - TIM-based downlink delivery is not used while dozing.
 * - An SP is not terminated early upon an End Of Service Period indication;
 *   transmissions that overrun the SP complete before the STA dozes.
 */
class TwtPowerSaveManager : public PowerSaveManager
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();
    TwtPowerSaveManager();
    ~TwtPowerSaveManager() override;

  protected:
    void DoNotifyAssocCompleted() override;
    void DoNotifyDisassociation() override;
    void DoNotifyPmModeChanged(WifiPowerManagementMode pmMode, linkId_t linkId) override;
    void DoNotifyReceivedBeacon(const MgtBeaconHeader& beacon, linkId_t linkId) override;
    void DoNotifyReceivedFrameAfterPsPoll(Ptr<const WifiMpdu> mpdu, linkId_t linkId) override;
    void DoNotifyReceivedGroupcast(Ptr<const WifiMpdu> mpdu, linkId_t linkId) override;
    void DoNotifyRequestAccess(Ptr<Txop> txop, linkId_t linkId) override;
    void DoNotifyChannelReleased(Ptr<Txop> txop, linkId_t linkId) override;
    void DoTxDropped(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu) override;

  private:
    /**
     * Schedule the first SP on the given link, anchored at the current time.
     *
     * @param linkId the ID of the given link
     */
    void ScheduleFirstSp(linkId_t linkId);

    /**
     * Start a service period on the given link: wake the PHY, schedule the
     * SP end and the next SP start.
     *
     * @param linkId the ID of the given link
     */
    void StartServicePeriod(linkId_t linkId);

    /**
     * End the ongoing service period on the given link and doze if idle.
     *
     * @param linkId the ID of the given link
     */
    void EndServicePeriod(linkId_t linkId);

    /**
     * Put the PHY operating on the given link to sleep, unless the STA is
     * inside an SP, channel access is requested or gained, or power save
     * does not apply.
     *
     * @param linkId the ID of the given link
     */
    void SleepIfIdle(linkId_t linkId);

    /**
     * Cancel the SP events scheduled on the given link.
     *
     * @param linkId the ID of the given link
     */
    void CancelSpEvents(linkId_t linkId);

    /**
     * Block the transmission of unicast frames to the AP on the given link
     * until the next SP, so that pending (re)transmissions do not keep the
     * STA awake past the end of an SP.
     *
     * @param linkId the ID of the given link
     */
    void BlockTxUntilNextSp(linkId_t linkId);

    /**
     * Unblock the transmission of unicast frames to the AP on the given link.
     *
     * @param linkId the ID of the given link
     */
    void UnblockTx(linkId_t linkId);

    Time m_wakeInterval;  ///< TWT wake interval (the SP period)
    Time m_wakeDuration;  ///< nominal duration of each SP
    Time m_firstSpOffset; ///< delay or grid offset of the first SP
    Time m_anchorTime;    ///< absolute SP grid anchor (negative = anchor at association)
    bool m_announced;     ///< announce SP boundaries by toggling the PM bit

    std::map<linkId_t, EventId> m_spStartEvents; ///< per-link next SP start events
    std::map<linkId_t, EventId> m_spEndEvents;   ///< per-link SP end events
    std::map<linkId_t, bool> m_spOngoing;        ///< per-link flag: inside an SP
    std::map<linkId_t, bool> m_cycleRunning;     ///< per-link flag: SP cycle scheduled
    std::map<linkId_t, Mac48Address> m_blockedPeer; ///< per-link peer with blocked queues
};

} // namespace ns3

#endif /* TWT_POWER_SAVE_MANAGER_H */
