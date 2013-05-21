set xlabel "seconds"
set ylabel "ratio"
set grid
set xrange [-100:1300]
set yrange [-0.1:1.1]
set offset graph 0.02, 0.02
set key right bottom

set terminal epslatex
set output "ratios.tex"
plot "sparse-32.dat" using 1:2 title 'Sparse 32' with linespoints, \
     "dense-32.dat" using 1:2 title 'Dense 32' with linespoints, \
     "sparse-16.dat" using 1:2 title 'Sparse 16' with linespoints, \
     "dense-16.dat" using 1:2 title 'Dense 16' with linespoints, \
     "sparse-8.dat" using 1:2 title 'Sparse 8' with linespoints, \
     "dense-8.dat" using 1:2 title 'Dense 8' with linespoints

set title "Subnet delivery ratio"
set output
set terminal wxt
replot
