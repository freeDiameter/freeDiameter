#!/usr/bin/env perl
use strict;
use File::Basename;
use Getopt::Std;

my ($progname) = basename($0);

our ($opt_V, $opt_v);

# default to Base
my ($vendor) = 0;
my ($vendor_name) = "";

sub convert_must_to_flags($) {
    my ($allmust) = @_;
    my ($mustfields) = "";
    $mustfields .= "AVP_FLAG_VENDOR |" if ($allmust =~ m/V/);
    $mustfields .= "AVP_FLAG_MANDATORY |" if ($allmust =~ m/M/);
    $mustfields =~ s/ \|$//;
    return $mustfields;
}

sub base_type($) {
    my ($type) = @_;

    return "AVP_TYPE_GROUPED" if ($type =~ m/Grouped/);
    return "AVP_TYPE_OCTETSTRING" if ($type =~ m/(Address|DiameterIdentity|DiameterURI|OctetString|IPFilterRule|Time|UTF8String|QoSFilterRule)/);
    return "AVP_TYPE_INTEGER32" if ($type =~ m/Enumerated|Integer32/);
    return "AVP_TYPE_INTEGER64" if ($type =~ m/Integer64/);
    return "AVP_TYPE_UNSIGNED32" if ($type =~ m/Unsigned32/);
    return "AVP_TYPE_UNSIGNED64" if ($type =~ m/Unsigned64/);
    return "AVP_TYPE_FLOAT32" if ($type =~ m/Float32/);
    return "AVP_TYPE_FLOAT64" if ($type =~ m/Float64/);

    die("unknown type '$type'");
}


my ($comment_width) = 64;

sub print_header() {
    printf "\t/*=%s=*/\n", '=' x $comment_width;
}

sub print_comment($) {
    my ($str) = @_;
    printf "\t/* %-*s */\n", $comment_width, $str;
}

sub print_insert($$) {
    my ($type, $name) = @_;
    my $avp_type;

    if ($type =~ m/(Grouped|OctetString|Integer32|Integer64|Unsigned32|Unsigned64|Float32|Float64)/) {
        $avp_type = "NULL";
    } elsif ($type =~ m/Enumerated/) {
        print "\t\tstruct dict_object\t*type;\n";
        print "\t\tstruct dict_type_data\t tdata = { AVP_TYPE_INTEGER32, \"Enumerated(" . ($vendor_name ? "$vendor_name/" : "") ."$name)\", NULL, NULL, NULL };\n";
        # XXX: add enumerated values
        print "\t\tCHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);\n";
        $avp_type = "type";
    } else {
        $avp_type = "${type}_type";
    }

    print "\t\tCHECK_dict_new(DICT_AVP, &data, $avp_type, NULL);\n";
}

sub usage($) {
    print STDERR "usage: $progname [-V vendor_name] [-v vendor_code] [file ...]\n";
    exit(1);
}

getopts("V:v:") || usage(1);

if (defined($opt_v)) {
    $vendor = $opt_v;
    if (!defined($opt_V)) {
        usage(1);
    }
    $vendor_name = $opt_V;
}

print_header();
print_comment("Start of generated data.");
print_comment("");
print_comment("The following is created automatically with:");
print_comment("    org_to_fd.pl -V '$vendor_name' -v $vendor");
print_comment("Changes will be lost during the next update.");
print_comment("Do not modify; modify the source .org file instead.");
print_header();
print "\n";

while (<>) {
    my ($dummy, $name, $code, $section, $type, $must, $may, $shouldnot, $mustnot, $encr) = split /\s*\|\s*/;

    next if ($name =~ m/Attribute Name/);
    if ($name =~ m/# (.*)/) {
        print_comment($1);
        next;
    }
    if ($name =~ m/#/) {
        print("\n");
        next;
    }

    if ($name =~ m/\s/) {
        die("name '$name' contains space");
    }

    my ($desc) = $name;
    $desc .= ", " . $type;
    $desc .= ", code " . $code;
    $desc .= ", section " . $section if $section ne "";
    print_comment($desc);
    print "\t{\n";
    print "\t\tstruct dict_avp_data data = {\n";
    print "\t\t\t$code,\t/* Code */\n";
    print "\t\t\t$vendor,\t/* Vendor */\n";
    print "\t\t\t\"$name\",\t/* Name */\n";
    print "\t\t\t" . convert_must_to_flags("$must, $mustnot") . ",\t/* Fixed flags */\n";
    print "\t\t\t" . convert_must_to_flags("$must") . ",\t/* Fixed flag values */\n";
    print "\t\t\t" . base_type($type) . "\t/* base type of data */\n";
    print "\t\t};\n";
    print_insert($type, $name);
    print "\t};\n\n";
}

print_header();
print_comment("End of generated data.");
print_header();
