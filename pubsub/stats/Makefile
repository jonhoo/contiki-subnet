all: overhead ratios

ratios: ratios.eps
	cp $@.eps $@.tex ~/uni/net/cw2/graphs/

overhead: overhead.eps
	cp $@.eps $@.tex ~/uni/net/cw2/graphs/

overhead.eps: overhead.plot stats.dat
	gnuplot overhead.plot

ratios.eps: sparse-32.ratios.dat sparse-16.ratios.dat sparse-8.ratios.dat \
            dense-32.ratios.dat dense-16.ratios.dat dense-8.ratios.dat
	gnuplot deliver-ratios.plot

%.ratios.dat: ../logs/scale/%/raw.log
	./deliver-ratios $*
