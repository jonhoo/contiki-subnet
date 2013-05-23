set xrange [-1:33]
set grid
set key right bottom

set terminal epslatex
set output "overhead-packets.tex"
plot "stats.dat" using 1:2 title 'Sparse network' with linespoints, \
     "stats.dat" using 1:4 title 'Dense network' with linespoints

set title "Subnet overhead in packets"
set xlabel "nodes"
set ylabel "packets"
set output
set terminal wxt
replot
