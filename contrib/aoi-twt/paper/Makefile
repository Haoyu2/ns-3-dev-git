# Two formats share body.tex / abstract.tex / commonmacros.tex:
#   main.tex       -> ACM (acmart sigconf)   -> main.pdf / techreport.pdf
#   main-ieee.tex  -> IEEE (IEEEtran)        -> main-ieee.pdf / techreport-ieee.pdf
# IEEE is the MSWiM submission format. latexmk runs the correct passes, tracks
# figure dependencies, and -- unlike a hand-rolled multi-pass recipe -- returns
# a non-zero exit on real LaTeX/BibTeX errors, so build failures are not hidden.

LATEXMK = latexmk -pdf -interaction=nonstopmode -halt-on-error

all: main-ieee.pdf main.pdf

main-ieee.pdf: main-ieee.tex body.tex abstract.tex commonmacros.tex refs.bib
	$(LATEXMK) main-ieee

main.pdf: main.tex body.tex abstract.tex commonmacros.tex refs.bib
	$(LATEXMK) main

# technical-report versions (with the full-proofs appendix)
techreport-ieee:
	$(LATEXMK) -jobname=techreport-ieee -usepretex='\def\TECHREPORT{1}' main-ieee

techreport:
	$(LATEXMK) -jobname=techreport -usepretex='\def\TECHREPORT{1}' main

clean:
	latexmk -C
	rm -f techreport.* techreport-ieee.* *.bbl

.PHONY: all clean techreport techreport-ieee
