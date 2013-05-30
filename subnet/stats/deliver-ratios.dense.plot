set grid
set xrange [-60:1260]
set yrange [0.85:1.01]
set key right bottom

set terminal epslatex
set output "dense-ratios.tex"
plot "dense-32.ratios.dat" using 1:2 title '32 nodes' with points, \
     "dense-16.ratios.dat" using 1:2 title '16 nodes' with points, \
     "dense-8.ratios.dat" using 1:2 title '8 nodes' with points

set xlabel "seconds"
set ylabel "ratio"
set title "Subnet delivery ratio for dense topology"
set output
set terminal wxt
replot
