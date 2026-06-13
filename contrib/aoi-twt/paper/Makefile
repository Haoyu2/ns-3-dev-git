# Two formats share body.tex / abstract.tex / commonmacros.tex:
#   main.tex       -> ACM (acmart sigconf)   -> main.pdf / techreport.pdf
#   main-ieee.tex  -> IEEE (IEEEtran)        -> main-ieee.pdf / techreport-ieee.pdf
# IEEE is the MSWiM submission format. pdflatex can exit non-zero on warnings,
# which would stop make before the ref-resolving passes -- prefix with `-`.

SHARED = body.tex abstract.tex commonmacros.tex refs.bib appendix.tex \
         $(wildcard results_macros.tex) $(wildcard lb_macros.tex)

all: main-ieee.pdf main.pdf

main-ieee.pdf: main-ieee.tex $(SHARED)
	-pdflatex -interaction=nonstopmode main-ieee
	-bibtex main-ieee
	-pdflatex -interaction=nonstopmode main-ieee
	pdflatex -interaction=nonstopmode main-ieee

main.pdf: main.tex $(SHARED)
	-pdflatex -interaction=nonstopmode main
	-bibtex main
	-pdflatex -interaction=nonstopmode main
	pdflatex -interaction=nonstopmode main

# technical-report versions (with the full-proofs appendix)
techreport-ieee: main-ieee.tex $(SHARED)
	-pdflatex -interaction=nonstopmode -jobname=techreport-ieee "\def\TECHREPORT{1}\input{main-ieee}"
	-bibtex techreport-ieee
	-pdflatex -interaction=nonstopmode -jobname=techreport-ieee "\def\TECHREPORT{1}\input{main-ieee}"
	pdflatex -interaction=nonstopmode -jobname=techreport-ieee "\def\TECHREPORT{1}\input{main-ieee}"

techreport: main.tex $(SHARED)
	-pdflatex -interaction=nonstopmode -jobname=techreport "\def\TECHREPORT{1}\input{main}"
	-bibtex techreport
	-pdflatex -interaction=nonstopmode -jobname=techreport "\def\TECHREPORT{1}\input{main}"
	pdflatex -interaction=nonstopmode -jobname=techreport "\def\TECHREPORT{1}\input{main}"

clean:
	rm -f *.aux *.bbl *.blg *.log *.out main.pdf main-ieee.pdf \
	      techreport.pdf techreport-ieee.pdf

.PHONY: all clean techreport techreport-ieee
