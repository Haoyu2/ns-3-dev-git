# Paper: Age-Optimal Target Wake Time

LaTeX sources and reproduction scripts for the AoI-TWT paper.

## Build the paper

    make            # conference version -> main.pdf
    make techreport # with the full-proofs appendix -> techreport.pdf

`main.tex` reads `results_macros.tex` (generated from experiment data) if
present, and falls back to recorded point estimates otherwise. Full proofs
live in `appendix.tex` (included by the techreport target).

## Reproduce the experiments

All simulation code lives in this repository (ns-3.48 base):

- mechanism: `src/wifi/model/twt-power-save-manager.{h,cc}`
- schedulers + AoI model: `contrib/aoi-twt/model/`
- single-station validation: `scratch/twt-aoi-validation.cc`
- multi-station experiment: `scratch/twt-aoi-multista.cc`

Steps (a 32-core machine runs the full sweep in ~30 min):

    ./ns3 configure --enable-modules=aoi-twt,wifi,internet,applications \
                    --enable-tests --enable-examples
    ./ns3 build
    python3 contrib/aoi-twt/paper/scripts/run_sweep.py 28   # -> results.csv
    python3 contrib/aoi-twt/paper/scripts/make_macros.py results.csv \
            contrib/aoi-twt/paper/results_macros.tex
    python3 contrib/aoi-twt/paper/scripts/make_pareto.py results.csv \
            contrib/aoi-twt/paper/figures/pareto.pdf

Unit tests (closed forms, schedule feasibility incl. the analyzed
dyadic-reservation variant):

    ./test.py -s aoi-twt

## Contents

    main.tex            paper source (IEEEtran conference)
    appendix.tex        full proofs (techreport build)
    refs.bib            bibliography
    scripts/run_sweep.py     200-run seed + Pareto sweep driver
    scripts/fig_data_sweep.py validation grid + scaling data
    scripts/make_macros.py   results.csv -> results_macros.tex (means + 95% CI)
    scripts/make_pareto.py   results.csv -> figures/pareto.pdf
    scripts/make_figs.py     validation/regimes/scaling figures
    figures/            generated figures (+ TikZ sources fig-*.tex)
    *.csv               experiment data (results, valgrid, scaling, validation)
