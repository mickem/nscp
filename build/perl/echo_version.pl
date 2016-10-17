use strict;
use POSIX;

if ($#ARGV != 0) {
  die "Incorrect syntax, usage: $ARGV[0] <file> ($#ARGV)"
}

open(DAT, "$ARGV[0]") || die("Could not open file: $ARGV[0] - $!"); 
my @raw_data=<DAT>;
close(DAT); 

foreach my $line (@raw_data)
{
 chomp($line);
 my ($skip,$key,$value)=split(/ +/,$line);
 if ($key eq "PRODUCTVER") {
  my ($major,$minor,$revision,$build)=split(/,/,$value);
   print "$major.$minor.$revision";
   exit 0;
 }
} 
die "ERROR: version not found in $ARGV[0]!";