all: overhead ratios loss-ratios

clean:
	rm *.eps *.tex *.dat

loss-ratios: loss-ratios.eps
ratios: dense-ratios.eps sparse-ratios.eps
overhead: overhead-packets.eps overhead-bytes.eps

overhead-%.eps: overhead-%.plot stats.dat
	gnuplot overhead-$*.plot

stats.dat: dense-32.dat dense-16.dat dense-8.dat dense-4.dat dense-2.dat dense-1.dat \
           sparse-32.dat sparse-16.dat sparse-8.dat sparse-4.dat sparse-2.dat sparse-1.dat
	./extract-packets-bytes > $@

dense-ratios.eps: dense-32.ratios.dat dense-16.ratios.dat dense-8.ratios.dat \
                  deliver-ratios.dense.plot
	gnuplot deliver-ratios.dense.plot

sparse-ratios.eps: sparse-32.ratios.dat sparse-16.ratios.dat sparse-8.ratios.dat \
                   deliver-ratios.sparse.plot
	gnuplot deliver-ratios.sparse.plot

loss-ratios.eps: 0-loss.ratios.dat 1-loss.ratios.dat 2-loss.ratios.dat 5-loss.ratios.dat 10-loss.ratios.dat \
                 deliver-ratios.loss.plot
	gnuplot deliver-ratios.loss.plot

%-loss.ratios.dat: ../logs/loss/%-loss/raw.log
	./deliver-ratios.pl $< > $@

%.ratios.dat: ../logs/scale/%/raw.log
	./deliver-ratios.pl $< > $@

%.dat: ../logs/scale/%/raw.log
	./analyze $(dir $<) > $@
