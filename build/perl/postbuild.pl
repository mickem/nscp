use strict;
use POSIX;
use File::Copy;
use File::Basename;

if ($#ARGV != 0) {
  die "Incorrect syntax, usage: $ARGV[0] <file> ($#ARGV)"
}

my $file = $ARGV[0];
my ($name,$path,$suffix) = fileparse($file,"\.zip");
my $datestamp = strftime("%Y%m%d-%H%M",localtime) ;
move $file,$path."\\".$name."-".$datestamp.$suffix
  or die "Cannot copy $file $!\n" ;
