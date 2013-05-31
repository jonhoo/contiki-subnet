#!/usr/bin/perl
use strict;
use warnings;

my $step = @ARGV < 2 ? 15 : int $ARGV[1];
my $end  = @ARGV < 3 ? -1 : int $ARGV[2];
my $offset = 0;

die "usage: $0 logfile [step=15] [end=-1]\n" unless @ARGV > 0;

if ($ARGV[0] =~ /(\d+)-loss/) {
  my $l = $1;
  $offset = 1 if $l == 1;
  $offset = 2 if $l == 2;
  $offset = 3 if $l == 5;
  $offset = 4 if $l == 10;
  $offset *= 3;
}

open my $h, "<", $ARGV[0];
my $last = -1;
my $lastm = -1;
my $pub = 0;
my $got = 0;
my $agg = 0;
my $dupish = 0;
while (<$h>) {
  /^(\d+)/;
  my $t = int(0.5 + $1/1000);
  $pub++ if /publish:/;
  $got++ if /got:/;
  $agg++ if /node: aggregate/;

  if (/resurrected (\d+) non-empty fragments?/) {
    $dupish += $1;
  }

  last if $end > -1 and $t > $end;
  next if $pub == 0 or $got == 0;

  if ($t != $last) {
    $last = $t;
    if (int($t / $step) > $lastm) {
      $lastm = $t / $step;
      printf "%d %.5f %.5f %.5f\n", $t + $offset, $got/($pub-$agg), $got/($pub+($dupish/2)-$agg), $got/($pub+$dupish-$agg)
    }
  }
}

printf "%d %.5f %.5f %.5f\n", $last + $offset, $got/($pub-$agg), $got/($pub+($dupish/2)-$agg), $got/($pub+$dupish-$agg)
