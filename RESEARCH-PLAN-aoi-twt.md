# Research Plan: AoI-Optimal TWT Scheduling (branch `aoi-twt`)

Status: initial scoping done 2026-06-11. Baseline: ns-3.48 (d2add90b4).
Companion notes: `../RESEARCH-EXPLORATION.md` (venue landscape, sibling projects).

## 1. Novelty check — what already exists (verified 2026-06-11)

The naive pitch ("optimize AoI with TWT") is partially taken. Direct competition:

1. **TASPER** — "Target Wake Time Scheduling for Time-sensitive and Energy-efficient
   Wi-Fi Networks" (arXiv:2509.26245, Sep 2025). Formulates TWT Acceptance and
   Scheduling Problem: maximize throughput + energy efficiency **with AoI as a
   constraint**. Proves NP-hardness, gives a **heuristic with no optimality
   guarantees**. Evaluated on a custom ns-3-based TWT simulator + 10-station
   commercial testbed.
2. "Deterministic Scheduling over Wi-Fi 6 using TWT: An Experimental Approach"
   (arXiv:2505.00447, May 2025). TWT scheduling for IIoT respecting AoI, experimental,
   deterministic traffic assumptions.

Classic AoI scheduling theory (Kadota et al., ToN 2018 etc.) gives Whittle-index
policies for per-slot centralized scheduling — but that control model does NOT match
TWT, where the AP commits to *periodic wake schedules* at agreement timescale and
stations are unreachable (asleep) between service periods.

## 2. Refined contribution (the defensible gap)

**AoI as the objective, periodicity as the structural constraint, with provable
guarantees.** Nobody has the theory piece:

- Formulate: minimize weighted average (and/or peak) AoI across stations subject to
  per-station energy budgets, where the decision variables are TWT agreement
  parameters (wake interval, offset, SP duration) — i.e., a *periodic schedule design*
  problem, not per-slot scheduling. Stochastic arrivals and channel errors
  (retransmission inflates AoI) included — both 2025 papers assume deterministic
  traffic.
- Two-timescale structure: outer problem chooses periodic schedules (relates to
  pinwheel/cyclic scheduling); inner problem allocates transmissions within an SP.
- Target guarantees: indexability / Whittle-like index for the relaxed problem,
  asymptotic optimality in the many-station regime, or an approximation ratio for the
  periodic-schedule packing problem. Any one of these beats TASPER's
  guarantee-free heuristic and is the MobiHoc/INFOCOM-shaped contribution.
- Position TASPER's heuristic and simple policies (round-robin TWT, equal-interval)
  as baselines.

### Deep related-work pass (done 2026-06-11) — gap CONFIRMED, boundaries mapped

Three closest works, none of which occupies our slot:

