# Theory Notes: AoI under TWT Schedules (working draft)

Companion to `RESEARCH-PLAN-aoi-twt.md`. Math here is sketch-level; to be made
rigorous in the paper. Notation: ASCII only per project style.

## 1. Model

- N stations, one AP. Station i holds a TWT agreement (T_i, phi_i, d_i):
  wake interval (period) T_i, offset phi_i, SP duration d_i. Station sleeps outside
  its SPs; inside an SP it can transmit one (or more) status updates uplink.
- Sampling: generate-at-will — station samples its source at the moment it wakes
  (fresh sample at SP start). Extension later: random arrivals (sample available
  with prob. q per period, or Poisson sources with buffering).
- Channel: each in-SP transmission attempt succeeds i.i.d. with prob. p_i
  (captures MCS/distance; retransmissions within the SP allowed up to SP capacity
  k_i = floor(d_i / s_i), s_i = per-attempt airtime).
- Energy: per period, station spends e_a * d_i (awake) + e_s * (T_i - d_i) (sleep).
  Budget: average power <= P_i  <=>  d_i (e_a - e_s) / T_i <= P_i - e_s
  => duty-cycle constraint d_i / T_i <= rho_i where
  rho_i := (P_i - e_s) / (e_a - e_s).
- AoI of source i at AP: A_i(t), sawtooth; jumps down to (transmission delay) on
  successful delivery, grows at unit rate otherwise.

## 2. Single-station renewal-reward (warm-up; simulator validation anchor)

Successful periods form a renewal process. Let p be the prob. that at least one
attempt in the SP succeeds: p_succ = 1 - (1 - p)^k for k attempts of success prob p.
Number of periods between successful deliveries: M ~ Geometric(p_succ),
E[M] = 1/p_succ.

With generate-at-will sampling at SP start and delivery within the SP (delay <= d,
treat as delta := mean in-SP delivery delay), the age at a successful delivery
resets to approximately delta, then grows for M periods until the next success.

Time-average AoI (standard renewal-reward over cycles of length M*T):

    E[A] = delta + T * ( E[M^2] / (2 E[M]) + 1/2 ) - T/2 ... (to re-derive carefully)

For Geometric(p_succ): E[M] = 1/p_succ, E[M^2] = (2 - p_succ)/p_succ^2, giving

    E[A] ~= delta + T * (1/p_succ - 1/2)

Sanity checks: p_succ = 1 => E[A] ~= delta + T/2 (classic periodic sampling);
p_succ -> 0 => blows up like T/p_succ. Matches intuition.

Peak AoI: E[A_peak] ~= delta + T / p_succ.

### Energy-optimal single station

minimize_{T, d}  delta + T (1/p_succ(d) - 1/2)   s.t.  d/T <= rho.

For fixed d, objective increasing in T => constraint tight: T = d/rho. Then minimize
over d (equivalently over k = floor(d/s)):

    f(k) = (k s / rho) * ( 1/(1 - (1-p)^k) - 1/2 )

Discrete, unimodal in k (to prove; ratio test on f(k+1)/f(k)). Optimal k* balances
"longer SP costs proportionally longer period under the energy budget" against
"more retries per wake". For p -> 1, k* = 1: wake briefly and often. For small p,
k* > 1: batching retries beats more frequent short wakes (since each wake re-pays
the period cost). Closed-form threshold on p for k*=1 vs k*>1 is a nice lemma.

**LEMMA (proved 2026-06-11, discovered via failing unit test):** with zero per-wake
overhead, k* = 1 for EVERY p in (0,1]:

    f(k+1) - f(k) at k=1:  f(2) - f(1) = p / (2 (2 - p)) > 0
    (general k: g(k) = k/(1-q^k) is strictly increasing in k for q in (0,1),
     and the -k/2 correction never compensates — write out the full proof.)

So batching retries is NEVER optimal without overhead — my earlier guess ("small p
=> k* > 1") was wrong. Corollary: the per-wake fixed overhead c_w (radio rampup,
beacon sync; real and present in the 802.11 energy model) is exactly what makes the
problem TWT-specific vs. abstract sampling models. With c = c_w/s the objective is

    f(k) = (k + c) * (1 / (1 - (1-p)^k) - 1/2)

and k* > 1 appears for moderate c even at high p. KEEP c_w IN THE MODEL — it is
load-bearing for every batching result.

