#!/usr/bin/perl -w

# The following script tries to detect version number from the CHANGELOG

use strict;

if(!@ARGV) {
    die("Error must supply the file to work on\n");
}
my $file = $ARGV[0];
my @lines = `head $file`;

foreach my $line (@lines) {
    if($line =~ /---\*\*/) {
	#print "Found section line\n";
	if($line =~ /Version:\s+([\w.]*)\s+/) {
	    print "$1";
	    exit(0);
	}
	else {
	    die("Section line does not contain version\n");
	}
	last;
    }
}
exit(1);
