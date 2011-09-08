#!/usr/bin/perl

if(@ARGV < 2) {
  die("Usage $0 dir licence-header\n");
}
my $dir=$ARGV[0];
my $licenceHeader = $ARGV[1];

print "About to apply licence header file $licenceHeader \n".
  "to all source files in $dir\n";


@fileList = `/bin/bash -c "ls $dir/*.{c,h}"`;
chomp(@fileList);

foreach my $file (@fileList) {
  print "Fixing file [$file]\n";
  system("cat $licenceHeader $file > ${file}.back");
  system("mv ${file}.back $file");
}
