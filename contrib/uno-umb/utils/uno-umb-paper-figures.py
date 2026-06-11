#!/usr/bin/env python3
"""Build paper figure sources from UNO-UMB summary tables."""

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class PointSpec:
    label: str
    scenario: str
    controls: tuple[str, ...]
    policy: str
    color: str
    marker: str
    anchor: str
    dx: float
    dy: float


@dataclass(frozen=True)
class PanelSpec:
    title: str
    points: tuple[PointSpec, ...]


PANELS = (
    PanelSpec(
        title="Center burst, 2x load",
        points=(
            PointSpec(
                label="Static twin",
                scenario="center 2x",
                controls=("no forecast",),
                policy="twin",
                color="black",
                marker="*",
                anchor="south east",
                dx=-0.45,
                dy=1.0,
            ),
            PointSpec(
                label="Adaptive wakeup",
                scenario="center 2x",
                controls=("no forecast",),
                policy="adaptive-twin",
                color="blue!70!black",
                marker="square*",
                anchor="north west",
                dx=0.45,
                dy=-1.2,
            ),
        ),
    ),
    PanelSpec(
        title="Right-edge transition, 1.5x load",
        points=(
            PointSpec(
                label="No forecast",
                scenario="right-edge 1.5x",
                controls=("no forecast",),
                policy="twin",
                color="black",
                marker="*",
                anchor="south east",
                dx=-0.45,
                dy=1.1,
            ),
            PointSpec(
                label="0.25 margin",
                scenario="right-edge 1.5x",
                controls=("-25% forecast + 0.25 margin",),
                policy="twin",
                color="blue!70!black",
                marker="square*",
                anchor="south east",
                dx=-0.35,
                dy=1.0,
            ),
            PointSpec(
                label="Selective|margin",
                scenario="right-edge 1.5x",
                controls=("-25% forecast + selective overlap margin",),
                policy="twin",
                color="teal!70!black",
                marker="diamond*",
                anchor="north west",
                dx=0.35,
                dy=-1.0,
            ),
            PointSpec(
                label="Global 0.35|margin",
                scenario="right-edge 1.5x",
                controls=("-25% forecast + 0.35 margin",),
                policy="twin",
                color="orange!80!black",
                marker="triangle*",
                anchor="north east",
                dx=-0.45,
                dy=-0.8,
            ),
        ),
    ),
)


def tex_escape(value):
    replacements = {
        "\\": r"\textbackslash{}",
        "&": r"\&",
        "%": r"\%",
        "$": r"\$",
        "#": r"\#",
        "_": r"\_",
        "{": r"\{",
        "}": r"\}",
        "~": r"\textasciitilde{}",
        "^": r"\textasciicircum{}",
    }
    return "".join(replacements.get(char, char) for char in value)


def load_rows(path):
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def as_float(row, field):
    return float(row[field])


def find_rows(rows, point):
    selected = []
    for control in point.controls:
        matches = [
            row
            for row in rows
            if row["scenario"] == point.scenario
            and row["control"] == control
            and row["policy"] == point.policy
        ]
        if len(matches) != 1:
            raise ValueError(
                f"expected one row for {point.scenario} / {control} / {point.policy}, "
                f"found {len(matches)}"
            )
        selected.append(matches[0])

    base = selected[0]
    for row in selected[1:]:
        for field in (
            "feasible_controller_rows",
            "safe_run_rate",
            "safe_energy_saving_pct_mean",
            "induced_violation_rate",
        ):
            if abs(as_float(base, field) - as_float(row, field)) > 1e-6:
                raise ValueError(
                    f"combined label {point.label!r} has mismatched {field} values"
                )
    return base


def safe_count(row):
    feasible = round(as_float(row, "feasible_controller_rows"))
    safe = round(feasible * as_float(row, "safe_run_rate"))
    return safe, feasible


def write_point(handle, row, point):
    saving = as_float(row, "safe_energy_saving_pct_mean")
    safe_pct = 100.0 * as_float(row, "safe_run_rate")
    label_x = saving + point.dx
    label_y = safe_pct + point.dy
    safe, feasible = safe_count(row)
    label = r"\\".join(tex_escape(part) for part in point.label.split("|"))

    handle.write(
        "  \\addplot[only marks, "
        f"mark={point.marker}, mark size=2.6pt, color={point.color}, "
        f"mark options={{fill={point.color}}}] "
        f"coordinates {{({saving:.3f},{safe_pct:.3f})}};\n"
    )
    handle.write(
        "  \\node[font=\\scriptsize, align=center, "
        f"anchor={point.anchor}] "
        f"at (axis cs:{label_x:.3f},{label_y:.3f}) "
        f"{{{label}\\\\{safe}/{feasible}}};\n"
    )


def write_panel(handle, rows, panel):
    ylabel = "ylabel={Safe feasible-row rate (\\%)},"
    handle.write("\\begin{minipage}{0.96\\linewidth}\n")
    handle.write("\\centering\n")
    handle.write("\\begin{tikzpicture}\n")
    handle.write("\\begin{axis}[\n")
    handle.write(f"  title={{{tex_escape(panel.title)}}},\n")
    handle.write("  width=0.96\\linewidth,\n")
    handle.write("  height=0.53\\linewidth,\n")
    handle.write("  xmin=0,\n")
    handle.write("  xmax=25,\n")
    handle.write("  ymin=70,\n")
    handle.write("  ymax=103,\n")
    handle.write("  xlabel={Safe energy saving (\\%)},\n")
    handle.write(f"  {ylabel}\n")
    handle.write("  grid=both,\n")
    handle.write("  major grid style={draw=gray!25},\n")
    handle.write("  minor tick num=1,\n")
    handle.write("  tick align=outside,\n")
    handle.write("  clip=false,\n")
    handle.write("]\n")
    handle.write(
        "  \\addplot[densely dotted, gray!70] coordinates {(0,100) (25,100)};\n"
    )
    for point in panel.points:
        write_point(handle, find_rows(rows, point), point)
    handle.write("\\end{axis}\n")
    handle.write("\\end{tikzpicture}\n")
    handle.write("\\end{minipage}\n")


def write_figure(rows, output_tex):
    output_tex.parent.mkdir(parents=True, exist_ok=True)
    with output_tex.open("w") as handle:
        handle.write(
            "% Generated from paper/data/main-evaluation-summary.csv by "
            "utils/uno-umb-paper-figures.py.\n"
        )
        for index, panel in enumerate(PANELS):
            if index:
                handle.write("\\par\\vspace{0.8ex}\n")
            write_panel(handle, rows, panel)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summary_csv", type=Path)
    parser.add_argument(
        "--output-tex",
        type=Path,
        default=Path("contrib/uno-umb/paper/figures/evaluation-tradeoff.tex"),
        help="Path for the generated figure source",
    )
    args = parser.parse_args()

    write_figure(load_rows(args.summary_csv), args.output_tex)
    print(f"Wrote {args.output_tex}")


if __name__ == "__main__":
    main()
