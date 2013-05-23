#!/usr/bin/perl
use strict;
use warnings;

my $step = @ARGV < 2 ? 15 : int $ARGV[1];
my $end  = @ARGV < 3 ? -1 : int $ARGV[2];

die "usage: $0 logfile [step=42] [end=-1]\n" unless @ARGV > 0;

open my $h, "<", $ARGV[0];
my $last = -1;
my $lastm = -1;
my $pub = 0;
my $got = 0;
my $agg = 0;
while (<$h>) {
  /^(\d+)/;
  my $t = int(0.5 + $1/1000);
  $pub++ if /publish:/;
  $got++ if /got:/;
  $agg++ if /node: aggregate/;

  last if $end > -1 and $t > $end;
  next if $pub == 0 or $got == 0;

  if ($t != $last) {
    $last = $t;
    if (int($t / $step) > $lastm) {
      $lastm = $t / $step;
      printf "%d %.5f %d\n", $t, $got/($pub-$agg), $pub-$agg-$got;
    }
  }
}

printf "%d %.5f %d\n", $last, $got/($pub-$agg), $pub-$agg-$got;
