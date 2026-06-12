/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "twt-power-save-manager.h"

#include "qos-utils.h"
#include "sta-wifi-mac.h"
#include "wifi-mac-queue-scheduler.h"
#include "wifi-phy.h"

#include "ns3/abort.h"
#include "ns3/boolean.h"
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
                          "If AnchorTime is negative: the delay between the completion of "
                          "association and the start of the first TWT service period. "
                          "Otherwise: the offset of the SP grid relative to AnchorTime.",
                          TimeValue(Time{0}),
                          MakeTimeAccessor(&TwtPowerSaveManager::m_firstSpOffset),
                          MakeTimeChecker(Time{0}))
            .AddAttribute("AnchorTime",
                          "If non-negative, SPs start on the absolute time grid "
                          "AnchorTime + FirstSpOffset + k * WakeInterval (mimicking "
                          "TSF-anchored target wake times), so that the SPs of "
                          "different STAs are aligned on a common timeline. If "
                          "negative, the first SP is anchored to association time.",
                          TimeValue(Seconds(-1)),
                          MakeTimeAccessor(&TwtPowerSaveManager::m_anchorTime),
                          MakeTimeChecker(Seconds(-1)))
            .AddAttribute("Announced",
                          "If true, the STA announces SP boundaries to the AP by toggling "
                          "its Power Management bit, so that the AP delivers buffered "
                          "frames during the SP.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&TwtPowerSaveManager::m_announced),
                          MakeBooleanChecker());
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
    m_cycleRunning[linkId] = true;

    Time delay = m_firstSpOffset;
    if (m_anchorTime.IsStrictlyNegative() == false)
    {
        // first occurrence of the grid AnchorTime + FirstSpOffset + k * T
        // at or after the current time
        auto period = m_wakeInterval.GetNanoSeconds();
        auto phase = (m_anchorTime + m_firstSpOffset).GetNanoSeconds() % period;
        auto rem = (phase - Simulator::Now().GetNanoSeconds()) % period;
        delay = NanoSeconds((rem + period) % period);
    }

    NS_LOG_DEBUG("First TWT SP on link " << +linkId << " scheduled at "
                                         << Simulator::Now() + delay);
    m_spStartEvents[linkId] = Simulator::Schedule(delay,
                                                  &TwtPowerSaveManager::StartServicePeriod,
                                                  this,
                                                  linkId);
    // doze until the first SP starts
    BlockTxUntilNextSp(linkId);
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

    UnblockTx(linkId);

    if (m_announced)
    {
        // announce the SP to the AP: the PM bit transition to active makes
        // the AP deliver the frames it has buffered for this STA
        GetStaMac()->SetPowerSaveMode({false, linkId});
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

    if (m_announced && GetStaMac()->GetPmMode(linkId) == WIFI_PM_ACTIVE)
    {
        // best-effort doze announcement: the PM=1 indication rides on the
        // next transmitted frame (or a Data Null). If the channel is bad,
        // the transition may only complete in a later SP; the STA dozes
        // regardless, as TWT wake times are schedule-based.
        GetStaMac()->SetPowerSaveMode({true, linkId});
    }

    // stop pending (re)transmissions from keeping the STA awake: frames
    // left in the queues are transmitted in the next SP
    BlockTxUntilNextSp(linkId);

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

    if (auto pmMode = GetStaMac()->GetPmMode(linkId);
        pmMode == WIFI_PM_ACTIVE || pmMode == WIFI_PM_SWITCHING_TO_ACTIVE ||
        (pmMode == WIFI_PM_SWITCHING_TO_PS && !m_cycleRunning[linkId]))
    {
        // Dozing is allowed while switching to PS only once the SP cycle is
        // running (the doze announcement is best-effort and may complete in
        // a later SP). Before the cycle starts, the STA must stay awake
        // until the initial PM transition completes: dozing would strand
        // the PM Null frame in the queue and the SP cycle would never be
        // anchored.
        NS_LOG_DEBUG("STA on link " << +linkId
                                    << " is (switching to) active mode, or the initial PM "
                                       "transition is still in progress");
        return;
    }

    auto phy = GetStaMac()->GetWifiPhy(linkId);
    if (phy->IsStateSleep())
    {
        NS_LOG_DEBUG("PHY operating on link " << +linkId << " is already in sleep state");
        return;
    }

    // A pending channel access request must not prevent dozing (deferring
    // such transmissions to the next SP is the defining TWT behavior), but
    // an ongoing frame exchange must complete first: doze upon channel
    // release (an SP overrun)
    const auto acList =
        GetStaMac()->GetQosSupported() ? edcaAcIndices : std::list<AcIndex>{AC_BE_NQOS};
    for (const auto aci : acList)
    {
        if (GetStaMac()->GetTxopFor(aci)->GetAccessStatus(linkId) == Txop::GRANTED)
        {
            NS_LOG_DEBUG("Frame exchange in progress, doze upon channel release");
            return;
        }
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
TwtPowerSaveManager::BlockTxUntilNextSp(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    if (m_blockedPeer.contains(linkId) || !GetStaMac()->IsAssociated())
    {
        return;
    }
    auto bssid = GetStaMac()->GetBssid(linkId);
    NS_LOG_DEBUG("Block transmissions to " << bssid << " on link " << +linkId
                                           << " until the next SP");
    GetStaMac()->BlockUnicastTxOnLinks(WifiQueueBlockedReason::POWER_SAVE_MODE,
                                       bssid,
                                       {linkId});
    m_blockedPeer[linkId] = bssid;
}

void
TwtPowerSaveManager::UnblockTx(linkId_t linkId)
{
    NS_LOG_FUNCTION(this << linkId);

    if (auto it = m_blockedPeer.find(linkId); it != m_blockedPeer.cend())
    {
        NS_LOG_DEBUG("Unblock transmissions to " << it->second << " on link " << +linkId);
        GetStaMac()->UnblockUnicastTxOnLinks(WifiQueueBlockedReason::POWER_SAVE_MODE,
                                             it->second,
                                             {linkId});
        m_blockedPeer.erase(it);
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
    for (const auto& [linkId, addr] : m_blockedPeer)
    {
        GetStaMac()->UnblockUnicastTxOnLinks(WifiQueueBlockedReason::POWER_SAVE_MODE,
                                             addr,
                                             {linkId});
    }
    m_blockedPeer.clear();
    m_spOngoing.clear();
    m_cycleRunning.clear();
}

void
TwtPowerSaveManager::DoNotifyPmModeChanged(WifiPowerManagementMode pmMode, linkId_t linkId)
{
    NS_LOG_FUNCTION(this << pmMode << linkId);

    if (pmMode == WifiPowerManagementMode::WIFI_PM_ACTIVE)
    {
        if (!m_cycleRunning[linkId])
        {
            // externally disabled power save before the cycle started
            CancelSpEvents(linkId);
            m_spOngoing[linkId] = false;
            return;
        }
        if (m_announced)
        {
            // our own SP start announcement completed
            if (!m_spOngoing[linkId])
            {
                // the SP ended while the announcement was in flight:
                // immediately announce the doze
                GetStaMac()->SetPowerSaveMode({true, linkId});
            }
            return;
        }
        // unannounced mode: this must be an external request to leave power
        // save mode; stop the SP cycle (base class has resumed the PHY)
        CancelSpEvents(linkId);
        UnblockTx(linkId);
        m_spOngoing[linkId] = false;
        m_cycleRunning[linkId] = false;
    }
    else if (pmMode == WifiPowerManagementMode::WIFI_PM_POWERSAVE)
    {
        if (!m_cycleRunning[linkId])
        {
            // power save (and thus TWT) enabled: anchor the SP cycle
            ScheduleFirstSp(linkId);
        }
        else
        {
            // our own SP end announcement completed: doze
            SleepIfIdle(linkId);
        }
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