**ALGORITHMIC NOTE (from first implementation, contrib/aoi-twt):** harmonic
rounding-up of periods can nearly double a station's AoI (worst case factor 2 loss).
Fix implemented and validated: *post-rounding SP re-expansion* — rounding T up
leaves energy (duty <= rho) and density (d/T_ideal share) slack; spend it on extra
in-SP attempts, which at fixed T strictly increases pSucc at zero AoI cost. Cap
d' <= min(d * T'/T_ideal, rho * T'). In the 10-station example this turned a 22%
LOSS vs the equal-interval baseline into a 14% win (204.2 -> 175.4 ms weighted AoI).
The paper's approximation-factor proof should bake this step in — it tightens the
constant.

## 3. Multi-station structure (the real problem)

Decision: {(T_i, phi_i, d_i)}_{i=1..N} such that SPs do not overlap (or share with
bounded contention), minimizing sum_i w_i E[A_i] s.t. duty-cycle budgets.

Three nested difficulties:

1. **Period assignment**: choosing {T_i} — weighted AoI suggests T_i proportional to
   sqrt-type rules (cf. square-root laws in cyclic AoI scheduling, arXiv:2311.18791
   has the no-energy analog). With energy: T_i >= d_i/rho_i couples in.
2. **Offset packing (feasibility)**: given {(T_i, d_i)}, do offsets {phi_i} exist
   with disjoint SPs? This is exactly a pinwheel/periodic-maintenance feasibility
   problem — NP-hard in general; restrict to harmonic periods (T_i in {2^j * T_0})
   => feasibility is a simple density condition sum_i d_i/T_i <= 1 and packing is
   constructive (binary-tree/TDMA-tree assignment). Harmonic restriction costs at
   most a constant factor (<= 2) in AoI — prove via rounding argument. THIS is the
   approximation-guarantee skeleton: round optimal fractional periods to powers of
   two, pack greedily, bound the loss.
3. **Stochastics**: with random arrivals / failures, per-SP success couples stations
   only through the schedule; given the packing, stations decouple => the relaxed
   problem is separable. Lagrangian on the density constraint sum d_i/T_i <= 1 plays
   the role of the Whittle relaxation; "index" = marginal AoI reduction per unit
   density. Candidate result: index policy asymptotically optimal as N -> infinity
   with density held fixed.

Headline theorem targets (in descending ambition):
- T1: constant-factor approximation for weighted average AoI with energy budgets via
  harmonic rounding + greedy packing (deterministic arrivals, lossy channel).
- T2: density-Lagrangian index policy, asymptotic optimality (stochastic case).
- T3: exact solution for N=2 (mirrors the 2-source closed form of 2311.18791 but
  with energy + SP overhead) — good warm-up paper section.

## 4. What to validate in ns-3 first

1. Single-station E[A] formula vs simulation across (T, d, p) grid — also validates
   the TWT implementation itself (Section 2 formula is the test oracle).
   STATUS (2026-06-11): DONE for p ~= 1 via scratch/twt-aoi-validation.cc with the
   new src/wifi TwtPowerSaveManager. Error 0.016% (T=100ms,d=8ms) and 0.065%
   (T=50ms,d=4ms); measured duty cycle 0.082 vs nominal 0.080 (delta = SP overrun +
   pre-first-SP awake, as expected). p < 1 cases need a fading/error model (YANS
   default has a hard SNR cliff: p=1 at 30m, p=0 at 45m for HeMcs3) — use Nakagami
   or a controlled error model next.
2. Per-wake overhead c_w measured from the ns-3 energy model (StateHelper traces)
   — feeds back into the model as a calibrated constant.
3. Two-station disjoint-SP schedule: AoI additivity check (no coupling when SPs
   disjoint) — validates the decoupling assumption behind T2.

## 5. Open questions / risks

- Does 802.11 beacon-anchored timing quantize offsets (nextTwt is defined relative
  to beacon in the wifiTwt API)? If offsets are beacon-quantized, packing granularity
  changes — check 802.11ax spec 26.8.2 / implementation.
- Clock drift between AP and STA shifts SPs (cf. arXiv:2008.02697) — ignore in
  theory, note as robustness experiment.
- Downlink AoI (AP -> stations) is a different problem (broadcast TWT) — out of
  scope for paper 1; mention as future work.
