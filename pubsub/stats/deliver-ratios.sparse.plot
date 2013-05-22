set grid
set xrange [-60:1260]
set yrange [-0.1:1.1]
set offset graph 0.02, 0.02
set key right bottom

set terminal epslatex
set output "sparse-ratios.tex"
plot "sparse-32.dat" using 1:2 title '32 nodes' with linespoints, \
     "sparse-16.dat" using 1:2 title '16 nodes' with linespoints, \
     "sparse-8.dat" using 1:2 title '8 nodes' with linespoints

set xlabel "seconds"
set ylabel "ratio"
set title "Subnet delivery ratio for sparse topology"
set output
set terminal wxt
replot
