/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "twt-power-save-manager.h"

#include "sta-wifi-mac.h"
#include "wifi-phy.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TwtPowerSaveManager");

NS_OBJECT_ENSURE_REGISTERED(TwtPowerSaveManager);

TypeId
TwtPowerSaveManager::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TwtPowerSaveManager")
            .SetParent<PowerSaveManager>()
            .SetGroupName("Wifi")
            .AddConstructor<TwtPowerSaveManager>()
            .AddAttribute("WakeInterval",
                          "The TWT wake interval, i.e., the time between the start of "
                          "consecutive TWT service periods.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&TwtPowerSaveManager::m_wakeInterval),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("WakeDuration",
                          "The nominal duration of each TWT service period.",
                          TimeValue(MilliSeconds(8)),
                          MakeTimeAccessor(&TwtPowerSaveManager::m_wakeDuration),
                          MakeTimeChecker(MicroSeconds(1)))
            .AddAttribute("FirstSpOffset",
                          "The delay between the completion of association and the start "
                          "of the first TWT service period.",
                          TimeValue(Time{0}),
                          MakeTimeAccessor(&TwtPowerSaveManager::m_firstSpOffset),
                          MakeTimeChecker(Time{0}));
    return tid;
}

TwtPowerSaveManager::TwtPowerSaveManager()
{
    NS_LOG_FUNCTION(this);
}

TwtPowerSaveManager::~TwtPowerSaveManager()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
TwtPowerSaveManager::ScheduleFirstSp(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    NS_ABORT_MSG_IF(m_wakeDuration >= m_wakeInterval,
                    "The TWT wake duration must be shorter than the wake interval");

    CancelSpEvents(linkId);
    m_spOngoing[linkId] = false;

    NS_LOG_DEBUG("First TWT SP on link " << +linkId << " scheduled at "
                                         << Simulator::Now() + m_firstSpOffset);
    m_spStartEvents[linkId] = Simulator::Schedule(m_firstSpOffset,
                                                  &TwtPowerSaveManager::StartServicePeriod,
                                                  this,
                                                  linkId);
    // doze until the first SP starts
    SleepIfIdle(linkId);
}

void
TwtPowerSaveManager::StartServicePeriod(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    m_spOngoing[linkId] = true;

    // Waking the PHY triggers ChannelAccessManager::NotifyWakeupNow, which
    // restarts channel access for all Txops; frames that were generated
    // while dozing are thus transmitted within this SP.
    if (auto phy = GetStaMac()->GetWifiPhy(linkId); phy->IsStateSleep())
    {
        NS_LOG_DEBUG("TWT SP start: resume PHY on link " << +linkId);
        phy->ResumeFromSleep();
    }

    m_spEndEvents[linkId] = Simulator::Schedule(m_wakeDuration,
                                                &TwtPowerSaveManager::EndServicePeriod,
                                                this,
                                                linkId);
    m_spStartEvents[linkId] = Simulator::Schedule(m_wakeInterval,
                                                  &TwtPowerSaveManager::StartServicePeriod,
                                                  this,
                                                  linkId);
}

void
TwtPowerSaveManager::EndServicePeriod(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    m_spOngoing[linkId] = false;
    SleepIfIdle(linkId);
}

void
TwtPowerSaveManager::SleepIfIdle(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    if (m_spOngoing[linkId])
    {
        NS_LOG_DEBUG("Inside a TWT SP on link " << +linkId << ", do not doze");
        return;
    }

    if (!GetStaMac()->IsAssociated())
    {
        NS_LOG_DEBUG("STA is not associated");
        return;
    }

    if (GetStaMac()->GetPmMode(linkId) == WIFI_PM_ACTIVE)
    {
        NS_LOG_DEBUG("STA on link " << +linkId << " is in active mode");
        return;
    }

    auto phy = GetStaMac()->GetWifiPhy(linkId);
    if (phy->IsStateSleep())
    {
        NS_LOG_DEBUG("PHY operating on link " << +linkId << " is already in sleep state");
        return;
    }

    if (HasRequestedOrGainedChannel(linkId))
    {
        // an SP overrun: doze upon channel release
        NS_LOG_DEBUG("Channel access requested or gained, do not doze yet");
        return;
    }

    NS_LOG_DEBUG("TWT doze: PHY on link " << +linkId << " is put to sleep");
    phy->SetSleepMode(true);
}

