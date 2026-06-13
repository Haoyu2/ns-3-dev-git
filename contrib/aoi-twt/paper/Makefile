all: main.pdf

# pdflatex can exit non-zero on warnings (e.g. acmart in nonacm mode), which
# would make `make` stop before the ref-resolving passes -- prefix with `-`.
main.pdf: main.tex refs.bib appendix.tex $(wildcard results_macros.tex)
	-pdflatex -interaction=nonstopmode main
	-bibtex main
	-pdflatex -interaction=nonstopmode main
	pdflatex -interaction=nonstopmode main

# technical-report version with the full-proofs appendix
techreport: main.tex refs.bib appendix.tex
	-pdflatex -interaction=nonstopmode -jobname=techreport "\def\TECHREPORT{1}\input{main}"
	-bibtex techreport
	-pdflatex -interaction=nonstopmode -jobname=techreport "\def\TECHREPORT{1}\input{main}"
	pdflatex -interaction=nonstopmode -jobname=techreport "\def\TECHREPORT{1}\input{main}"

clean:
	rm -f main.aux main.bbl main.blg main.log main.out main.pdf \
	      techreport.aux techreport.bbl techreport.blg techreport.log \
	      techreport.out techreport.pdf

.PHONY: all clean techreport