| Work | Model | What it does | Why we differ |
|---|---|---|---|
| TASPER (arXiv:2509.26245, 2025) | TWT, IIoT | AoI as *constraint*; max throughput/energy; NP-hard + heuristic | We: AoI as *objective*, stochastic arrivals/errors, provable guarantees |
| Bedewy, Sun, Singh, Shroff (MobiHoc'20, arXiv:1910.00205) | CSMA random access | Min weighted-sum *peak* AoI via continuous sleep-wake *rates*, battery constraints; near-optimal for small sensing times | We: negotiated *deterministic periodic* schedules (period/offset/SP triples), AP-coordinated, combinatorial structure |
| Age-agnostic cyclic scheduling (arXiv:2311.18791) | Centralized per-slot server | Optimal cyclic patterns for weighted AoI; closed form for 2 sources; no energy | We: energy budgets, sleep windows, per-source periodic wake constraints, contention within SPs |
| Bedewy et al. follow-up (arXiv:2102.02846) | Sleep-wake, low power | Age-energy trade-off | Same differentiator as MobiHoc'20 (no periodic agreement structure) |

**Our slot, sharpened:** minimize weighted average (and peak) AoI where the decision
variables are per-station TWT triples (wake interval T_i, offset phi_i, SP duration
d_i) — a *harmonic periodic packing* problem (pinwheel-like) — under per-station
energy budgets, stochastic arrivals, and channel errors, with optimality/approximation
guarantees. Cite all four as the boundary; TASPER + equal-interval + round-robin are
the evaluation baselines.

## 3. ns-3 implementation mapping

Mainline ns-3.48 has **no functional TWT** — only capability/operation information
elements:

- `src/wifi/model/he/he-capabilities.{h,cc}` — TWT Requester/Responder support bits
- `src/wifi/model/he/he-operation.{h,cc}` — TWT Required field
- `src/wifi/model/eht/eht-capabilities.{h,cc}`, `eht-frame-exchange-manager.cc` —
  passing mentions

Existing external implementation: **gtgnan/wifiTwt** (GitHub, Georgia Tech GNAN lab,
GPL-2.0). Cloned to `../refs/wifiTwt` and inspected (2026-06-11):

- Fork base: ns-3-dev just after ns-3.40 (~early 2024) — 8 releases behind us.
- Footprint is small: new `WifiTwtAgreement` class (507 lines,
  `src/wifi/model/wifi-twt-agreement.{h,cc}`) + scattered hooks in `wifi-mac`,
  `sta-wifi-mac`, `wifi-remote-station-manager` (PS buffering, ~49 mentions),
  `he-frame-exchange-manager`. Sleep cycling done via raw
  `phy->SetSleepMode()/ResumeFromSleep()` calls in sta-wifi-mac.
- API: programmatic `WifiMac::SetTwtSchedule(flowId, peer, ..., wakeInterval,
  nominalWakeDuration, nextTwt)` — agreements installed "god-mode", no management
  frame negotiation. Acceptable for scheduling research; negotiation can come later.
- Missing: teardown, broadcast/announced/trigger-based TWT, MLD support.

**Decision: clean re-implementation on ns-3.48, using wifiTwt as design reference,
not a mechanical port.** UPDATE (implemented): ns-3.48's `PowerSaveManager` is an
abstract base explicitly designed for pluggable doze/wake logic (added 2024 by
S. Avallone, UNINA — the ICNS3 2026 host institution). The STA-side TWT mechanism is
now `TwtPowerSaveManager` (`src/wifi/model/twt-power-save-manager.{h,cc}`), a
~300-line subclass implementing individual implicit unannounced non-trigger-based
TWT: periodic SP wake/doze cycle, and crucially NO wake on channel-access requests
outside an SP (uplink defers to the next SP; `Txop::NotifyWakeUp` automatically
restarts access at SP start when the PHY resumes). Compiles against unmodified wifi
internals — the only `src/wifi` diff is the new file pair plus two CMakeLists lines,
which is the best possible upstreaming position. Rationale: ns-3.48 now has a `PowerSaveManager` and a
WifiPowerManagementMode state machine (`sta-wifi-mac.h`: WIFI_PM_ACTIVE /
WIFI_PM_SWITCHING_TO_PS / WIFI_PM_POWERSAVE) that did not exist at the fork base —
TWT should integrate with that state machine instead of raw PHY sleep calls. This is
also the stronger upstreaming story (mainline has no TWT at all). Estimated effort
revised down: ~3-4 weeks for the minimal individual-TWT mechanism + scheduler hook.
A second WNS3 2024 TWT implementation exists (doi:10.1145/3659111.3659112) — skim
its paper for design lessons before starting.

Components we will touch:

- `src/wifi/model/sta-wifi-mac.{h,cc}`, `ap-wifi-mac.{h,cc}` — TWT agreement state,
  negotiation hooks
- `src/wifi/model/wifi-phy-state-helper.{h,cc}` + sleep scheduling — wake/sleep per SP
- `src/wifi/model/wifi-mgt-header` / action frames — TWT Setup/Teardown frames
- `src/energy` + `src/wifi/model/wifi-radio-energy-model` — energy accounting
- New: AoI tracker (app-layer tag: generation timestamp -> AoI process at AP),
  AP-side TWT scheduler interface (pluggable policy = our algorithm vs baselines)

## 3b. AP scheduler interface (design, 2026-06-11)

Pluggable policy object on the AP so our algorithm and all baselines share one
mechanism. Sketch (new files under `src/wifi/model/twt/`):

```cpp
/// One installed agreement (subset of 802.11ax 9.4.2.199 we need)
struct TwtAgreement {
    uint8_t flowId; Mac48Address peer;
    Time wakeInterval;   // T_i
    Time wakeDuration;   // d_i
    Time nextTwt;        // offset phi_i, anchored to beacon time
    bool implicit{true}; bool triggerBased{false};
};

/// Abstract AP-side policy: observes per-station state, emits schedules.
class TwtScheduler : public Object {
  public:
    /// Called on association, traffic-profile change, or periodic re-plan
    virtual std::vector<TwtAgreement> Recompute(
        const std::vector<TwtStationInfo>& stations,  // AoI weight, energy
                                                      // budget rho_i, est. p_i
        Time beaconInterval) = 0;
};
// Implementations: EqualIntervalTwtScheduler, RoundRobinTwtScheduler,
// TasperLikeScheduler (baseline), HarmonicIndexScheduler (ours)
```

Mechanism (separate from policy): STA-side wake/sleep driven by the ns-3.48
PowerSaveManager state machine; AP-side per-station PS buffering already exists in
`wifi-remote-station-manager` (used by legacy PS) — extend rather than duplicate.
AoI instrumentation: timestamp tag at app source + `AoiTracker` trace sink at AP.

## 4. Evaluation plan

- Scenarios: N = 5..100 stations, periodic + Poisson + bursty sources, uplink
  status-update traffic; error-prone channel (distance/MCS sweep).
- Policies: ours (index/approx algorithm) vs TASPER-style heuristic, equal-interval
  TWT, round-robin TWT, legacy PSM, always-on.
- Metrics: time-average AoI, peak AoI, per-station energy (J) and lifetime, schedule
  overhead; AoI-energy Pareto frontier as the headline figure.
- Validation of theory: simulated AoI vs analytical AoI for the single-station and
  symmetric-N cases.

## 5. Targets and timeline (deadlines to re-verify)

- **Primary: IEEE ICC 2027** (deadline typically ~Oct 2026) — realistic with theory +
  ns-3 evaluation done by fall. Per-symposium best paper is the award target.
- **Stretch: INFOCOM 2027** (deadline ~late Jul 2026, based on INFOCOM 2026's
  Jul 31, 2025 deadline) — only if the theory core lands within ~6 weeks; tight.
- **MobiHoc 2027** — next cycle if results are strong (MobiHoc 2026 deadline has
  likely passed; conference is Nov 23-26, 2026, Tokyo).
- **ICNS3 2027** — companion paper: the ported/extended TWT module itself
  (upstreamable; mainline has none — high community value).

Rough effort: port TWT module 4-6 wks; theory core 6-10 wks (parallel); scheduler +
evaluation 4-6 wks; writing 3 wks.

## 6. Immediate next steps

1. Deep related-work pass: AoI + sleep/wake scheduling, AoI + periodic (pinwheel)
   scheduling, Whittle index for AoI — confirm and sharpen the gap statement.
2. Clone gtgnan/wifiTwt and the alternative WNS3'24 implementation; diff against
   ns-3.48 wifi MAC; estimate port cost; pick base.
3. Write the single-station AoI/energy renewal-reward analysis (warm-up result; also
   the simulator-validation anchor).
4. Define the AP scheduler interface in ns-3 so policies are pluggable from day one.
