#!/usr/bin/perl -w

use strict;

sub build_machine_list;
sub get_machine_list;

my $MOSBIN="/cs/mosix/sbin";
my $MOSCTL= "$MOSBIN/mosctl";
my $MOSRUN= "$MOSBIN/mosrun";
my $TESTLOAD= "$MOSBIN/test_load";
my $DAEMONIZE= "$MOSBIN/daemonize";
my $RUNMANY= "$MOSBIN/run_many";
my $RUNMANY_PIPED= "$MOSBIN/run_many_direct_piped";
my $out_dir = "/cs/phd/lior/lab/og/disconn_test";


my $owner_cluster = "mos100";
my $remote_cluster = "bmos-01..14,cmos-01..20";
my $time_node="mos100";

my @owner_cluster = get_machine_list($owner_cluster);
my @remote_cluster = get_machine_list($remote_cluster);
my $owner_size = @owner_cluster;
my $remote_size = @remote_cluster;
my $owner_hn = $owner_cluster[0];
my $remote_hn = $remote_cluster[0];

my $guests_per_node = 1;
my $guest_num = $remote_size * $guests_per_node;
my $guest_mem = 256;

print "Owner     : @owner_cluster\n";
print "Owner HN  : $owner_hn\n";
print "Remote    : @remote_cluster\n";
print "Remote HN : $remote_hn\n";
print "Guest Num : $guest_num\n";
print "      Mem : $guest_mem\n";
print "Press any key to continue ...\n";
<STDIN>;

# Getting all the owner cluster node id
print "Obtaining pes of guest machines  ";
my @remote_pes;
`rsh $remote_hn /etc/init.d/inetd restart`;
foreach my $node (@remote_cluster) {
    my $pe = `rsh $remote_hn $MOSCTL whois $node`;
    chomp($pe);
    print ".";
    push @remote_pes, $pe;
}
@remote_pes = reverse @remote_pes;
print "Done\n";

print "Running Guest processes in Remote cluster\n";

print "Total Guests $guest_num\n";
for (my $i=0 ; $i< $guest_num ; $i++)
{
    my $hn = $owner_cluster[$i % $owner_size];
    my $pe = $remote_pes[$i % $remote_size];
    if(($i % 10) == 0) {
	`rsh $hn /etc/init.d/inetd restart`;
    }

    print "Running $i from $hn on $pe\n";
    `rsh $hn $DAEMONIZE $MOSRUN -L -G -$pe  $TESTLOAD -m $guest_mem --onmig-report --uid 1297 -o $out_dir/$i.out`;
}


print "Press any key to freeze all guests\n";
<STDIN>;
my $start_time = `rsh $time_node date`;
print "Start Time $start_time\n";
system("rsh $owner_hn $MOSCTL freeze ");
#print "/cs/moslnx/bin/cluster_for --verbose $owner_cluster $DAEMONIZE $MOSCTL gblock";
#`/cs/moslnx/bin/cluster_for --verbose  $owner_cluster $DAEMONIZE $MOSCTL gblock`;
#`/cs/moslnx/bin/cluster_for --verbose  $owner_cluster $DAEMONIZE $MOSCTL gblock`;
#`/cs/moslnx/bin/cluster_for --verbose  $owner_cluster $DAEMONIZE $MOSCTL gblock`;


#-------------------------------------------------------------------------
# Building a list of machines from a given range  
# mos1...2
#
sub build_machine_list 
{
    my $range;
    my $list = shift;
    my @range_list = split /\,/ , $list;
    my @list = ();
    my $i;
	
    foreach $range( @range_list ) {
	if( $range =~ /^(.*)\.\.(.*)$/ ) {
	    my $first_machine = $1;
	    my $until = $2;
	    $first_machine =~ /^(.*\D)(\d+)$/ or
		die "`$first_machine' doesn't end with a digit!\n";
	    my $name = $1;
	    my $from = $2;
	    for $i( $from..$until ) {
		push @list , "$name$i";
	    }
	} else {
	    push @list , $range ;
	}
    }

    return @list;
}

#--------------------------------------------------------------------------
# This function is use by add and remove machine scripts 
# to parse this list of the machine
#
sub get_machine_list 
{
    my $list = shift;
    my @two_lists = split /\^/ , $list;
    my $machine;
	
    @two_lists <= 2 or
	die "`!' might appear only once inside machine list!\n";
    my @temp_list = build_machine_list( $two_lists[0] );
    my %del_list = ();
    if( defined $two_lists[1] ) {
	foreach $machine( build_machine_list( $two_lists[1] ) ) {
	    $del_list{$machine} = 1;
	}
    }
    my %list = ();
    foreach $machine( @temp_list ) {
	$list{$machine} = 1 if not defined $del_list{$machine};
    }
    my @l = keys %list;
    return sort_machine_names( \@l);
}
# bmos-01 cmos-05 mos1 mos2 mos218 bmos-05;
sub sort_machine_names {
    my $list = shift;

    my %word = ();
    my %num = ();

    for my $n (@$list) {
	$n =~ /^(\D+)\d*$/;
	$word{$n} = $1;
	if($n =~ /^\D*(\d+)$/) {
	    $num{$n} = $1;
	}
	else {$num{$n} = 0};
    }
    
    my @new = sort { $word{$a} cmp $word{$b} || $num{$a} <=> $num{$b}} @$list;  
    return @new;
}
