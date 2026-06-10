LATEX ?= pdflatex

.PHONY: all clean

all: draft.pdf

draft.pdf: draft.tex
	$(LATEX) -interaction=nonstopmode -halt-on-error draft.tex
	$(LATEX) -interaction=nonstopmode -halt-on-error draft.tex

clean:
	rm -f draft.aux draft.fdb_latexmk draft.fls draft.log draft.out draft.pdf draft.synctex.gz
