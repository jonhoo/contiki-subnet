set xrange [-1:33]
set grid
set key right bottom

set terminal epslatex
set output "overhead-bytes.tex"
plot "stats.dat" using 1:3 title 'Sparse network' with linespoints, \
     "stats.dat" using 1:5 title 'Dense network' with linespoints

set title "Subnet overhead in bytes"
set xlabel "nodes"
set ylabel "kbytes"
set output
set terminal wxt
replot