void
TwtPowerSaveManager::CancelSpEvents(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    if (auto it = m_spStartEvents.find(linkId); it != m_spStartEvents.cend())
    {
        it->second.Cancel();
    }
    if (auto it = m_spEndEvents.find(linkId); it != m_spEndEvents.cend())
    {
        it->second.Cancel();
    }
}

void
TwtPowerSaveManager::DoNotifyAssocCompleted()
{
    NS_LOG_FUNCTION(this);

    for (const auto linkId : GetStaMac()->GetSetupLinkIds())
    {
        NS_LOG_DEBUG("PM mode for link " << +linkId << ": " << GetStaMac()->GetPmMode(linkId));
        if (GetStaMac()->GetPmMode(linkId) == WifiPowerManagementMode::WIFI_PM_POWERSAVE)
        {
            ScheduleFirstSp(linkId);
        }
    }
}

void
TwtPowerSaveManager::DoNotifyDisassociation()
{
    NS_LOG_FUNCTION(this);

    // base class has resumed PHYs from sleep; here we have to cancel timers
    for (auto& [linkId, event] : m_spStartEvents)
    {
        event.Cancel();
    }
    for (auto& [linkId, event] : m_spEndEvents)
    {
        event.Cancel();
    }
    m_spOngoing.clear();
}

void
TwtPowerSaveManager::DoNotifyPmModeChanged(WifiPowerManagementMode pmMode, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << pmMode << linkId);

    if (pmMode == WifiPowerManagementMode::WIFI_PM_ACTIVE)
    {
        // base class has resumed the PHY from sleep; cancel the SP cycle
        CancelSpEvents(linkId);
        m_spOngoing[linkId] = false;
    }
    else if (pmMode == WifiPowerManagementMode::WIFI_PM_POWERSAVE)
    {
        ScheduleFirstSp(linkId);
    }
}

void
TwtPowerSaveManager::DoNotifyReceivedBeacon(const MgtBeaconHeader& beacon, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << beacon << linkId);

    // TWT wake/doze times are driven by the agreement, not by TBTTs; if the
    // STA happens to be awake outside an SP (e.g., before the first SP),
    // doze as soon as possible
    SleepIfIdle(linkId);
}

void
TwtPowerSaveManager::DoNotifyReceivedFrameAfterPsPoll(Ptr<const WifiMpdu> mpdu, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << mpdu << linkId);

    // this manager does not send PS-Poll frames
    SleepIfIdle(linkId);
}

void
TwtPowerSaveManager::DoNotifyReceivedGroupcast(Ptr<const WifiMpdu> mpdu, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << mpdu << linkId);

    SleepIfIdle(linkId);
}

void
TwtPowerSaveManager::DoNotifyRequestAccess(Ptr<Txop> txop, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << txop << linkId);

    // never defer while unassociated: (re)association management frames
    // must be transmitted immediately
    if (m_spOngoing[linkId] || !GetStaMac()->IsAssociated() ||
        GetStaMac()->GetPmMode(linkId) == WIFI_PM_ACTIVE)
    {
        if (auto phy = GetStaMac()->GetWifiPhy(linkId); phy->IsStateSleep())
        {
            NS_LOG_DEBUG("Resume from sleep STA operating on link " << +linkId);
            phy->ResumeFromSleep();
        }
        return;
    }

    // This is the defining TWT behavior: frames generated outside an SP do
    // not wake the STA; they are transmitted at the start of the next SP.
    NS_LOG_DEBUG("Outside TWT SP on link " << +linkId
                                           << ": defer channel access until the next SP");
}

void
TwtPowerSaveManager::DoNotifyChannelReleased(Ptr<Txop> txop, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << txop << linkId);

    // doze if the SP ended while the STA held the channel (SP overrun)
    SleepIfIdle(linkId);
}

void
TwtPowerSaveManager::DoTxDropped(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu)
{
    NS_LOG_FUNCTION(this << reason << *mpdu);

    for (const auto& linkId : GetStaMac()->GetLinkIds())
    {
        SleepIfIdle(linkId);
    }
}

} // namespace ns3
