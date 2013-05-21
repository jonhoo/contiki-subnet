all: overhead ratios loss-ratios

loss-ratios: loss-ratios.eps
	cp $@.eps $@.tex ~/uni/net/cw2/graphs/

ratios: ratios.eps
	cp $@.eps $@.tex ~/uni/net/cw2/graphs/

overhead: overhead.eps
	cp $@.eps $@.tex ~/uni/net/cw2/graphs/

overhead.eps: overhead.plot stats.dat
	gnuplot overhead.plot

ratios.eps: sparse-32.ratios.dat sparse-16.ratios.dat sparse-8.ratios.dat \
            dense-32.ratios.dat dense-16.ratios.dat dense-8.ratios.dat \
            deliver-ratios.ratios.plot
	gnuplot deliver-ratios.ratios.plot

loss-ratios.eps: 0-loss.ratios.dat 1-loss.ratios.dat 2-loss.ratios.dat 5-loss.ratios.dat 10-loss.ratios.dat \
                 deliver-ratios.loss.plot
	gnuplot deliver-ratios.loss.plot

%-loss.ratios.dat: ../logs/loss/%-loss/raw.log
	./deliver-ratios loss/$*-loss

%.ratios.dat: ../logs/scale/%/raw.log
	./deliver-ratios scale/$*
