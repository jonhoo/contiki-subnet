set xlabel "nodes"
set ylabel "packets"
set y2label "bytes"
set xrange [-1:33]
set ytics nomirror
set y2tics
set grid
set key right bottom

set terminal epslatex
set output "overhead.tex"
plot "stats.dat" using 1:2 axes x1y1 title 'Sparse packets' with linespoints, \
     "stats.dat" using 1:3 axes x1y2 title 'Sparse bytes' with linespoints, \
     "stats.dat" using 1:4 axes x1y1 title 'Dense bytes' with linespoints, \
     "stats.dat" using 1:5 axes x1y2 title 'Dense bytes' with linespoints

set title "Subnet scaling"
set output
set terminal wxt
replot
