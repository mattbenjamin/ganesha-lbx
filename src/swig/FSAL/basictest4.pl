#!/usr/bin/perl

use strict;
use warnings;

use File::Basename;
use Getopt::Std;

use ConfigParsing;
use MesureTemps;
use FSAL;
BEGIN {
  require FSAL_tools;
  import FSAL_tools qw(:DEFAULT fsal_str2path fsal_lookupPath fsal_str2name fsal_lookup fsal_setattrs fsal_getattrs);
}

# get options
my $options = "tvs:f:p:";
my %opt;
my $usage = sprintf "Usage: %s [-t] [-v] -s <server_config> -f <config_file> [-p <path>]", basename($0);
getopts( $options, \%opt ) or die "Incorrect option used\n$usage\n";
my $conf_file = $opt{f} || die "Missing Config File\n";
my $server_config = $opt{s} || die "Missing Server Config File\n";

# filehandle & path
my $roothdl = FSAL::fsal_handle_t::new();
my $curhdl = FSAL::fsal_handle_t::new();
my $rootstr = $opt{p} || "/";
my $curstr = ".";
my $flag_v = ($opt{v}) ? 1 : 0;

my $ret;

my $start = MesureTemps::Temps::new();
my $end = MesureTemps::Temps::new();

# Init layers
$ret = fsalinit(  CONFIG_FILE   => $server_config,
                  FLAG_VERBOSE  => $flag_v
       );
if ($ret != 0) { die "Initialization FAILED\n"; }

# go to the mount point.
my $path = FSAL::fsal_path_t::new();
$ret = fsal_str2path($rootstr, \$path);
if ($ret != 0) { die "str2path failed\n"; }
$ret = fsal_lookupPath($path, \$roothdl);
if ($ret != 0) { die "lookupPath failed\n"; }
FSAL::copy_fsal_handle_t($roothdl, $curhdl);

# get parameters for test
my $param = ConfigParsing::readin_config($conf_file);
if (!defined($param)) { die "Nothing to built\n"; }

my $b = ConfigParsing::get_btest_args($param, $ConfigParsing::FOUR);
if (!defined($b)) { die "Missing test number four\n"; }

if ($b->{files} == -1) { die "Missing 'files' parameter in the config file $conf_file for the basic test number four\n"; }
if ($b->{count} == -1) { die "Missing 'count' parameter in the config file $conf_file for the basic test number four\n"; }

printf "%s : setattr, getattr, and lookup\n", basename($0);

# get the test directory filehandle
my $teststr = ConfigParsing::get_test_directory($param);
$ret = testdir($roothdl, \$curstr, \$curhdl, $teststr);
if ($ret != 0) { die; }

# run ...
my ($totfiles, $totdirs) = (0, 0);
$ret = dirtree($curhdl, 1, $b->{files}, 0, $b->{fname}, $b->{dname}, \$totfiles, \$totdirs);
if ($ret != 0) {die "Basic test number four failed : can't create files\n"; }

MesureTemps::StartTime($start);
for (my $i=0; $i<$b->{count}; $i++) {
  for (my $fi=0; $fi<$b->{files}; $fi++) {
    # LOOKUP 
    my $hdl = FSAL::fsal_handle_t::new();
    my $lookupname = FSAL::fsal_name_t::new();
    $ret = fsal_str2name($b->{fname}.$fi, \$lookupname);
    if ($ret != 0) { die "str2name failed\n"; }
    $ret = fsal_lookup($curhdl, $lookupname, \$hdl);
    if ($ret != 0) { die; }
    # CHMOD NONE (SETATTR)
    my $sattr = FSAL::fsal_attrib_list_t::new();
    $sattr->{asked_attributes} = FSAL::get_mask_attr_mode();
    $sattr->{mode} = $FSAL::CHMOD_NONE;
    $ret = fsal_setattrs($hdl, $sattr);
    if ($ret != 0) { die "can't chmod CHMOD_NONE ".$b->{fname}.$fi."\n"; }
    # GETATTR
    my $fattr = FSAL::fsal_attrib_list_t::new();
    $fattr->{asked_attributes} = FSAL::get_mask_attr_mode();
    $ret = fsal_getattrs($hdl, \$fattr);
    if ($ret != 0) { die "can't stat ".$b->{fname}.$fi." after CHMOD_NONE\n"; }
    # MODE VALUE
    if (!FSAL::are_accessmode_equal($fattr->{mode}, $FSAL::CHMOD_NONE)) { 
      printf $b->{fname}.$fi." has mode 0x%04x after chmod NONE (0x%04x)\n", FSAL::get_int_from_accessmode($fattr->{mode}), $FSAL::CHMOD_NONE; 
      die;
    }
    # CHMOD RW
    $sattr->{mode} = $FSAL::CHMOD_RW;
    $ret = fsal_setattrs($hdl, $sattr);
    if ($ret != 0) { die "can't chmod CHMOD_RW ".$b->{fname}.$fi."\n"; }
    # GETATTR
    $ret = fsal_getattrs($hdl, \$fattr);
    if ($ret != 0) { die "can't stat ".$b->{fname}.$fi." after CHMOD_RW\n"; }
    # MODE VALUE
    if (!FSAL::are_accessmode_equal($fattr->{mode}, $FSAL::CHMOD_RW)) { 
      printf $b->{fname}.$fi." has mode 0x%04x after chmod RW (0x%04x)\n", FSAL::get_int_from_accessmode($fattr->{mode}), $FSAL::CHMOD_RW; 
      die;
    }
  }
}
MesureTemps::EndTime($start, $end);

# print results
print "\t".($b->{files} * $b->{count} * 2)." chmods and stats on ".$b->{files}." files";
if ($opt{t}) {printf " in %s seconds", MesureTemps::ConvertiTempsChaine($end, ""); }
print "\n";

# log
my $log_file = ConfigParsing::get_log_file($param);
open(LOG, ">>$log_file") or die "Enable to open $log_file : $!\n";
printf LOG "b4\t".($b->{files} * $b->{count} * 2)."\t".$b->{files}."\t%s\n", MesureTemps::ConvertiTempsChaine($end, "");
close LOG;

print "Basic test number four OK ! \n";
