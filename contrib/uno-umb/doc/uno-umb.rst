UNO-UMB Module
==============

.. include:: replace.txt
.. highlight:: cpp

The UNO-UMB module contains experiments for QoS-aware cell sleep control
in cellular RAN energy-saving studies.  The current implementation focuses on
a lightweight digital-twin controller that predicts whether low-load cells can
sleep without violating utilization and coverage safety margins.

The source code lives in ``contrib/uno-umb``.

Scope and Limitations
---------------------

The module currently provides:

* A reusable ``CellSleepController`` model.
* Five policy modes: all-on, threshold, aggressive, twin, and adaptive-twin.
* An LTE/EPC example that writes summary and controller-event CSV files.
* Distribution-shift traffic profiles for steady, center-cell burst, edge-cell
  burst, right-edge burst, and global burst demand.
* Python utilities for parameter sweeps, manifests, aggregate summaries, and a
  dependency-free energy-risk SVG figure.

The current controller uses an analytical energy model and a simple RSRP proxy.
It is intended as a reproducible research scaffold, not as a calibrated base
station energy model or a complete RAN digital twin.

Design
------

The controller periodically estimates per-cell load from the current UE serving
map and per-UE offered-load estimates.  Low-load cells are candidate sleep cells.
Threshold policies apply the sleep decision directly.  The twin policy first
estimates the post-sleep maximum active-cell utilization and minimum coverage
margin, adds uncertainty margins, and sleeps a cell only when both safety checks
pass.

The adaptive-twin policy uses the same risk gate but updates the uncertainty
multiplier online.  The multiplier increases when the controller observes a
large interval-to-interval load shock or high active-cell utilization, and then
relaxes toward the configured base value after stable intervals.  This creates a
testable tradeoff between static conservatism and shift-responsive risk
calibration.

The adaptive-twin policy also tracks latent preferred-cell demand.  When a cell
is already sleeping, its serving load can be zero even though its home UEs are
creating load on neighboring active cells.  The controller therefore estimates
the counterfactual peak-utilization relief from waking that sleeping cell and
keeps it active when the latent demand and relief exceed configured thresholds.

Sleeping a cell is modeled by requesting X2 handovers for served UEs and then
reducing the eNB transmit power.  Energy is accounted analytically from active
and sleep power constants.

Usage
-----

The main example is ``contrib/uno-umb/examples/uno-umb-dt-energy.cc``.

Run one policy:

.. code-block:: console

   ./ns3 run "uno-umb-dt-energy --policy=twin"
   ./ns3 run "uno-umb-dt-energy --policy=twin --trafficProfile=center-burst \
     --burstRateMultiplier=3.0"

Run the policy pilot sweep:

.. code-block:: console

   python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py

Run a static-versus-adaptive calibration sweep:

.. code-block:: console

   python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py \
     --policies=twin,adaptive-twin \
     --traffic-profiles=center-burst \
     --uncertainty-scales=0.5,0.7,1.0

Summarize aggregate sweep output:

.. code-block:: console

   python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py \
     results/uno-umb-dt-energy-*/aggregate.csv

The analyzer writes policy and scenario summaries, pairwise comparisons against
the twin policy, and an ``energy-risk.svg`` scatter plot.  It also reports a
safe energy-saving metric that counts energy saving only for runs with no SLA
violation and no unsafe sleep action.

When all-on reference rows are included, the analyzer also writes
``feasibility-comparison.csv``.  This output separates controller-induced SLA
violations from workloads that violate the SLA even with all cells active.  It
also writes ``feasible-policy-summary.csv`` and ``feasible-policy-summary.md``
for policy summaries restricted to workloads that are feasible under all-on.
For burst-profile sweeps, it additionally writes
``feasibility-envelope-summary.csv`` and
``feasibility-envelope-summary.md``.  These files group results by traffic
profile and burst multiplier so that overload regimes are separated from
controller-induced SLA violations.

Helpers
~~~~~~~

Not applicable.

Attributes
~~~~~~~~~~

The module does not currently expose ns-3 attributes.  Example and controller
parameters are set through command-line arguments and ``CellSleepControllerConfig``.

Traces
~~~~~~

The controller writes a CSV event log with one row per decision interval and
cell.  The event log includes the policy, action, load estimate, utilization
estimate, coverage margin estimate, uncertainty margins, and the final twin
safety decision.  For adaptive experiments, it also records the effective
uncertainty scale, the normalized load shock, and the observed maximum
utilization for each interval.
The event log also includes latent preferred-cell load and the estimated peak
utilization relief from waking a sleeping cell.

Examples and Tests
------------------

``uno-umb-dt-energy`` compares all-on, threshold, aggressive, twin, and
adaptive-twin
cell-sleep policies in an LTE/EPC scenario and writes CSV summaries.  The
example can also add a temporary demand burst to a selected subset of UEs so
that policies are compared under distribution shift.

The ``uno-umb`` unit test validates policy parsing and the controller event CSV
header.

Validation
----------

The implementation is validated by the module unit test, build checks, and
targeted smoke runs of the LTE/EPC example.  Formal validation against measured
base-station energy traces has not yet been performed.

References
----------

No external references are required for the current software scaffold.
