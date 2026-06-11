/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TWT_POWER_SAVE_MANAGER_H
#define TWT_POWER_SAVE_MANAGER_H

#include "power-save-manager.h"

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
 * Current simplifications (documented for upstreaming):
 * - The TWT agreement is installed programmatically through attributes
 *   rather than negotiated via TWT Setup action frames.
 * - The first SP is anchored to the time of association (plus the
 *   FirstSpOffset attribute) instead of a TSF-based Target Wake Time.
 * - TIM-based downlink delivery is not used while dozing; downlink frames
 *   buffered at the AP are delivered when the STA is awake during an SP.
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

    Time m_wakeInterval;  ///< TWT wake interval (the SP period)
    Time m_wakeDuration;  ///< nominal duration of each SP
    Time m_firstSpOffset; ///< delay between association and the first SP

    std::map<linkId_t, EventId> m_spStartEvents; ///< per-link next SP start events
    std::map<linkId_t, EventId> m_spEndEvents;   ///< per-link SP end events
    std::map<linkId_t, bool> m_spOngoing;        ///< per-link flag: inside an SP
};

} // namespace ns3

#endif /* TWT_POWER_SAVE_MANAGER_H */
