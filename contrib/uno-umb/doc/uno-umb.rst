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
* Four policy modes: all-on, threshold, aggressive, and twin.
* An LTE/EPC example that writes summary and controller-event CSV files.
* Distribution-shift traffic profiles for steady, center-cell burst, edge-cell
  burst, right-edge burst, and global burst demand.
* Python utilities for parameter sweeps and aggregate-result summaries.

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

Sleeping a cell is modeled by requesting X2 handovers for served UEs and then
reducing the eNB transmit power.  Energy is accounted analytically from active
and sleep power constants.

Usage
-----

The main example is ``contrib/uno-umb/examples/uno-umb-dt-energy.cc``.

Run one policy:

.. code-block:: console

   ./ns3 run "uno-umb-dt-energy --policy=twin"
   ./ns3 run "uno-umb-dt-energy --policy=twin --trafficProfile=center-burst --burstRateMultiplier=3.0"

Run the four-policy pilot sweep:

.. code-block:: console

   python3 contrib/uno-umb/utils/uno-umb-dt-energy-sweep.py

Summarize aggregate sweep output:

.. code-block:: console

   python3 contrib/uno-umb/utils/uno-umb-dt-energy-analyze.py results/uno-umb-dt-energy-*/aggregate.csv

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
safety decision.

Examples and Tests
------------------

``uno-umb-dt-energy`` compares all-on, threshold, aggressive, and twin
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
