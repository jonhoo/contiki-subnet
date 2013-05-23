set xlabel "seconds"
set grid
set xrange [-60:1260]
set yrange [-0.1:1.1]
set offset graph 0.02, 0.02
set key right bottom

set terminal epslatex
set output "loss-ratios.tex"
plot "0-loss.ratios.dat" using 1:2 title '0\% loss' with points, \
     "1-loss.ratios.dat" using 1:2 title '1\% loss' with points, \
     "2-loss.ratios.dat" using 1:2 title '2\% loss' with points, \
     "5-loss.ratios.dat" using 1:2 title '5\% loss' with points, \
     "10-loss.ratios.dat" using 1:2 title '10\% loss' with points

set title "Subnet delivery ratio with loss"
set ylabel "ratio"
set output
set terminal wxt
replot
