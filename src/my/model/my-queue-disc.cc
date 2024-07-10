/*
 * Copyright © 2011 Marcos Talau
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Marcos Talau (talau@users.sourceforge.net)
 *
 * Thanks to: Duy Nguyen<duy@soe.ucsc.edu> by RED efforts in NS3
 *
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (c) 1990-1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PORT NOTE: This code was ported from ns-2 (queue/red.cc).  Almost all
 * comments have also been ported from NS-2
 */

#include "my-queue-disc.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4-queue-disc-item.h"

namespace ns3
{

QueueCounter::QueueCounter()
{

}
QueueCounter::QueueCounter(uint32_t to)
{
    total = to;
    mean = total;
}
void
QueueCounter::Dequeue(uint32_t src)
{
    waitingLine.push(src);
    counter[src]++;
    if (counter[src] == 1) {
        categories++;
        mean = total / categories;
    }
    while (waitingLine.size() > total)
    {
            uint32_t out = waitingLine.front();
            waitingLine.pop();
            counter[out]--;
            if (counter[out] == 0) {
                categories--;
                mean = categories == 0? total : total / categories;
            }
    }
}

void
QueueCounter::SetTotal(uint32_t t)
{
    total = t;
    mean = total;
}

std::map<uint32_t, uint32_t>
QueueCounter::GetCounter()
{
    return counter;
}
std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>
QueueCounter::GetState()
{
    return {total, categories, mean, current};
}
bool
QueueCounter::IncrementAndMark(uint32_t ipIn, uint32_t n, double th)
{

//    return counter[ipIn] > mean * th / n ; //updated
    return counter[ipIn] > mean * 3 / 5 ; //update1
}


NS_LOG_COMPONENT_DEFINE("MyQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(MyQueueDisc);

TypeId
MyQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MyQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<MyQueueDisc>()
            .AddAttribute("QueueCounterTotal",
                          "QueueCounter total size",
                          UintegerValue(2000),
                          MakeUintegerAccessor(&MyQueueDisc::m_queueCounterTotal),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("MeanPktSize",
                          "Average of packet size",
                          UintegerValue(500),
                          MakeUintegerAccessor(&MyQueueDisc::m_meanPktSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("IdlePktSize",
                          "Average packet size used during idle times. Used when m_cautions = 3",
                          UintegerValue(0),
                          MakeUintegerAccessor(&MyQueueDisc::m_idlePktSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Wait",
                          "True for waiting between dropped packets",
                          BooleanValue(true),
                          MakeBooleanAccessor(&MyQueueDisc::m_isWait),
                          MakeBooleanChecker())
            .AddAttribute("Gentle",
                          "True to increases dropping probability slowly when average queue "
                          "exceeds maxthresh",
                          BooleanValue(true),
                          MakeBooleanAccessor(&MyQueueDisc::m_isGentle),
                          MakeBooleanChecker())
            .AddAttribute("ARED",
                          "True to enable ARED",
                          BooleanValue(false),
                          MakeBooleanAccessor(&MyQueueDisc::m_isARED),
                          MakeBooleanChecker())
            .AddAttribute("AdaptMaxP",
                          "True to adapt m_curMaxP",
                          BooleanValue(false),
                          MakeBooleanAccessor(&MyQueueDisc::m_isAdaptMaxP),
                          MakeBooleanChecker())
            .AddAttribute("FengAdaptive",
                          "True to enable Feng's Adaptive RED",
                          BooleanValue(false),
                          MakeBooleanAccessor(&MyQueueDisc::m_isFengAdaptive),
                          MakeBooleanChecker())
            .AddAttribute("NLRED",
                          "True to enable Nonlinear RED",
                          BooleanValue(false),
                          MakeBooleanAccessor(&MyQueueDisc::m_isNonlinear),
                          MakeBooleanChecker())
            .AddAttribute("MinTh",
                          "Minimum average length threshold in packets/bytes",
                          DoubleValue(5),
                          MakeDoubleAccessor(&MyQueueDisc::m_minTh),
                          MakeDoubleChecker<double>())
            .AddAttribute("MaxTh",
                          "Maximum average length threshold in packets/bytes",
                          DoubleValue(15),
                          MakeDoubleAccessor(&MyQueueDisc::m_maxTh),
                          MakeDoubleChecker<double>())
            .AddAttribute("MaxSize",
                          "The maximum number of packets accepted by this queue disc",
                          QueueSizeValue(QueueSize("25p")),
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker())
            .AddAttribute("QW",
                          "Queue weight related to the exponential weighted moving average (EWMA)",
                          DoubleValue(0.002),
                          MakeDoubleAccessor(&MyQueueDisc::m_qW),
                          MakeDoubleChecker<double>())
            .AddAttribute("LInterm",
                          "The maximum probability of dropping a packet",
                          DoubleValue(50),
                          MakeDoubleAccessor(&MyQueueDisc::m_lInterm),
                          MakeDoubleChecker<double>())
            .AddAttribute("TargetDelay",
                          "Target average queuing delay in ARED",
                          TimeValue(Seconds(0.005)),
                          MakeTimeAccessor(&MyQueueDisc::m_targetDelay),
                          MakeTimeChecker())
            .AddAttribute("Interval",
                          "Time interval to update m_curMaxP",
                          TimeValue(Seconds(0.5)),
                          MakeTimeAccessor(&MyQueueDisc::m_interval),
                          MakeTimeChecker())
            .AddAttribute("Top",
                          "Upper bound for m_curMaxP in ARED",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&MyQueueDisc::m_top),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("Bottom",
                          "Lower bound for m_curMaxP in ARED",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&MyQueueDisc::m_bottom),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("Alpha",
                          "Increment parameter for m_curMaxP in ARED",
                          DoubleValue(0.01),
                          MakeDoubleAccessor(&MyQueueDisc::SetAredAlpha),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("Beta",
                          "Decrement parameter for m_curMaxP in ARED",
                          DoubleValue(0.9),
                          MakeDoubleAccessor(&MyQueueDisc::SetAredBeta),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("FengAlpha",
                          "Decrement parameter for m_curMaxP in Feng's Adaptive RED",
                          DoubleValue(3.0),
                          MakeDoubleAccessor(&MyQueueDisc::SetFengAdaptiveA),
                          MakeDoubleChecker<double>())
            .AddAttribute("FengBeta",
                          "Increment parameter for m_curMaxP in Feng's Adaptive RED",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&MyQueueDisc::SetFengAdaptiveB),
                          MakeDoubleChecker<double>())
            .AddAttribute("LastSet",
                          "Store the last time m_curMaxP was updated",
                          TimeValue(Seconds(0.0)),
                          MakeTimeAccessor(&MyQueueDisc::m_lastSet),
                          MakeTimeChecker())
            .AddAttribute("Rtt",
                          "Round Trip Time to be considered while automatically setting m_bottom",
                          TimeValue(Seconds(0.1)),
                          MakeTimeAccessor(&MyQueueDisc::m_rtt),
                          MakeTimeChecker())
            .AddAttribute("Ns1Compat",
                          "NS-1 compatibility",
                          BooleanValue(false),
                          MakeBooleanAccessor(&MyQueueDisc::m_isNs1Compat),
                          MakeBooleanChecker())
            .AddAttribute("LinkBandwidth",
                          "The RED link bandwidth",
                          DataRateValue(DataRate("1.5Mbps")),
                          MakeDataRateAccessor(&MyQueueDisc::m_linkBandwidth),
                          MakeDataRateChecker())
            .AddAttribute("LinkDelay",
                          "The RED link delay",
                          TimeValue(MilliSeconds(20)),
                          MakeTimeAccessor(&MyQueueDisc::m_linkDelay),
                          MakeTimeChecker())
            .AddAttribute("UseEcn",
                          "True to use ECN (packets are marked instead of being dropped)",
                          BooleanValue(false),
                          MakeBooleanAccessor(&MyQueueDisc::m_useEcn),
                          MakeBooleanChecker())
            .AddAttribute("UseHardDrop",
                          "True to always drop packets above max threshold",
                          BooleanValue(true),
                          MakeBooleanAccessor(&MyQueueDisc::m_useHardDrop),
                          MakeBooleanChecker());

    return tid;
}

MyQueueDisc::MyQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
    NS_LOG_FUNCTION(this);
    m_uv = CreateObject<UniformRandomVariable>();
}

MyQueueDisc::~MyQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
MyQueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_uv = nullptr;
    QueueDisc::DoDispose();
}

void
MyQueueDisc::SetAredAlpha(double alpha)
{
    NS_LOG_FUNCTION(this << alpha);
    m_alpha = alpha;

    if (m_alpha > 0.01)
    {
        NS_LOG_WARN("Alpha value is above the recommended bound!");
    }
}

double
MyQueueDisc::GetAredAlpha()
{
    NS_LOG_FUNCTION(this);
    return m_alpha;
}

void
MyQueueDisc::SetAredBeta(double beta)
{
    NS_LOG_FUNCTION(this << beta);
    m_beta = beta;

    if (m_beta < 0.83)
    {
        NS_LOG_WARN("Beta value is below the recommended bound!");
    }
}

double
MyQueueDisc::GetAredBeta()
{
    NS_LOG_FUNCTION(this);
    return m_beta;
}

void
MyQueueDisc::SetFengAdaptiveA(double a)
{
    NS_LOG_FUNCTION(this << a);
    m_a = a;

    if (m_a != 3)
    {
        NS_LOG_WARN("Alpha value does not follow the recommendations!");
    }
}


QueueCounter
MyQueueDisc::GetQueueCounter()
{
    NS_LOG_FUNCTION(this);
    return m_queueCounter;
}


double
MyQueueDisc::GetFengAdaptiveA()
{
    NS_LOG_FUNCTION(this);
    return m_a;
}

void
MyQueueDisc::SetFengAdaptiveB(double b)
{
    NS_LOG_FUNCTION(this << b);
    m_b = b;

    if (m_b != 2)
    {
        NS_LOG_WARN("Beta value does not follow the recommendations!");
    }
}

double
MyQueueDisc::GetFengAdaptiveB()
{
    NS_LOG_FUNCTION(this);
    return m_b;
}

void
MyQueueDisc::SetTh(double minTh, double maxTh)
{
    NS_LOG_FUNCTION(this << minTh << maxTh);
    NS_ASSERT(minTh <= maxTh);
    m_minTh = minTh;
    m_maxTh = maxTh;
}

int64_t
MyQueueDisc::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_uv->SetStream(stream);
    return 1;
}

bool
MyQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    auto ipv4QItem = DynamicCast<Ipv4QueueDiscItem, QueueDiscItem>(item);
    auto ipIn = ipv4QItem->GetHeader().GetSource();
    uint32_t nQueued = GetNPackets();

    if (m_queueCounter.IncrementAndMark(ipIn.Get(), nQueued, m_maxTh) && nQueued >= m_maxTh )  {
        Mark(item, "Bigger than max threshold, my queue");
    }

    bool retval1 = GetInternalQueue(0)->Enqueue(item);

    // If Queue::Dequeue fails, QueueDisc::DropBeforeEnqueue is called by the
    // internal queue because QueueDisc::AddInternalQueue sets the trace callback

    NS_LOG_LOGIC("Number packets " << GetInternalQueue(0)->GetNPackets());
    NS_LOG_LOGIC("Number bytes " << GetInternalQueue(0)->GetNBytes());

    return retval1;
    // simulate number of packets arrival during idle period
    uint32_t m = 0;

    if (m_idle == 1)
    {
        NS_LOG_DEBUG("RED Queue Disc is idle.");
        Time now = Simulator::Now();

        if (m_cautious == 3)
        {
            double ptc = m_ptc * m_meanPktSize / m_idlePktSize;
            m = uint32_t(ptc * (now - m_idleTime).GetSeconds());
        }
        else
        {
            m = uint32_t(m_ptc * (now - m_idleTime).GetSeconds());
        }

        m_idle = 0;
    }

    m_qAvg = Estimator(nQueued, m + 1, m_qAvg, m_qW);

    NS_LOG_DEBUG("\t bytesInQueue  " << GetInternalQueue(0)->GetNBytes() << "\tQavg " << m_qAvg);
    NS_LOG_DEBUG("\t packetsInQueue  " << GetInternalQueue(0)->GetNPackets() << "\tQavg "
                                       << m_qAvg);

    m_count++;
    m_countBytes += item->GetSize();

    uint32_t dropType = DTYPE_NONE;
    if (m_qAvg >= m_minTh && nQueued > 1)
    {
        if ((!m_isGentle && m_qAvg >= m_maxTh) || (m_isGentle && m_qAvg >= 2 * m_maxTh))
        {
            NS_LOG_DEBUG("adding DROP FORCED MARK");
            dropType = DTYPE_FORCED;
        }
        else if (m_old == 0)
        {
            /*
             * The average queue size has just crossed the
             * threshold from below to above m_minTh, or
             * from above m_minTh with an empty queue to
             * above m_minTh with a nonempty queue.
             */
            m_count = 1;
            m_countBytes = item->GetSize();
            m_old = 1;
        }
        else if (DropEarly(item, nQueued))
        {
            NS_LOG_LOGIC("DropEarly returns 1");
            dropType = DTYPE_UNFORCED;
        }
    }
    else
    {
        // No packets are being dropped
        m_vProb = 0.0;
        m_old = 0;
    }

    if (dropType == DTYPE_UNFORCED)
    {
        if (!m_useEcn || !Mark(item, UNFORCED_MARK))
        {
            NS_LOG_DEBUG("\t Dropping due to Prob Mark " << m_qAvg);
            DropBeforeEnqueue(item, UNFORCED_DROP);
            return false;
        }
        NS_LOG_DEBUG("\t Marking due to Prob Mark " << m_qAvg);
    }
    else if (dropType == DTYPE_FORCED)
    {
        if (m_useHardDrop || !m_useEcn || !Mark(item, FORCED_MARK))
        {
            NS_LOG_DEBUG("\t Dropping due to Hard Mark " << m_qAvg);
            DropBeforeEnqueue(item, FORCED_DROP);
            if (m_isNs1Compat)
            {
                m_count = 0;
                m_countBytes = 0;
            }
            return false;
        }
        NS_LOG_DEBUG("\t Marking due to Hard Mark " << m_qAvg);
    }

    bool retval = GetInternalQueue(0)->Enqueue(item);

    // If Queue::Dequeue fails, QueueDisc::DropBeforeEnqueue is called by the
    // internal queue because QueueDisc::AddInternalQueue sets the trace callback

    NS_LOG_LOGIC("Number packets " << GetInternalQueue(0)->GetNPackets());
    NS_LOG_LOGIC("Number bytes " << GetInternalQueue(0)->GetNBytes());

    return retval;
}

/*
 * Note: if the link bandwidth changes in the course of the
 * simulation, the bandwidth-dependent RED parameters do not change.
 * This should be fixed, but it would require some extra parameters,
 * and didn't seem worth the trouble...
 */
void
MyQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("Initializing RED params.");

    m_queueCounter.SetTotal(m_queueCounterTotal);
    m_cautious = 0;
    m_ptc = m_linkBandwidth.GetBitRate() / (8.0 * m_meanPktSize);

    if (m_isARED)
    {
        // Set m_minTh, m_maxTh and m_qW to zero for automatic setting
        m_minTh = 0;
        m_maxTh = 0;
        m_qW = 0;

        // Turn on m_isAdaptMaxP to adapt m_curMaxP
        m_isAdaptMaxP = true;
    }

    if (m_isFengAdaptive)
    {
        // Initialize m_fengStatus
        m_fengStatus = Above;
    }

    if (m_minTh == 0 && m_maxTh == 0)
    {
        m_minTh = 5.0;

        // set m_minTh to max(m_minTh, targetqueue/2.0) [Ref:
        // http://www.icir.org/floyd/papers/adaptiveRed.pdf]
        double targetqueue = m_targetDelay.GetSeconds() * m_ptc;

        if (m_minTh < targetqueue / 2.0)
        {
            m_minTh = targetqueue / 2.0;
        }
        if (GetMaxSize().GetUnit() == QueueSizeUnit::BYTES)
        {
            m_minTh = m_minTh * m_meanPktSize;
        }

        // set m_maxTh to three times m_minTh [Ref:
        // http://www.icir.org/floyd/papers/adaptiveRed.pdf]
        m_maxTh = 3 * m_minTh;
    }

    NS_ASSERT(m_minTh <= m_maxTh);

    m_qAvg = 0.0;
    m_count = 0;
    m_countBytes = 0;
    m_old = 0;
    m_idle = 1;

    double th_diff = (m_maxTh - m_minTh);
    if (th_diff == 0)
    {
        th_diff = 1.0;
    }
    m_vA = 1.0 / th_diff;
    m_curMaxP = 1.0 / m_lInterm;
    m_vB = -m_minTh / th_diff;

    if (m_isGentle)
    {
        m_vC = (1.0 - m_curMaxP) / m_maxTh;
        m_vD = 2.0 * m_curMaxP - 1.0;
    }
    m_idleTime = NanoSeconds(0);

    /*
     * If m_qW=0, set it to a reasonable value of 1-exp(-1/C)
     * This corresponds to choosing m_qW to be of that value for
     * which the packet time constant -1/ln(1-m)qW) per default RTT
     * of 100ms is an order of magnitude more than the link capacity, C.
     *
     * If m_qW=-1, then the queue weight is set to be a function of
     * the bandwidth and the link propagation delay.  In particular,
     * the default RTT is assumed to be three times the link delay and
     * transmission delay, if this gives a default RTT greater than 100 ms.
     *
     * If m_qW=-2, set it to a reasonable value of 1-exp(-10/C).
     */
    if (m_qW == 0.0)
    {
        m_qW = 1.0 - std::exp(-1.0 / m_ptc);
    }
    else if (m_qW == -1.0)
    {
        double rtt = 3.0 * (m_linkDelay.GetSeconds() + 1.0 / m_ptc);

        if (rtt < 0.1)
        {
            rtt = 0.1;
        }
        m_qW = 1.0 - std::exp(-1.0 / (10 * rtt * m_ptc));
    }
    else if (m_qW == -2.0)
    {
        m_qW = 1.0 - std::exp(-10.0 / m_ptc);
    }

    if (m_bottom == 0)
    {
        m_bottom = 0.01;
        // Set bottom to at most 1/W, where W is the delay-bandwidth
        // product in packets for a connection.
        // So W = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize * m_rtt.GetSeconds())
        double bottom1 = (8.0 * m_meanPktSize * m_rtt.GetSeconds()) / m_linkBandwidth.GetBitRate();
        if (bottom1 < m_bottom)
        {
            m_bottom = bottom1;
        }
    }

    NS_LOG_DEBUG("\tm_delay " << m_linkDelay.GetSeconds() << "; m_isWait " << m_isWait << "; m_qW "
                              << m_qW << "; m_ptc " << m_ptc << "; m_minTh " << m_minTh
                              << "; m_maxTh " << m_maxTh << "; m_isGentle " << m_isGentle
                              << "; th_diff " << th_diff << "; lInterm " << m_lInterm << "; va "
                              << m_vA << "; cur_max_p " << m_curMaxP << "; v_b " << m_vB
                              << "; m_vC " << m_vC << "; m_vD " << m_vD);
}

// Updating m_curMaxP, following the pseudocode
// from: A Self-Configuring RED Gateway, INFOCOMM '99.
// They recommend m_a = 3, and m_b = 2.
void
MyQueueDisc::UpdateMaxPFeng(double newAve)
{
    NS_LOG_FUNCTION(this << newAve);

    if (m_minTh < newAve && newAve < m_maxTh)
    {
        m_fengStatus = Between;
    }
    else if (newAve < m_minTh && m_fengStatus != Below)
    {
        m_fengStatus = Below;
        m_curMaxP = m_curMaxP / m_a;
    }
    else if (newAve > m_maxTh && m_fengStatus != Above)
    {
        m_fengStatus = Above;
        m_curMaxP = m_curMaxP * m_b;
    }
}

// Update m_curMaxP to keep the average queue length within the target range.
void
MyQueueDisc::UpdateMaxP(double newAve)
{
    NS_LOG_FUNCTION(this << newAve);

    Time now = Simulator::Now();
    double m_part = 0.4 * (m_maxTh - m_minTh);
    // AIMD rule to keep target Q~1/2(m_minTh + m_maxTh)
    if (newAve < m_minTh + m_part && m_curMaxP > m_bottom)
    {
        // we should increase the average queue size, so decrease m_curMaxP
        m_curMaxP = m_curMaxP * m_beta;
        m_lastSet = now;
    }
    else if (newAve > m_maxTh - m_part && m_top > m_curMaxP)
    {
        // we should decrease the average queue size, so increase m_curMaxP
        double alpha = m_alpha;
        if (alpha > 0.25 * m_curMaxP)
        {
            alpha = 0.25 * m_curMaxP;
        }
        m_curMaxP = m_curMaxP + alpha;
        m_lastSet = now;
    }
}

// Compute the average queue size
double
MyQueueDisc::Estimator(uint32_t nQueued, uint32_t m, double qAvg, double qW)
{
    NS_LOG_FUNCTION(this << nQueued << m << qAvg << qW);

    double newAve = qAvg * std::pow(1.0 - qW, m);
    newAve += qW * nQueued;

    Time now = Simulator::Now();
    if (m_isAdaptMaxP && now > m_lastSet + m_interval)
    {
        UpdateMaxP(newAve);
    }
    else if (m_isFengAdaptive)
    {
        UpdateMaxPFeng(newAve); // Update m_curMaxP in MIMD fashion.
    }

    return newAve;
}

// Check if packet p needs to be dropped due to probability mark
bool
MyQueueDisc::DropEarly(Ptr<QueueDiscItem> item, uint32_t qSize)
{
    NS_LOG_FUNCTION(this << item << qSize);

    double prob1 = CalculatePNew();
    m_vProb = ModifyP(prob1, item->GetSize());

    // Drop probability is computed, pick random number and act
    if (m_cautious == 1)
    {
        /*
         * Don't drop/mark if the instantaneous queue is much below the average.
         * For experimental purposes only.
         * pkts: the number of packets arriving in 50 ms
         */
        double pkts = m_ptc * 0.05;
        double fraction = std::pow((1 - m_qW), pkts);

        if ((double)qSize < fraction * m_qAvg)
        {
            // Queue could have been empty for 0.05 seconds
            return false;
        }
    }

    double u = m_uv->GetValue();

    if (m_cautious == 2)
    {
        /*
         * Decrease the drop probability if the instantaneous
         * queue is much below the average.
         * For experimental purposes only.
         * pkts: the number of packets arriving in 50 ms
         */
        double pkts = m_ptc * 0.05;
        double fraction = std::pow((1 - m_qW), pkts);
        double ratio = qSize / (fraction * m_qAvg);

        if (ratio < 1.0)
        {
            u *= 1.0 / ratio;
        }
    }

    if (u <= m_vProb)
    {
        NS_LOG_LOGIC("u <= m_vProb; u " << u << "; m_vProb " << m_vProb);

        // DROP or MARK
        m_count = 0;
        m_countBytes = 0;
        /// \todo Implement set bit to mark

        return true; // drop
    }

    return false; // no drop/mark
}

// Returns a probability using these function parameters for the DropEarly function
double
MyQueueDisc::CalculatePNew()
{
    NS_LOG_FUNCTION(this);
    double p;

    if (m_isGentle && m_qAvg >= m_maxTh)
    {
        // p ranges from m_curMaxP to 1 as the average queue
        // size ranges from m_maxTh to twice m_maxTh
        p = m_vC * m_qAvg + m_vD;
    }
    else if (!m_isGentle && m_qAvg >= m_maxTh)
    {
        /*
         * OLD: p continues to range linearly above m_curMaxP as
         * the average queue size ranges above m_maxTh.
         * NEW: p is set to 1.0
         */
        p = 1.0;
    }
    else
    {
        /*
         * p ranges from 0 to m_curMaxP as the average queue size ranges from
         * m_minTh to m_maxTh
         */
        p = m_vA * m_qAvg + m_vB;

        if (m_isNonlinear)
        {
            p *= p * 1.5;
        }

        p *= m_curMaxP;
    }

    if (p > 1.0)
    {
        p = 1.0;
    }

    return p;
}

// Returns a probability using these function parameters for the DropEarly function
double
MyQueueDisc::ModifyP(double p, uint32_t size)
{
    NS_LOG_FUNCTION(this << p << size);
    auto count1 = (double)m_count;

    if (GetMaxSize().GetUnit() == QueueSizeUnit::BYTES)
    {
        count1 = (double)(m_countBytes / m_meanPktSize);
    }

    if (m_isWait)
    {
        if (count1 * p < 1.0)
        {
            p = 0.0;
        }
        else if (count1 * p < 2.0)
        {
            p /= (2.0 - count1 * p);
        }
        else
        {
            p = 1.0;
        }
    }
    else
    {
        if (count1 * p < 1.0)
        {
            p /= (1.0 - count1 * p);
        }
        else
        {
            p = 1.0;
        }
    }

    if ((GetMaxSize().GetUnit() == QueueSizeUnit::BYTES) && (p < 1.0))
    {
        p = (p * size) / m_meanPktSize;
    }

    if (p > 1.0)
    {
        p = 1.0;
    }

    return p;
}

Ptr<QueueDiscItem>
MyQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    if (GetInternalQueue(0)->IsEmpty())
    {
        NS_LOG_LOGIC("Queue empty");
        m_idle = 1;
        m_idleTime = Simulator::Now();

        return nullptr;
    }
    else
    {
        m_idle = 0;
        Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
        auto ipOut = DynamicCast<Ipv4QueueDiscItem, QueueDiscItem>(item)
                ->GetHeader().GetSource().Get();
        m_queueCounter.Dequeue(ipOut);
        NS_LOG_LOGIC("Popped " << item);

        NS_LOG_LOGIC("Number packets " << GetInternalQueue(0)->GetNPackets());
        NS_LOG_LOGIC("Number bytes " << GetInternalQueue(0)->GetNBytes());

        return item;
    }
}

Ptr<const QueueDiscItem>
MyQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);
    if (GetInternalQueue(0)->IsEmpty())
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    Ptr<const QueueDiscItem> item = GetInternalQueue(0)->Peek();

    NS_LOG_LOGIC("Number packets " << GetInternalQueue(0)->GetNPackets());
    NS_LOG_LOGIC("Number bytes " << GetInternalQueue(0)->GetNBytes());

    return item;
}

bool
MyQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNQueueDiscClasses() > 0)
    {
        NS_LOG_ERROR("MyQueueDisc cannot have classes");
        return false;
    }

    if (GetNPacketFilters() > 0)
    {
        NS_LOG_ERROR("MyQueueDisc cannot have packet filters");
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // add a DropTail queue
        AddInternalQueue(
            CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>("MaxSize",
                                                                     QueueSizeValue(GetMaxSize())));
    }

    if (GetNInternalQueues() != 1)
    {
        NS_LOG_ERROR("MyQueueDisc needs 1 internal queue");
        return false;
    }

    if ((m_isARED || m_isAdaptMaxP) && m_isFengAdaptive)
    {
        NS_LOG_ERROR("m_isAdaptMaxP and m_isFengAdaptive cannot be simultaneously true");
    }

    return true;
}

} // namespace ns3