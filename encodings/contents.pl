#!/usr/bin/perl

# fuck. more perl.

# this thing is to generate a single big array indicating which
# charsets contain each unicode codepoint.

# ascii and latin1

foreach $a ( 1 .. 127 ) {
    $s{$a}{"US-ASCII"} = 1;
    $s{$a}{"ISO-8859-1"} = 1;
}

$s{0}{"ISO-8859-1"} = 1;
foreach $a ( 160 .. 255 ) {
    $s{$a}{"ISO-8859-1"} = 1;
}

$v{"US-ASCII"} = 1;
$v{"ISO-8859-1"} = 2;
$mcs = 2;
$max = 255;

$/ = undef;
sub add() {
    local ($f,$n) = @_;
    $mcs = 2*$mcs;
    $v{$n} = $mcs;
    open( I, "< $f" ) || die "cannot open $f, stopped";
    foreach $cp ( split( /,/, <I> ) ) {
	$cp =~ s/\s+//g;
	$cp = hex $cp;
        if ( $cp < 128 || $cp >= 160 ) {
	    $s{$cp}{$n} = $n;
	    $max = $cp if( $cp > $max );
	}
    }
}

&add( "8859-2.inc", "ISO-8859-2" );
&add( "8859-15.inc", "ISO-8859-15" );
&add( "koi8-r.inc", "KOI8-R" );

#foreach $a ( sort { $a <=> $b } keys %s ) {
#    print "Code point ", $a, " is supported by\n   ";
#    foreach $n ( sort keys %{$s{$a}} ) {
#	print " ", $n;
#    }
#    print "\n";
#}

open( O, "> codec-aliases.inc" )
  || die "unable to open codec-aliases.inc";
print O "// generated by ",
    '$Id$',
    " at ",


($junk,$junk,$junk,$mday,$mon,$year,$junk,$junk) = gmtime();
open( O, "> charset-support.inc" )
  || die "unable to open charset-support.inc";
print O "// generated by ",
    '$Id$',
    " at ",
    sprintf( "%04d-%02d-%02d", $year + 1900, $mon + 1, $mday ), "\n",
    "static char charsetSupport[", 2+$max, "] = {\n    ";
foreach $a ( 0 .. $max ) {
    $v = 0;
    foreach $n ( sort keys %{$s{$a}} ) {
	$v = $v + $v{$n};
    }
    printf( O "0x%04x, ", $v );
    print O "\n    " if ( ( $a % 8 ) == 7 );
}
# evil hack
print O "0\n};\n";
# not so evil
print O "static struct {\n    uint v;\n    const char * n;\n" .
  "} charsetValues[] = {\n";
foreach $v ( sort { $v{$a} <=> $v{$b} } keys %v ) {
    printf( O "    { %3d, \"%s\" },\n", $v{$v}, $v );
}
print O "};\nstatic uint lastSupportedChar = ", $max, ";\n";
