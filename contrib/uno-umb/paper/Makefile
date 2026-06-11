LATEX ?= pdflatex
BIBTEX ?= bibtex

FIGURE_DATA := data/main-evaluation-summary.csv
FIGURE_TEX := figures/evaluation-tradeoff.tex
FIGURE_SCRIPT := ../utils/uno-umb-paper-figures.py

.PHONY: all clean figures

all: draft.pdf

figures: $(FIGURE_TEX)

$(FIGURE_TEX): $(FIGURE_DATA) $(FIGURE_SCRIPT)
	python3 $(FIGURE_SCRIPT) $(FIGURE_DATA) --output-tex $(FIGURE_TEX)

draft.pdf: draft.tex references.bib $(FIGURE_TEX)
	$(LATEX) -interaction=nonstopmode -halt-on-error draft.tex
	$(BIBTEX) draft
	$(LATEX) -interaction=nonstopmode -halt-on-error draft.tex
	$(LATEX) -interaction=nonstopmode -halt-on-error draft.tex

clean:
	rm -f draft.aux draft.bbl draft.blg draft.fdb_latexmk draft.fls draft.log draft.out draft.pdf draft.synctex.gz
