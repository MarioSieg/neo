@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
perl -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
perl -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!/usr/bin/perl
#line 15
##########################################################
## This script is part of the Devel::NYTProf distribution
##
## Copyright, contact and other information can be found
## at the bottom of this file, or by going to:
## https://metacpan.org/pod/Devel::NYTProf
##
###########################################################

=head1 NAME

nytprofhtml - Generate reports from Devel::NYTProf data

=head1 SYNOPSIS

Typical usage:

 $ perl -d:NYTProf some_perl_app.pl
 $ nytprofhtml --open

Options synopsis:

 $ nytprofhtml [-h] [-d] [-m] [-o <output directory>] [-f <input file>] [--open]

=encoding ISO8859-1

=cut

use warnings;
use strict;

use Carp;
use Config qw(%Config);
use Getopt::Long;
use List::Util qw(sum max);
use File::Copy;
use File::Spec;
use File::Which qw(which);
use File::Path qw(rmtree);

# Handle --profself before loading Devel::NYTProf::Core
# (because it parses NYTPROF for options)
BEGIN {
    if (grep { $_ eq '--profself' } @ARGV) {
        # profile nytprofhtml itself
        our $profself = "nytprof-nytprofhtml.out";
        $ENV{NYTPROF} .= ":file=$profself:trace=1";
        require Devel::NYTProf;
        END { warn "Profile of $0 written to $profself\n" if our $profself; } 
    }
}

use Devel::NYTProf::Reader;
use Devel::NYTProf::Core;
use Devel::NYTProf::Util qw(
    fmt_float fmt_time fmt_incl_excl_time
    calculate_median_absolute_deviation
    get_abs_paths_alternation_regex
    html_safe_filename
);
use Devel::NYTProf::Constants qw(NYTP_SCi_CALLING_SUB);

our $VERSION = '6.04';

if ($VERSION != $Devel::NYTProf::Core::VERSION) {
    die "$0 version '$VERSION' doesn't match version '$Devel::NYTProf::Core::VERSION' of $INC{'Devel/NYTProf/Core.pm'}\n";
}

my $has_json = eval { require JSON::MaybeXS; JSON::MaybeXS->import(); 1 }
    or warn "Can't load JSON::MaybeXS module - HTML visualizations skipped ($@)\n";

my $script_ext   = ($^O eq "MSWin32") ? "" : ".pl";

my $nytprofcalls = File::Spec->catfile($Config{'bin'}, 'nytprofcalls');
$nytprofcalls    = which 'nytprofcalls' if not -e $nytprofcalls;

die "Unable to find nytprofcalls in $Config{bin} or on the PATH"
    unless $nytprofcalls;

my $flamegraph   = File::Spec->catfile($Config{'bin'}, 'flamegraph') . $script_ext;
$flamegraph      = which "flamegraph$script_ext" if not -e $flamegraph;

die "Unable to find flamegraph$script_ext in $Config{bin} or on the PATH"
    unless $flamegraph;

my @treemap_colors = (0,2,4,6,8,10,1,3,5,7,9);

# These control the limits for what the script will consider ok to severe times
# specified in standard deviations from the mean time
use constant SEVERITY_SEVERE => 2.0;    # above this deviation, a bottleneck
use constant SEVERITY_BAD    => 1.0;
use constant SEVERITY_GOOD   => 0.5;    # within this deviation, okay

use constant NUMERIC_PRECISION => 5;

my @on_ready_js;

GetOptions(
    'file|f=s'   => \(my $opt_file = 'nytprof.out'),
    'lib|l=s'   => \my $opt_lib,
    'out|o=s'   => \(my $opt_out = 'nytprof'),
    'delete|d!' => \my $opt_delete,
    'open!'     => \my $opt_open,
    'help|h'    => sub { exit usage() },
    'minimal|m!'=> \my $opt_minimal,
    'flame!'    => \(my $opt_flame = 1),
    'mergeevals!'=> \(my $opt_mergeevals = 1),
    'profself!'     => sub { }, # handled in BEGIN above
    'debug!'        => \my $opt_debug,
) or do { exit usage(); };


DB::set_option('blocks', 0) if $opt_minimal;

sub usage {
    print <<END;
usage: [perl] nytprofhtml [opts]
 --file <file>, -f <file>  Read profile data from the specified file [default: nytprof.out]
 --out <dir>,   -o <dir>   Write report files to this directory [default: nytprof]
 --delete,      -d         Delete any old report files in <dir> first
 --open                    Open the generated report in a web browser
 --lib <lib>,   -l <lib>   Add <lib> to the beginning of \@INC
 --no-flame                Disable flame graph (and call stacks processing)
 --minimal,     -m         Don't generate graphviz .dot files or block/sub-level reports
 --no-mergeevals           Disable merging of string evals
 --help,        -h         Print this message

This script of part of the Devel::NYTProf distribution.
See http://metacpan.org/release/Devel-NYTProf/ for details and copyright.
END
    return 0;
}


# handle output location
if (!-e $opt_out) {
    # will be created
}
elsif (!-d $opt_out) {
    die "$0: Specified output directory '$opt_out' already exists as a file!\n";
}
elsif (!-w $opt_out) {
    die "$0: Unable to write to output directory '$opt_out'\n";
}
else {
    if (defined($opt_delete)) {
        print "Deleting existing $opt_out directory\n";
        rmtree($opt_out);
    }
}

# handle custom lib path
if (defined($opt_lib)) {
    warn "$0: Specified lib directory '$opt_lib' does not exist.\n"
        unless -d $opt_lib;
    require lib;
    lib->import($opt_lib);
}

$SIG{USR2} = \&Carp::cluck
    if exists $SIG{USR2}; # some platforms don't have SIGUSR2 (Windows)

my $reporter = new Devel::NYTProf::Reader($opt_file, {
    quiet => 0,
    skip_collapse_evals => !$opt_mergeevals,
});

# place to store this
$reporter->output_dir($opt_out);

# set formatting for html
$reporter->set_param(
    'header',
    sub {
        my ($profile, $fi, $output_filestr, $level) = @_;

        my $profile_level_buttons = ($fi->is_eval)
            ? '' : get_level_buttons($profile->get_profile_levels, $output_filestr, $level);

        my $subhead = qq{&emsp;&emsp;$profile_level_buttons<br />
            For ${ \($profile->{attribute}{application}) }
        };

        my $html_header = get_html_header("Profile of ".$fi->filename_without_inc);
        my $page_header = get_page_header(
            profile  => $profile,
            title    => "NYTProf Performance Profile",
            subtitle => $subhead,
        );
        my $filename_escaped = _escape_html($fi->filename);
        my @intro_rows = (
            [ "Filename", $fi->is_file
                ? sprintf(q{<a href="file://%s">%s</a>}, $fi->filename, $filename_escaped)
                : $filename_escaped ],
            [ "Statements", sprintf "Executed %d statements in %s",
                $fi->sum_of_stmts_count, fmt_time($fi->sum_of_stmts_time) ],
        );
        if ($fi->is_eval) {
            push @intro_rows, [
                "Eval Invoked At", sprintf q{<a %s>%s line %d</a>},
                    $reporter->href_for_file($fi->eval_fi, $fi->eval_line),
                    _escape_html($fi->eval_fi->filename), $fi->eval_line
            ];

            my @sibling_html;
            for my $e_fi ($fi->sibling_evals) {
                if ($e_fi == $fi) {
                    push @sibling_html, 1+@sibling_html;
                }
                else {
                    push @sibling_html, sprintf qq{<a %s>%d</a>},
                        $reporter->href_for_file($e_fi),
                        1+@sibling_html;
                }
            }
            push @intro_rows, [ "Sibling evals",
                join ", ", @sibling_html
            ] if @sibling_html >= 2;
        }

        my $intro_table = join "\n", map {
            sprintf q{<tr><td class="h">%s</td><td align="left">%s</td></tr>}, @$_
        } @intro_rows;

        return join "\n", $html_header, $page_header,
            q{<div class="body_content"><br />},
            qq{<table class="file_summary">$intro_table</table>},
    }
);

$reporter->set_param(
    'taintmsg',
    qq{<br /><div class="warn_title">WARNING!</div>\n
<div class="warn">The source file used to generate this report was modified
after the profiler data was generated.
The data might be out of sync with the modified source code so you should regenerate it.
Meanwhile, the data on this page might not make much sense!</div>\n}
);

$reporter->set_param(
    'sawampersand',
    sub {
        my ($profile, $fi) = @_;
        my $line = $profile->{attribute}{sawampersand_line};
        return qq{<br /><div class="warn_title">NOTE!</div>\n
<div class="warn"><p>While profiling this file Perl noted the use of one or more special
variables that impact the performance of <i>all</i> regular expressions in the program.</p>

<p>Use of the "<tt>\$`</tt>", "<tt>\$&</tt>", and "<tt>\$'</tt>" variables should be replaced with faster alternatives.<br />
See the WARNING at the end of the <a href="http://perldoc.perl.org/perlre.html#Capture-buffers">
Capture Buffers section of the perlre documentation</a>.</p>

<p>The use is detected by perl at compile time but by NYTProf during execution.
NYTProf first noted it when executing <a href="#$line">line $line</a>.
That was probably the first statement executed by the program after perl
compiled the code containing the variables.
If the variables can't be found by studying the source code, try using the
<a href="http://metacpan.org/pod/Devel::FindAmpersand">Devel::FindAmpersand</a>
or <a href="http://metacpan.org/pod/B::Lint">B::Lint</a>
modules.</p>

</div>\n}
    }
) if $] < 5.017008;

$reporter->set_param(
    'merged_fids',
    sub {
        my ($profile, $fi) = @_;

        my $merged_fids = $fi->meta->{merged_fids};
        my $evals_shown = 1 + scalar @$merged_fids;

        my @siblings = $fi->sibling_evals;
        my $merged_siblings = sum(map { scalar @{$_->meta->{merged_fids}||[]} } @siblings);
        my $evals_total = @siblings + $merged_siblings;

        my @msg;
        push @msg, sprintf qq{
            The data used to generate this report page was merged from %s<br />
            of the string eval on line %d of %s.
        },  ($evals_shown == $evals_total)
                ? sprintf("all %d executions", $evals_shown)
                : sprintf("%d of the %d executions", $evals_shown, $evals_total),
            $fi->eval_line, $fi->eval_fi->filename;

        push @msg, qq{
            The source code shown below is the text of just one of the calls to the eval.<br />\n
            This report page might not make much sense because the argument source code of those eval calls varied.<br />\n
        } if $fi->meta->{merged_fids_src_varied};

        return sprintf qq{<br /><div class="warn_title">NOTE!</div>\n
            <div class="warn">%s</div>
        }, join "<br />", @msg;
    },
);


sub calc_mad_from_objects {
    my ($ary, $meth, $ignore_zeros) = @_;
    return calculate_median_absolute_deviation([map { scalar $_->$meth } @$ary], $ignore_zeros);
}

sub subroutine_table {
    my ($profile, $fi, $max_subs, $sortby) = @_;
    $sortby ||= 'excl_time';

    my $subs_in_file = ($fi)
        ? $profile->subs_defined_in_file($fi, 0)
        : $profile->subname_subinfo_map;
    return "" unless $subs_in_file && %$subs_in_file;

    my $inc_path_regex = get_abs_paths_alternation_regex([$profile->inc], qr/^|\[/);
    my $filestr = ($fi) ? $fi->filename : undef;

    # XXX slow - use Schwartzian transform or via XS or Sort::Key
    my @subs =
        sort { $b->$sortby <=> $a->$sortby or $a->subname cmp $b->subname }
        values %$subs_in_file;

    # in the overall summary, don't show subs that were never called
    @subs = grep { $_->calls > 0 } @subs if !$fi;

    my $dev_incl_time  = calc_mad_from_objects(\@subs, 'incl_time',    1);
    my $dev_excl_time  = calc_mad_from_objects(\@subs, 'excl_time',    1);
    my $dev_calls      = calc_mad_from_objects(\@subs, 'calls',        1);
    my $dev_call_count = calc_mad_from_objects(\@subs, 'caller_count', 1);
    my $dev_call_fids  = calc_mad_from_objects(\@subs, 'caller_fids',  1);

    my @subs_to_show = ($max_subs) ? splice @subs, 0, $max_subs : @subs;
    my $qualifier = (@subs > @subs_to_show) ? "Top $max_subs " : "";
    my $max_pkg_name_len = max(map { length($_->package) } @subs_to_show);

    my $sub_links;

    my $sortby_desc = ($sortby eq 'excl_time') ? "exclusive time" : "inclusive time";
    $sub_links .= qq{
        <table id="subs_table" border="1" cellpadding="0" class="tablesorter floatHeaders">
        <caption>${qualifier}Subroutines</caption>
        <thead>
        <tr>
        <th>Calls</th>
        <th><span title="Number of Places sub is called from">P</span></th>
        <th><span title="Number of Files sub is called from">F</span></th>
        <th>Exclusive<br />Time</th>
        <th>Inclusive<br />Time</th>
        <th>Subroutine</th>
        </tr>
        </thead>
    };

    my $profiler_active = $profile->{attribute}{profiler_active};

    my @rows;
    $sub_links .= "<tbody>\n";
    for my $sub (@subs_to_show) {

        $sub_links .= "<tr>";

        $sub_links .= determine_severity($sub->calls        || 0, $dev_calls);
        $sub_links .= determine_severity($sub->caller_count || 0, $dev_call_count);
        $sub_links .= determine_severity($sub->caller_fids  || 0, $dev_call_fids);
        $sub_links .= determine_severity($sub->excl_time    || 0, $dev_excl_time, 1,
            sprintf("%.1f%%", $sub->excl_time/$profiler_active*100)
        );
        $sub_links .= determine_severity($sub->incl_time    || 0, $dev_incl_time, 1,
            sprintf("%.1f%%", $sub->incl_time/$profiler_active*100)
        );

        my @hints;

        # package and subname
        my $subname = $sub->subname;
        if (my $merged_sub_names = $sub->meta->{merged_sub_names}) {
            push @hints, sprintf "merge of %d subs", 1+scalar @$merged_sub_names;
        }
        my ($pkg, $subr) = ($subname =~ /^(.*::)(.*?)$/) ? ($1, $2) : ('', $subname);

        # remove OWN filename from eg __ANON__[(eval 3)[/long/path/name.pm:99]:53]
        #                     becomes __ANON__[(eval 3)[:99]:53]
        # XXX doesn't work right if $filestr isn't full filename
        $subr =~ s/\Q$filestr\E:(\d+)/:$1/g if $filestr;
        # remove @INC prefix from other paths
        $subr =~ s/$inc_path_regex//;    # for __ANON__[/very/long/path...]

        $sub_links .= qq{<td class="sub_name">};
        # hidden span is for tablesorter to sort on
        $sub_links .= sprintf(qq{<span style="display: none;">%s::%s</span>}, $pkg, $subr);

        if ($sub->is_xsub) {
            my $is_opcode = ($pkg eq 'CORE' or $subr =~ /^CORE:/);
            unshift @hints, ($is_opcode) ? 'opcode' : 'xsub';
        }
        if (my $recdepth = $sub->recur_max_depth) {
            unshift @hints, sprintf "recurses: max depth %d, inclusive time %s",
                $recdepth, fmt_time($sub->recur_incl_time);
        }

        $sub_links .= sprintf qq{%*s<a %s>%s</a>%s</td>},
            $max_pkg_name_len+2, $pkg,
            $reporter->href_for_sub($subname),
            $subr,
            (@hints) ? "&nbsp;(".join("; ",@hints).")" : "";

        $sub_links .= "</tr>\n";
    }
    $sub_links .= q{</tbody>};
    $sub_links .= q{</table>};

    # make table sortable if it contains all the subs
    push @on_ready_js, q<
        $("#subs_table").tablesorter({
            sortList: [[3,1]],
            headers: {
                3: { sorter: 'fmt_time' },
                4: { sorter: 'fmt_time' }
            }
        });
        $(".floatHeaders").each( function(){ $(this).floatThead(); } );

        show_fragment_target();
        $(window).on('hashchange', function(e){
          show_fragment_target();
        });

    > if @subs_to_show == @subs;

    return $sub_links;
}


$reporter->set_param(
    'datastart',
    sub {
        my ($profile, $fi) = @_;
        my $filestr = $fi->filename;

        my $sub_table = subroutine_table($profile, $fi, undef, undef);

        if ($sub_table and not $opt_minimal) {
            my $dot_file = html_safe_filename($filestr) . ".dot";

            $sub_table .= qq{
                Call graph for these subroutines as a
                <a href="http://en.wikipedia.org/wiki/Graphviz">Graphviz</a>
                <a href="$dot_file">dot language file</a>.
            };

            our %dot_file_generated;
            if ($dot_file_generated{$dot_file}++) { # just once for line/block/sub
                my $subs_in_file = $profile->subs_defined_in_file($filestr, 0);
                # include subs defined in this file
                # and/or called from subs defined in this file
                #warn "$dot_file: @{[ keys %$subs_in_file ]}\n";
                my $sub_filter = sub {
                    my ($si, $calledby) = @_;
                    return 1 if not defined $calledby;
                    my $subname = $si->subname;
                    my $include = ($subs_in_file->{$subname}
                                || $subs_in_file->{$calledby});
                    #warn "Call graph $subname<-$calledby: ".($include ? "SHOW" : "skip")."\n";
                    return $include;
                };
                output_subs_callgraph_dot_file($reporter, $dot_file, $sub_filter, 0);
            }
        }

        return qq{
        $sub_table
      <table border="1" cellpadding="0" class="floatHeaders">
      <thead>
      <tr><th>Line</th>
      <th><span title="Number of statements executed">State<br />ments</span></th>
      <th><span title="Time spend executing statements on the line,
        excluding time spent executing statements in any called subroutines">Time<br />on line</span></th>
      <th><span title="Number of subroutines calls">Calls</span></th>
      <th><span title="Time spent in subroutines called (inclusive)">Time<br />in subs</span></th>
      <th class="left_indent_header">Code</th>
      </tr>\n
      </thead>
      <tbody>
    };
    }
);

$reporter->set_param( footer => sub {
    my ($profile, $fi) = @_;
    my $footer = get_footer($profile);
    return "</tbody></table></div>$footer</body></html>";
} );

$reporter->set_param(mk_report_source_line => \&mk_report_source_line);
$reporter->set_param(mk_report_xsub_line   => \&mk_report_xsub_line  );
$reporter->set_param(mk_report_separator_line => \&mk_report_separator_line  );

sub mk_report_source_line {
    my ($linenum, $line, $stats_for_line, $stats_for_file, $profile, $fi) = @_;

    my $l = sprintf(qq{<td class="h"><a name="%s"></a>%s</td>}, $linenum, $linenum);
    my $s = report_src_line(undef, $linenum, $line, $profile, $fi, $stats_for_line);

    return "<tr>$l<td></td><td></td><td></td><td></td>$s</tr>\n"
        if not %$stats_for_line;

    return join "",
        "<tr>$l",
        determine_severity($stats_for_line->{'calls'},     $stats_for_file->{'calls'}),
        determine_severity($stats_for_line->{'time'},      $stats_for_file->{'time'}, 1,
            \sprintf("Avg %s", fmt_time($stats_for_line->{'time/call'})||'--' )),
        determine_severity($stats_for_line->{'subcall_count'}, $stats_for_file->{subcall_count}, 0),
        determine_severity($stats_for_line->{'subcall_time'},  $stats_for_file->{subcall_time}, 1),
        $s, "</tr>\n";
}

sub mk_report_xsub_line {
    my ($subname, $line, $stats_for_line, $stats_for_file, $profile, $fi) = @_;
    (my $anchor = $subname) =~ s/\W/_/g;
    return join "",
        sprintf(qq{<tr><td class="h"><a name="%s"></a>%s</td>}, $anchor, ''),
        "<td></td><td></td><td></td><td></td>",
        report_src_line(undef, undef, $line, $profile, $fi, $stats_for_line),
        "</tr>\n";
}

sub mk_report_separator_line {
    my ($profile, $fi) = @_;
    return join "",
        sprintf(qq{<tr><td class="s"><a name="%s"></a>%s</td>}, '', '&nbsp;'),
        "<td></td><td></td><td></td><td></td>",
        '<td class="s"></td>',
        "</tr>\n";
}


sub _escape_html {
    local $_ = shift;
    s/\t/        /g; # XXX incorrect for most non-leading tabs
    s/&/&amp;/g;
    s/</&lt;/g;
    s/>/&gt;/g;
    s{\n}{<br />}g;  # for xsub pseudo-sub declarations
    s{"}{&quot;}g;   # for attributes like title="..."
    return $_;
}


sub report_src_line {
    my ($value, undef, $linesrc, $profile, $fi, $stats_for_line) = @_;

    $linesrc = _escape_html($linesrc);

    our $inc_path_regex ||= get_abs_paths_alternation_regex([$profile->inc]);

    my @prologue;

    # for each of the subs defined on this line, who called them
    my $subdef_info = $stats_for_line->{subdef_info} || [];
    for my $sub_info (@$subdef_info) {
        my $callers = $sub_info->caller_fid_line_places;
        next unless $callers && %$callers;
        my $subname = $sub_info->subname;

        my @callers;
        while (my ($fid, $fid_line_info) = each %$callers) {
            for my $line (keys %$fid_line_info) {
                my $sc = $fid_line_info->{$line};
                warn "$linesrc $subname caller info missing" if !@$sc;
                next if !@$sc;
                push @callers, [ $fid, $line, @$sc ];
            }
        }
        my $total_calls = sum(my @caller_calls = map { $_->[2] } @callers);

        push @prologue, sprintf "# spent %s within %s which was called%s:",
            fmt_incl_excl_time($sub_info->incl_time, $sub_info->excl_time),
            $subname,
            ($total_calls <= 1) ? ""
                : sprintf(" %d times, avg %s/call",
                    $total_calls, fmt_time($sub_info->incl_time / $total_calls));
        push @prologue, sprintf "# (data for this subroutine includes %d others that were merged with it)",
                scalar @{$sub_info->meta->{merged_sub_names}}
            if $sub_info->meta->{merged_sub_names};
        my $max_calls = max(@caller_calls);

        # order by most frequent caller first, then by time
        @callers = sort { $b->[2] <=> $a->[2] || $b->[3] <=> $a->[3] } @callers;

        for my $caller (@callers) {
            my ($fid, $line, $count, $incl_time, $excl_time, undef, undef,
                undef, undef, $calling_subs) = @$caller;

            my @subnames = sort keys %{$calling_subs || {}};
            my $subname = (@subnames) ? " by " . join(" or ", @subnames) : "";

            my $caller_fi = $profile->fileinfo_of($fid);
            if (!$caller_fi) { # should never happen
                warn sprintf "Caller of %s, from fid %d line %d has no fileinfo (%s)",
                    $sub_info, $fid, $line, $subname;
                    die 2;
                next;
            }

            my $avg_time = "";
            $avg_time = sprintf ", avg %s/call", fmt_time($incl_time / $count)
                if $count > 1;
            my $times = sprintf " (%s+%s)", fmt_time($excl_time),
                fmt_time($incl_time - $excl_time);

            my $filename = $caller_fi->filename($fid);
            my $line_desc = "line $line of $filename";
            $line_desc =~ s/ of \Q$filename\E$//g if $filename eq $fi->filename;
            # remove @INC prefix from paths
            $line_desc =~ s/$inc_path_regex//g;

            my $href = $reporter->href_for_file($caller_fi, $line);
            push @prologue,
                sprintf q{# %*s times%s%s at <a %s>%s</a>%s},
                length($max_calls), $count, $times, $subname, $href,
                $line_desc, $avg_time;
            $prologue[-1] =~ s/^(# +)1 times/$1   once/;  # better English
        }
    }
    my $prologue = '';
    $prologue = sprintf qq{<div class="calls"><div class="calls_in">%s</div></div>}, join("\n", @prologue)
        if @prologue;

    my $epilogue = '';
    my $ws;

    # give details of each of the subs called by this line
    my $subcall_info = $stats_for_line->{subcall_info};
    if ($subcall_info && %$subcall_info) {

        my @calls_to = sort {
            $subcall_info->{$b}[1] <=> $subcall_info->{$a}[1] or    # incl_time
                $a cmp $b
        } keys %$subcall_info;
        my $max_calls_to = max(map { $_->[0] } values %$subcall_info);
        $ws ||= ($linesrc =~ m/^((?:&nbsp;|\s)+)/) ? $1 : '';

        my $subs_called_html = join "\n", map {
            my $subname = $_;
            my ($count, $incl_time, $reci_time, $rec_depth) = (@{$subcall_info->{$subname}})[0,1,5,6];
            my $html = sprintf qq{%s# spent %s making %*d call%s to }, $ws,
                fmt_time($incl_time+$reci_time, 5), length($max_calls_to),
                $count, $count == 1 ? "" : "s";
            (my $subname_trimmed = $subname) =~ s/$inc_path_regex//g;
            $html .= sprintf qq{<a %s>%s</a>}, $reporter->href_for_sub($subname), $subname_trimmed;
            $html .= sprintf qq{, avg %s/call}, fmt_time(($incl_time+$reci_time) / $count),
                if $count > 1;
            if ($rec_depth) {
                $html .= sprintf qq{, recursion: max depth %d, sum of overlapping time %s},
                    $rec_depth, fmt_time($reci_time);
            }
            $html;
        } @calls_to;
        $epilogue .= sprintf qq{<div class="calls"><div class="calls_out">%s</div></div>}, $subs_called_html;
    }

    # give details of each of the string evals executed on this line
    my $evals_called = $stats_for_line->{evalcall_info};
    if ($evals_called && %$evals_called) {
        $ws ||= ($linesrc =~ m/^((?:&nbsp;|\s)+)/) ? $1 : '';

        my @eval_fis = sort {
            $b->sum_of_stmts_time(1) <=> $a->sum_of_stmts_time(1) or
            $a->filename cmp $b->filename
        } values %$evals_called;

        my $evals_called_html = join "\n", map {
            my $eval_fi = $_;
            my $sum_of_stmts_time = $eval_fi->sum_of_stmts_time;
            my ($what, $extra) = ("string eval", "");

            my $merged_fids = $eval_fi->meta->{merged_fids};
            if ($merged_fids) {
                $what = sprintf "%d string evals (merged)", 1+@$merged_fids;
            }

            my @nested_evals = $eval_fi->has_evals(1);
            my $nest_eval_time = 0;
            if (@nested_evals) {
                $nest_eval_time = sum map { $_->sum_of_stmts_time } @nested_evals;
                $extra .= sprintf ", %s here plus %s in %d nested evals",
                        fmt_time($sum_of_stmts_time), fmt_time($nest_eval_time),
                        scalar @nested_evals
                    if $nest_eval_time;
            }

            if (my @subs_defined = $eval_fi->subs_defined(1)) {
                my $sub_count  = @subs_defined;
                my $call_count = sum map { $_->calls } @subs_defined;
                my $excl_time  = sum map { $_->excl_time } @subs_defined;
                $extra .= sprintf "<br />%s# includes %s spent executing %d call%s to %d sub%s defined therein.",
                        $ws, fmt_time($excl_time, 2),
                        $call_count, ($call_count != 1) ? 's' : '',
                        $sub_count,  ($sub_count  != 1) ? 's' : ''
                    if $call_count;
            }

            my $link = sprintf(q{<a %s>%s</a>}, $reporter->href_for_file($eval_fi), $what);
            my $html = sprintf qq{%s# spent %s executing statements in %s%s},
                $ws, fmt_time($sum_of_stmts_time+$nest_eval_time, 5),
                $link, $extra;

            $html;
        } @eval_fis;

        $epilogue .= sprintf qq{<div class="calls"><div class="calls_out">%s</div></div>}, $evals_called_html;
    }

    return qq{<td class="s">$prologue$linesrc$epilogue</td>};
}


# set output options
$reporter->set_param('suffix', '.html');

# output a css file too (optional, but good for pretty pages)
$reporter->_output_additional('style.css', get_css());

# generate the files
$reporter->report();

output_subs_index_page($reporter, "index-subs-excl.html", 'excl_time');
output_index_page($reporter, "index.html");

output_js_files($reporter);

open_browser_on("$opt_out/index.html") if $opt_open;

exit 0;

#
# SUBROUTINES
#

# output an html indexing page or subroutines
sub output_subs_index_page {
    my ($r, $filename, $sortby) = @_;
    my $profile = $reporter->{profile};

    open my $fh, '>', "$opt_out/$filename"
        or croak "Unable to open file $opt_out/$filename: $!";

    print $fh get_html_header("Subroutine Index - NYTProf");
    print $fh get_page_header(profile => $profile, title => "Performance Profile Subroutine Index");
    print $fh qq{<div class="body_content"><br />};

    # Show top subs across all files
    print $fh subroutine_table($profile, undef, 0, $sortby);

    my $footer = get_footer($profile);
    print $fh "</div>$footer</body></html>";
    close $fh;
}


# output an html indexing page with some information to help navigate potential
# large numbers of profiled files. Optional, recommended
sub output_index_page {
    my ($r, $filename) = @_;
    my $profile = $reporter->{profile};

    ###
    open my $fh, '>', "$opt_out/$filename"
        or croak "Unable to open file $opt_out/$filename: $!";

    my $application = $profile->{attribute}{application};
    (my $app = $application) =~ s:.*/::; # basename
    $app =~ s/ .*//;

    print $fh get_html_header("NYTProf $app");
    print $fh get_page_header(profile => $profile, title => "Performance Profile Index", skip_link_to_index=>1);
    print $fh qq{
        <div class="body_content"><br />
    };

    # overall description
    my @all_fileinfos = $profile->all_fileinfos;
    my $eval_fileinfos = $profile->eval_fileinfos;
    my $summary = sprintf "Profile of %s for %s (of %s),", $application,
        fmt_time($profile->{attribute}{profiler_active}),
        fmt_time($profile->{attribute}{profiler_duration});
    $summary .= " executing";
    $summary .= sprintf " %d statements and",
         $profile->{attribute}{total_stmts_measured}
        -$profile->{attribute}{total_stmts_discounted}
        if $profile->{option}{stmts};
    $summary .= sprintf " %d subroutine calls",
         $profile->{attribute}{total_sub_calls};
    $summary .= sprintf " in %d source files",
        @all_fileinfos - $eval_fileinfos;
    $summary .= sprintf " and %d string evals",
        $eval_fileinfos if $eval_fileinfos;
    printf $fh qq{<div class="index_summary">%s.</div>}, _escape_html($summary);

    # generate name-sorted select options for files, if there are many
    if ($profile->noneval_fileinfos > 30) {
        print $fh qq{<div class="jump_to_file"><form name="jump">};
        print $fh qq{<select name="file" onChange="location.href=document.jump.file.value;">\n};
        printf $fh qq{<option disabled="disabled">%s</option>\n}, "Jump to file...";
        foreach my $fi (sort { $a->filename cmp $b->filename } $profile->noneval_fileinfos) {
            printf $fh qq{<option value="#f%s">%s</option>\n},
                $fi->fid, _escape_html($fi->filename);
        }
        print $fh "</select></form></div>\n";
    }

    my $call_stacks_file = "all_stacks_by_time.calls";
    my $call_stacks_svg  = "all_stacks_by_time.svg";
    if ($profile->{option}{calls} && $opt_flame) {
        my $mk_flamegraph = sub {

            my $total_sub_calls = $profile->{attribute}{total_sub_calls};
            my $is_big = ($total_sub_calls <= 1_000_000);
            warn sprintf "Extracting subroutine call data%s ...\n",
                ($is_big) ? "" : " (There were $total_sub_calls of them, so this may take some time, or cancel and use --no-flame to skip this step.)";
            system("\"$nytprofcalls\" $opt_file > $opt_out/$call_stacks_file") == 0
                or die "Generating $opt_out/$call_stacks_file failed\n";

            my %subname_subinfo_map = %{ $profile->subname_subinfo_map };
            warn "Extracting subroutine links\n";
            my $subattr = "$opt_out/flamegraph_subattr.txt";
            open my $subattrfh, ">", $subattr
                or die "Error creating $subattr: $!\n";
            while ( my ($subname, $si) = each %subname_subinfo_map ) {
                next unless $si->incl_time;
                print $subattrfh join("\t", $subname,
                    q{href=}.$reporter->url_for_sub($subname),
                )."\n";
            }
            close $subattrfh or die "Error writing $subattr: $!\n";

            warn "Generating subroutine stack flame graph ...\n";
            # factor to scale the values to microseconds
            my $factor = 1_000_000 / $profile->{attribute}{ticks_per_sec};
            # total (width) for flamegraph is profiler_active in ticks
            my $run_us = $profile->{attribute}{profiler_active} * $profile->{attribute}{ticks_per_sec};
            system("\"$flamegraph\" --nametype=sub --countname=microseconds --factor=$factor --nameattr=$subattr --hash --total=$run_us $opt_out/$call_stacks_file > $opt_out/$call_stacks_svg") == 0
                or die "Generating $opt_out/$call_stacks_svg failed\n";

            print $fh qq{<div class="flamegraph">\n};
            print $fh qq{<object data="$call_stacks_svg" width="1200" type="image/svg+xml" >SVG not supported</object>\n};
            print $fh qq{<p>The <a href="http://dtrace.org/blogs/brendan/2011/12/16/flame-graphs/">Flame Graph</a> above is a visualization of the time spent in <em>distinct call stacks</em>. The colors and x-axis position are not meaningful.</p>\n};
            print $fh qq{</div>\n};
            1;
        };
        eval { $mk_flamegraph->() }
            or warn $@;
    }

    # Show top subs across all files
    my $max_subs = 15; # keep it less than a page so users can see the file table
    my $all_subs = keys %{$profile->{sub_subinfo}};
    print $fh subroutine_table($profile, undef, $max_subs, undef);
    if ($all_subs > $max_subs) {
        print $fh sprintf qq{<div class="table_footer">
            See <a href="%s">all %d subroutines</a>
            </div>
        }, "index-subs-excl.html", $all_subs;
    }

    if ($has_json) {
        output_subs_treemap_page($reporter, "subs-treemap-excl.html",
            "Subroutine Exclusive Time Treemap", sub { shift->excl_time });
        print $fh q{<br/>You can view a <a href="subs-treemap-excl.html">treemap of subroutine exclusive time</a>, grouped by package.<br/>};
    }
    else {
        print $fh q{<br/>(Can't create visual treemap of subroutine exclusive times without the <a href="http://metacpan.org/release/JSON-MaybeXS/">JSON::MaybeXS</a> module.)<br/>};
    }

    if (not $opt_minimal) {
        output_subs_callgraph_dot_file($reporter, "packages-callgraph.dot", undef, 1);
        print $fh q{NYTProf also generates call-graph files in }
            .q{<a href="http://en.wikipedia.org/wiki/Graphviz">Graphviz</a> format: }
            .q{<a href="packages-callgraph.dot">inter-package calls</a>};
        output_subs_callgraph_dot_file($reporter, "subs-callgraph.dot", undef, 0);
        print $fh q{, <a href="subs-callgraph.dot">all inter-subroutine calls</a>};
        print $fh q{ (probably too complex to render easily)}
            if $all_subs > 200; # arbitrary
        print $fh q{.<br/>};
    }

    print $fh q{<br/>You can hover over some table cells and headings to view extra information.};
    print $fh q{<br/>Some table column headings can be clicked on to sort the table by that column.};
    print $fh q{<br/>};

    output_file_table($fh, $profile, 1);

    my $footer = get_footer($profile);
    print $fh "</div>$footer</body></html>";
    close $fh;
}


# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
# treemap subs

sub js_for_new_treemap {
    my ($name, $new_args, $tree_data) = @_;

    return '' unless $has_json;

    my $default_new_args = {
        titleHeight => 0, # no titles
        addLeftClickHandler => 1,   # zoom in
        #addRightClickHandler => 1, # zoom out (XXX but disables right click menu)
        offset => 0, # (0/2/4) extra padding around nested levels

        Color => {
            allow => 1,  
            # value range for the $color property
            minValue => 0,  
            maxValue => scalar @treemap_colors,  
            # corresponding color range [R,G,B]:
            minColorValue => [0, 255, 50],  
            maxColorValue => [255, 0, 50],
        },

        Tips => {  
            allow => 1,
            offsetX => 20,  
            offsetY => 20,  
        },

        selectPathOnHover => 1, # adds "over-<foo>" css class to elements
    };
    exists $new_args->{$_} or $new_args->{$_} = $default_new_args->{$_}
        for keys %$default_new_args;

    my $new_args_json  = encode_json($new_args);
    my $tree_data_json = encode_json($tree_data);

    my $js = qq{
        function init_$name() {
            var tm_args = $new_args_json;

            //This method is invoked when a DOM element is created.  
            //Its useful to set DOM event handlers here or manipulate  
            //the DOM Treemap nodes.  
            tm_args.onCreateElement = function(content, tree, isLeaf, leaf){  
                //Add background image for cushion effect
                if(isLeaf) { 
                    var style = leaf.style,   
                    width = parseInt(style.width) - 2,   
                    height = parseInt(style.height) - 2;  
                    // don't add gradient if too small to be worth the cost
                    if (width < 10 || height < 10) {   // is narrow
                        if (width < 50 && height < 50) // is small
                            return;
                    }
                    leaf.innerHTML = tree.name +   
                        "<img src=\\"js/jit/gradient-cushion1.png\\" " +  
                        " style=\\"position:absolute;top:0;left:0;width:" +   
                        width+"px;height:" + height+"px;\\" />";  
                    style.width = width + "px";  
                    style.height = height + "px";  
                }  
            };

            // add content to the tooltip when a node is hovered  
            // move to separate function later
            tm_args.Tips.onShow = function(tip, node, isLeaf, domElement) {
                tip.innerHTML = node.data.tip;
            };

            TM.Squarified.implement({
                'onLeftClick': function(elem) { // zoom in one level
                    //if is leaf
                    var node = TreeUtil.getSubtree(this.tree, elem.parentNode.id);
                    if(node.children && node.children.length == 0) {
                        var oldparent = node, newparent = node;
                        while(newparent.id != this.shownTree.id) {
                            oldparent = newparent;
                            newparent = TreeUtil.getParent(this.tree, newparent.id);
                        }
                        this.view(oldparent.id);
                    } else {
                      this.enter(elem);
                    }
                }
            });

            TM.Squarified.implement({
                createBox: function(json, coord, html) {
                    if((coord.width * coord.height > 1) && json.data.\$area > 0) {
                        if(!this.leaf(json))
                            var box = this.headBox(json, coord) + this.bodyBox(html, coord);
                        else
                            var box = this.leafBox(json, coord);
                        return this.contentBox(json, coord, box);
                    } else {
                        return ""; //return empty string
                    }
                }
            });

            var $name = new TM.Squarified(tm_args);

            var json = $tree_data_json;

            $name.loadJSON(json);
        }
    };
    return $js;
}


sub pl {    # dumb but sufficient pluralization
    my ($fmt, $n) = @_;
    sprintf $fmt.($n == 1 ? "" : "s"), $n;
}


sub package_subinfo_map_to_tm_data {
    my ($package_tree_subinfo_map, $area_sub) = @_;

    my $sub_tip_html = sub {
        my $si = shift;
        my @html;
        push @html, sprintf "<p><b>%s</b></p><p>", $si->subname;

        push @html, sprintf "Called %s from %s in %s",
            pl("%d time", $si->calls),
            pl("%d place", scalar $si->caller_places),
            pl("%d file", scalar $si->caller_fids);

        my $total_time = $si->profile->{attribute}{profiler_duration};
        my $incl_time = $si->incl_time;
        push @html, sprintf "Inclusive time: %s, %.2f%%",
                fmt_time($incl_time), $total_time ? $incl_time/$total_time*100 : 0;
        my $excl_time = $si->excl_time;
        push @html, sprintf "Exclusive time: %s, %.2f%%",
                fmt_time($excl_time), $total_time ? $excl_time/$total_time*100 : 0
            if $excl_time ne $incl_time;

        if (my $mrd = $si->recur_max_depth) {
            push @html, sprintf "Recursion: max depth %d, recursive inclusive time %s",
                $mrd, fmt_time($si->recur_incl_time);
        }

        return join("<br />", @html)."</p>";
    };

    my $leaf_data_sub = sub {
        my ($subinfo, $area_from, $color) = @_;
        my $data = {
            '$area' => $area_from->($subinfo),
            '$color' => $color,
            tip => $sub_tip_html->($subinfo),
            map({ $_ => $subinfo->$_() }
                qw(subname incl_time excl_time))
        };
        return $data;
    };

    our $nid;
    my $node_mapper;
    $node_mapper = sub {
        my ($k, $v, $title) = @_;
        $title = ($title) ? '::'.$k : $k;

        my $n = {
            id => "n".++$nid,
            name => $title,
        };

        my @kids;
        for my $pkg_elem (keys %$v) {
            my $infos = $v->{$pkg_elem};

            if (ref $infos eq 'HASH') { # recurse into subpackages
                push @kids, $node_mapper->($pkg_elem, $infos, $title);
                next;
            }

            # subs within this package
            our $color_seqn; # all subs in pkg get same color
            my $color = $treemap_colors[ $color_seqn++ % @treemap_colors ];

            for my $info (@$infos) {

                # don't bother including subs that don't have any data
                # (unless we've not got any subs yet, to avoid problems elsewhere)
                next if $area_sub->($info) <= 0;

                push @kids, {
                    id => ++$nid."-".$info->subname,
                    name => $info->subname_without_package,
                    data => $leaf_data_sub->($info, $area_sub, $color),
                    children => [],
                };
            }
        }

        $n->{data}{'$area'} = (@kids) ? sum(map { $_->{data}{'$area'} } @kids) : 0
            if not defined $n->{data}{'$area'};
        $n->{children} = \@kids;

        return $n;
    };

    return $node_mapper->('', $package_tree_subinfo_map, '');
}


sub output_treemap_code {
    my (%spec) = @_;
    my $fh = $spec{fh};
    my $tm_id = 'tm'.$spec{id};
    my $root_id = 'infovis'.$spec{id};

    my $treemap_data = $spec{get_data}->();
    $treemap_data->{name} = $spec{title} if $spec{title};

    my $tm_js = js_for_new_treemap($tm_id, { rootId => $root_id }, $treemap_data);
    print $fh qq{<script type="text/javascript">$tm_js\n</script>\n};

    push @on_ready_js, qq{init_$tm_id(); };
    return $root_id;
}


sub output_subs_treemap_page {
    my ($r, $filename, $title, $area_sub) = @_;
    my $profile = $reporter->{profile};

    open(my $fh, '>', "$opt_out/$filename")
        or croak "Unable to open file $opt_out/$filename: $!";

    $title ||= "Subroutine Time Treemap";
    print $fh get_html_header("$title - NYTProf", { add_jit => "Treemap" });
    print $fh get_page_header( profile => $profile, title => $title);

    my @specs;
    push @specs, {
        id => 1,
        title => "Treemap of subroutine exclusive time",
        get_data => sub {
            package_subinfo_map_to_tm_data(
                $profile->package_subinfo_map(0,1),
                $area_sub || sub { shift->excl_time }, 0);
        }
    };

    my @root_ids;
    for my $spec (@specs) {
        push @root_ids, output_treemap_code(
            fh => $fh,
            profile => $profile,
            %$spec
        );
    }

    print $fh qq{<div class="vis_header"><br/>Boxes represent time spent in a subroutine. Coloring represents packages. Click to drill-down into package hierarchy, reload page to reset.</div>\n};
    print $fh qq{<div id="infovis">\n};
    print $fh qq{<br /><div id="$_"></div>\n} for @root_ids;
    print $fh qq{</div>\n};

    my $footer = get_footer($profile);
    print $fh "$footer</body></html>";
    close $fh;
}


# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

sub output_subs_callgraph_dot_file {
    my ($r, $filename, $sub_filter, $only_show_packages) = @_;
    my $profile = $reporter->{profile};
    my $subinfos = $profile->subname_subinfo_map;

    my $dot_file = "$opt_out/$filename";
    open my $fh, '>', $dot_file
        or croak "Unable to open file $dot_file: $!";

    my $inc_path_regex = get_abs_paths_alternation_regex([$profile->inc], qr/^|\[/);
    my $dotnode = sub {
        my $name = shift;
        $name =~ s/$inc_path_regex//;
        $name =~ s/"/\\"/g;
        return '"'.$name.'"';
    };

    print $fh "digraph {\n"; # }
    print $fh "graph [overlap=false]\n"; # target="???", URL="???"

    # gather link info
    my %sub2called_by;
    for my $subname (keys %$subinfos) {
        my $si = $subinfos->{$subname};
        next unless $si->calls; # skip subs never called

        next if $sub_filter and not $sub_filter->($si, undef);

        my $called_by_subnames = $si->called_by_subnames;
        if (!%$called_by_subnames) { 
            warn sprintf "%s has no caller subnames but a call count of %d\n",
                    $subname, $si->calls;
            next;
        }

        if ($sub_filter) {
            my @delete = grep { !$sub_filter->($si, $_) } keys %$called_by_subnames;
            if (@delete) {
                # shallow copy so we can edit it safely
                $called_by_subnames = { %$called_by_subnames };
                delete @{$called_by_subnames}{@delete};
            }
            next if !keys %$called_by_subnames;
        }

        $sub2called_by{$subname} = $called_by_subnames;
    }

    # list of all subs to be included in graph (has duplicates)
    my %pkg_subs;
    for (keys %sub2called_by, map { keys %$_ } values %sub2called_by) {
        m/^(.*)::(.*)?$/ or warn "Strange sub name '$_'";
        $pkg_subs{$1}{$_} = $sub2called_by{$_} || {};
    }

#stmt : node_stmt | edge_stmt | attr_stmt | ID '=' ID | subgraph
#attr_stmt : (graph | node | edge) attr_list
#attr_list : '[' [ a_list ] ']' [ attr_list ]
#a_list : ID [ '=' ID ] [ ',' ] [ a_list ]
#subgraph : [ subgraph [ ID ] ] '{' stmt_list '}'

    if ($only_show_packages) {
        my %once;
        # XXX many shapes cause v.large graphs with nodes v.far apart
        # when using neato (energy minimized) possibly a neato bug
        # some shapes, like doublecircle seem to avoid the problem.
        print $fh "node [shape=doublecircle];\n";
        while ( my ($pkg, $subs) = each %pkg_subs ) {
            my @called_by = map { keys %$_ } values %$subs;

            for my $called_by (@called_by) {
                (my $called_by_pkg = $called_by) =~ s/^(.*)::.*?$/$1/;
                my $link = sprintf qq{%s -> %s;\n},
                    $dotnode->("$called_by_pkg"), $dotnode->("$pkg");
                $once{$link} = 1;
            }

        }
        print $fh $_ for keys %once;

    }
    else {

        # output nodes and gather link info
        while ( my ($pkg, $pkg_subs) = each %pkg_subs) {
            (my $pkgmangled = $pkg) =~ s/\W+/_/g;

            # node_stmt: node_id [ attr_list ]
            printf $fh "subgraph cluster_%s {\n", $pkgmangled; # }
            printf $fh "\tlabel=%s;\n", $dotnode->($pkg);

            for my $subname (keys %$pkg_subs) {
                # node_stmt: node_id [ attr_list ]
                #printf $fh qq{\tnode [ %s ]}, ...
                printf $fh qq{\t%s;\n}, $dotnode->($subname);
            }

            # { - just to balance the brace below
            printf $fh "}\n";
        }

        while ( my ($subname, $called_by_subnames) = each %sub2called_by ) {

            for my $called_by (keys %$called_by_subnames) {
                # edge_stmt : (node_id | subgraph) edgeRHS [ attr_list ]
                # edgeRHS   : edgeop (node_id | subgraph) [ edgeRHS ]
                printf $fh qq{%s -> %s;\n},
                    $dotnode->($called_by), $dotnode->($subname);
            }

        }
    }

    print $fh "}\n";

    close $fh;
    #system("open '$dot_file'"); die 1;

    return;
}

# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

sub output_js_files {
    my ($profile) = @_;
    # find the js, gif, css etc files installed with Devel::NYTProf
    (my $lib = $INC{"Devel/NYTProf/Data.pm"}) =~ s/\/Data\.pm$//;
    _copy_dir("$lib/js", "$opt_out/js");
}

sub _copy_dir {
    my ($srcdir, $dstdir) = @_;
    mkdir $dstdir or die "Can't create $dstdir directory: $!\n"
        unless -d $dstdir;
    for my $src (glob("$srcdir/*")) {
        (my $name = $src) =~ s{.*/}{};
        next if $name =~ m/^\./; # skip . and .. etc
        my $dstname = "$dstdir/$name";
        if (not -f $src) {
            _copy_dir($src, $dstname) if -d $src; # recurse
            next; # skip non-ordinary-files
        }
        unlink $dstname;
        copy($src, $dstname)
            or warn "Unable to copy $src to $dstname: $!";
    }
}


sub open_browser_on {
    my $index = shift;

    return if eval { require Browser::Open; Browser::Open::open_browser($index, 1); };
    warn "$@\n" if $@ && $opt_debug;

    return if eval { require ActiveState::Browser; ActiveState::Browser::open($index); 1 };
    warn "$@\n" if $@ && $opt_debug && $^O eq "MSWin32";


    my $BROWSER;
    if ($^O eq "MSWin32") {
        $BROWSER = "start %s";
    }
    elsif ($^O eq "darwin") {
        $BROWSER = "/usr/bin/open %s";
    }
    else {
        my @try;
        if ($ENV{BROWSER}) {
            push(@try, split(/:/, $ENV{BROWSER}));
        }
        else {
            push(@try, qw(firefox galeon mozilla opera netscape));
        }
        unshift(@try, "kfmclient") if $ENV{KDE_FULL_SESSION};
        unshift(@try, "gnome-open") if $ENV{GNOME_DESKTOP_SESSION_ID};
        unshift(@try, "xdg-open");
        for (grep { have_prog($_) } @try) {
            if ($_ eq "kfmclient") {
                $BROWSER = "$_ openURL %s";
            }
            elsif ($_ eq "gnome-open" || $_ eq "opera") {
                $BROWSER = "$_ %s";
            }
            else {
                $BROWSER = "$_ %s &";
            }
            last;
        }
    }
    if ($BROWSER) {
        (my $cmd = $BROWSER) =~ s/%s/"$index"/;
        warn "Running $cmd\n" if $opt_debug;
        system($cmd);
    }
    else {
        warn "Don't know how to invoke your web browser.\nPlease visit $index yourself!\n";
    }
}


sub have_prog {
    my $prog = shift;
    for (split($Config{path_sep}, $ENV{PATH})) {
        return 1 if -x "$_/$prog";
    }
    return 0;
}


sub output_file_table {
    my ($fh, $profile, $add_totals) = @_;

    # generate time-sorted sections for files
    print $fh qq{
        <table id="filestable" border="1" cellspacing="0" class="tablesorter floatHeaders">
        <caption>Source Code Files &mdash; ordered by exclusive time then name</caption>
    };
    print $fh qq{
        <thead><tr class="index">
        <th>Stmts</th><th>Exclusive<br />Time</th>
        <th>Reports</th><th>Source File</th>
        </tr></thead>
        <tbody>
    };

    my $inc_path_regex = get_abs_paths_alternation_regex([$profile->inc], qr/^|\[/);

    my $allTimes = $profile->{attribute}{total_stmts_duration};
    my $allCalls = $profile->{attribute}{total_stmts_measured}
                 - $profile->{attribute}{total_stmts_discounted};
    # file in which sawampersand was noted during profiling
    my $sawampersand_fi = $profile->fileinfo_of($profile->{attribute}{sawampersand_fid}, 1);

    my (@t_stmt_exec, @t_stmt_time);
    my @fis = $profile->noneval_fileinfos;
    @fis = sort { $b->meta->{'time'} <=> $a->meta->{'time'} } @fis;

    my $dev_time = calculate_median_absolute_deviation([map { scalar $_->meta->{'time'} } @fis], 1);
    
    foreach my $fi (@fis) {
        my $meta = $fi->meta;
        my $fid = $fi->fid;
        my @extra;
        my $css_class = 'index';

        # The stats in this table include rolled up sums of nested evals.

        my ($eval_stmts, $eval_time) = (0,0);
        if (my @has_evals = $fi->has_evals(1)) {
            my $n_evals = scalar @has_evals;
            my $msg = sprintf "including %d string eval%s", $n_evals, ($n_evals>1) ? "s" : "";
            if (my @nested = grep { $_->eval_fid != $fid } @has_evals) {
                $msg .= sprintf ": %d direct plus %d nested",
                    $n_evals-@nested, scalar @nested;
            }
            push @extra, $msg;
            $eval_stmts = sum(map { $_->sum_of_stmts_count } @has_evals);
            $eval_time  = sum(map { $_->sum_of_stmts_time  } @has_evals);
        }
        # is this file one where we sawampersand (or contains an eval that is)?
        if ($sawampersand_fi
            && $] < 5.017008
            && $fi == ($sawampersand_fi->outer || $sawampersand_fi)
        ) {
            my $in_eval = ($fi == $sawampersand_fi)
                ? 'here'
                : sprintf q{<a %s>in eval here</a>}, $reporter->href_for_file($sawampersand_fi, undef, 'line');
            push @extra, sprintf qq{variables that impact regex performance for whole application seen $in_eval},
            $css_class = "warn $css_class";
        }

        print $fh qq{<tr class="$css_class">};

        my $stmts = $meta->{'calls'} + $eval_stmts;
        print $fh determine_severity($stmts,     undef, 0,
            ($allCalls) ? sprintf("%.1f%%", $stmts/$allCalls*100) : ''
        );
        push @t_stmt_exec, $stmts;

        my $time = $meta->{'time'} + $eval_time;
        print $fh determine_severity($time,      $dev_time, 1,
            ($allTimes) ? sprintf("%.1f%%", $time/$allTimes*100) : ''
        );
        push @t_stmt_time, $time;


        my %levels = reverse %{$profile->get_profile_levels};
        my $rep_links = join '&nbsp;&bull;&nbsp;', map {
            sprintf(qq{<a %s>%s</a>}, $reporter->href_for_file($fi, undef, $_), $_)
        } grep { $levels{$_} } qw(line block sub);
        print $fh "<td>$rep_links</td>";

        print $fh sprintf q{<td><a name="f%s" title="%s">%s</a> %s</td>},
            $fi->fid, $fi->abs_filename, $fi->filename_without_inc,
            (@extra) ? sprintf("(%s)", join "; ", @extra) : "";
        print $fh "</tr>\n";
    }
    print $fh "</tbody>\n";

    if ($add_totals) {
        print $fh "<tfoot>\n";
        my $stats_fmt =
            qq{<tr class="index"><td class="n">%s</td><td class="n">%s</td><td colspan="2" style="font-style: italic">%s</td></tr>};
        my $t_notes = "";
        my $stmt_time_diff = $allTimes - sum(@t_stmt_time);
        if (sum(@t_stmt_exec) != $allCalls or $stmt_time_diff > 0.001) {
            $stmt_time_diff = ($stmt_time_diff > 0.001)
                ? sprintf(" and %s", fmt_time($stmt_time_diff)) : "";
            $t_notes = sprintf "(%d statements%s are unaccounted for)",
                $allCalls - sum(@t_stmt_exec), $stmt_time_diff;
        }
        print $fh sprintf $stats_fmt, fmt_float(sum(@t_stmt_exec)), fmt_time(sum(@t_stmt_time)),
                "Total $t_notes"
            if @t_stmt_exec > 1 or $t_notes;

        if (@t_stmt_exec > 1) {
            print $fh sprintf $stats_fmt,
                int(fmt_float(sum(@t_stmt_exec) / @t_stmt_exec)),
                    fmt_time( sum(@t_stmt_time) / @t_stmt_time), "Average";
            print $fh sprintf $stats_fmt, '', fmt_time( $dev_time->[1]), "Median";
            print $fh sprintf $stats_fmt, '', fmt_float($dev_time->[0]), "Deviation"
                if $dev_time->[0];
        }

        print $fh "</tfoot>\n";
    }

    print $fh '</table>';
    push @on_ready_js, q{
        $("#filestable").tablesorter({
            sortList: [[1,1],[3,1]],
            headers: {
                1: { sorter: 'fmt_time' },
                2: { sorter: false      }
            }
        });

        $(".floatHeaders").each( function(){ $(this).floatThead(); } );

        show_fragment_target();
        $(window).on('hashchange', function(e){
          show_fragment_target();
        });
    };

    return "";
}

# calculates how good or bad the time is for a file based on the others
sub determine_severity {
    my $val = shift;
    return "<td></td>" unless defined $val;
    my $stats = shift;    # @_[3] is like arrayref (deviation, mean)
    my $is_time = shift;
    my $title   = shift;

    # normalize the width/precision so that the tables look good.
    my $fmt_val = ($is_time)
        ? fmt_time($val)
        : fmt_float($val, NUMERIC_PRECISION);

    my $class;
    if (defined $stats) {

        my $devs = ($val - $stats->[1]);    #stats->[1] is the mean.
        $devs /= $stats->[0] if $stats->[0];    # no divide by zero when all values equal

        if ($devs < 0) {                        # fast
            $class = 'c3';
        }
        elsif ($devs < SEVERITY_GOOD) {
            $class = 'c3';
        }
        elsif ($devs < SEVERITY_BAD) {
            $class = 'c2';
        }
        elsif ($devs < SEVERITY_SEVERE) {
            $class = 'c1';
        }
        else {
            $class = 'c0';
        }
    }
    else {
        $class = 'n';
    }

    if ($title) {
        $title = (ref $title) ? $$title : _escape_html($title);
        $fmt_val = qq{<span title="$title">$fmt_val</span>};
    }

    return qq{<td class="$class">$fmt_val</td>};
}


# return an html string with buttons for switching between profile levels of detail
sub get_level_buttons {
    my $mode_ref = shift;
    my $file     = shift;
    my $level    = shift;

    my $html = join '&emsp;&bull;&emsp;', map {
        my $mode = $mode_ref->{$_};

        if ($mode eq $level) {
            qq{<span class="mode_btn mode_btn_selected">$mode view</span>};
        }
        else {
            my $mode_file = $file;

            # replace the mode specifier in the output file name -- file-name-MODE.html
            $mode_file =~ s/(.*-).*?\.html/$1$mode.html/o;

            qq{<span class="mode_btn"><a href="$mode_file">$mode view</a></span>};
        }
    } keys %$mode_ref;

    return qq{<span>&laquo;&emsp;$html&emsp;&raquo;</span>};
}


sub get_footer {
    my ($profile) = @_;
    my $version = $Devel::NYTProf::Core::VERSION;

    my $js = '';
    if (@on_ready_js) {
        # XXX I've no idea why this workaround is needed (or works).
        # without it the file table on the index page isn't sortable
        @on_ready_js = reverse @on_ready_js;

        $js = sprintf q{
            <script type="text/javascript">
              $(document).ready(function() { %s } );
            </script>
        }, join("\n", '', @on_ready_js, '');
        @on_ready_js = ();
    };

    # spacing so links to #line near can put right line at top near the bottom of the report
    my $spacing = "<br />" x 10;
    return qq{
        $js
        <div class="footer">Report produced by the
        <a href="http://metacpan.org/release/Devel-NYTProf/">NYTProf $version</a>
        Perl profiler, developed by
        <a href="http://www.linkedin.com/in/timbunce">Tim Bunce</a> based on
        work by Adam Kaplan and Salvador Fandiño García.
        </div>
        $spacing
    };
}

# returns the generic header string.  Here only to make the code more readable.
sub get_html_header {
    my $title = shift || "Profile Index - NYTProf";
    my $opts = shift || {};

    $title = _escape_html($title);

    my $html = <<EOD;
    <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
    <html xmlns="http://www.w3.org/1999/xhtml">
EOD
    $html = "<html>" if $opts->{not_xhtml};
    $html .= <<EOD;
<!--
This file was generated by Devel::NYTProf version $Devel::NYTProf::Core::VERSION
-->
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <meta http-equiv="Content-Language" content="en-us" />
    <meta name="robots" content="noindex,nofollow" />
    <title>$title</title>
EOD

    $html .= qq{    <link rel="stylesheet" type="text/css" href="style.css" />\n}
        unless $opts->{skip_style};

    if (my $css = $opts->{add_jit}) {
        $html .= qq{    <link rel="stylesheet" type="text/css" href="js/jit/$css.css" />\n};
        $html .= qq{    <script language="JavaScript" src="js/jit/jit.js"></script>\n};
    }

    $html .= <<'EOD' unless $opts->{skip_jquery};
    <script type="text/javascript" src="js/jquery-min.js"></script>

    <script type="text/javascript" src="js/jquery.floatThead.min.js"></script>
    <script type="text/javascript" src="js/jquery-tablesorter-min.js"></script>
    <link rel="stylesheet" type="text/css" href="js/style-tablesorter.css" />
    <script type="text/javascript">
    // when a column is first clicked on to sort it, use descending order
    // XXX doesn't seem to work (and not just because the tablesorter formatSortingOrder() is broken)
    $.tablesorter.defaults.sortInitialOrder = "desc";
    // add parser through the tablesorter addParser method
    $.tablesorter.addParser({
        id: 'fmt_time',   // name of this parser
        is: function(s) {
            return false; // return false so this parser is not auto detected
        },
        format: function(orig) { // format data for normalization
            // console.log(orig);
            var val = orig.replace(/ns/,'');
            if (val != orig) { return val / (1000*1000*1000); }
            val = orig.replace(/[µ\\xB5]s/,''); /* micro */
            if (val != orig) { return val / (1000*1000); }
            val = orig.replace(/ms/,'');
            if (val != orig) { return val / (1000); }
            val = orig.replace(/([0-9])s/,"$1");
            if (val != orig) { return val; }
            if (orig == '0') { return orig; }
            var non_number = orig.replace(/^[-+]?[0-9.]+/, '', 'g');
            console.log('no match for fmt_time of '+orig+' (units:'+non_number+' charCodeAt0:'+non_number.charCodeAt(0)+')');
            return orig;
        },
        type: 'numeric' // set type, either numeric or text
    });

    function show_fragment_target() {
        var tgt = $(':target');
        var table = tgt.closest('table.floatHeaders');
        if( tgt.is('a') && table.is('table.floatHeaders') )
        {
            var cury     = $(window).scrollTop();
            var fhYPos   = table.prev('.floatThead-container').offset().top;
            var thHeight = table.find('thead').first().height();
            var tYPos    = parseInt($(':target').closest('tr').position().top);
            if( tYPos < (fhYPos + thHeight) )
            {
                $(window).scrollTop(
                    tYPos - (thHeight)
                );
            }
        }
    }
    </script>
EOD
    $html .= $opts->{head_epilogue} if $opts->{head_epilogue};

    $html .= <<EOD;
</head>
EOD

    return $html;
}

sub get_page_header {
    my %args = @_;
    my ($profile, $head1, $head2, $right1, $right2, $skip_link_to_index) = (
        $args{profile}, $args{title},     $args{subtitle},
        $args{title2},  $args{subtitle2}, $args{skip_link_to_index}
    );

    $head2  ||= qq{<br />For ${ \($profile->{attribute}{application}) }};
    $right1 ||= "&nbsp;";
    $right2 ||= "Run on ${ \scalar localtime($profile->{attribute}{basetime}) }<br />Reported on "
        . localtime(time);

    my $back_link = q//;
    unless ($skip_link_to_index) {
        $back_link = qq{<div class="header_back">
            <a href="index.html">&larr; Index</a>
        </div>};
    }

    my @body_attribs;
    push @body_attribs, qq{onload="$args{body_onload}"} if $args{body_onload};
    my $body_attribs = join "; ", @body_attribs;

    return qq{<body $body_attribs> 
<div class="header" style="position: relative; overflow-x: hidden; overflow-y: hidden; z-index: 0; ">
$back_link
<div class="headerForeground" style="float: left">
    <span class="siteTitle">$head1</span>
    <span class="siteSubtitle">$head2</span>
</div>
<div class="headerForeground" style="float: right; text-align: right">
    <span class="siteTitle">$right1</span>
    <span class="siteSubtitle">$right2</span>
</div>
<div style="position: absolute; left: 0px; top: 0%; width: 100%; height: 101%; z-index: -1; background-color: rgb(17, 136, 255); "></div>
<div style="position: absolute; left: 0px; top: 2%; width: 100%; height: 99%; z-index: -1; background-color: rgb(16, 134, 253); "></div>
<div style="position: absolute; left: 0px; top: 4%; width: 100%; height: 97%; z-index: -1; background-color: rgb(16, 133, 252); "></div>
<div style="position: absolute; left: 0px; top: 6%; width: 100%; height: 95%; z-index: -1; background-color: rgb(15, 131, 250); "></div>
<div style="position: absolute; left: 0px; top: 8%; width: 100%; height: 93%; z-index: -1; background-color: rgb(15, 130, 249); "></div>
<div style="position: absolute; left: 0px; top: 10%; width: 100%; height: 91%; z-index: -1; background-color: rgb(15, 129, 248); "></div>
<div style="position: absolute; left: 0px; top: 12%; width: 100%; height: 89%; z-index: -1; background-color: rgb(14, 127, 246); "></div>
<div style="position: absolute; left: 0px; top: 14%; width: 100%; height: 87%; z-index: -1; background-color: rgb(14, 126, 245); "></div>
<div style="position: absolute; left: 0px; top: 16%; width: 100%; height: 85%; z-index: -1; background-color: rgb(14, 125, 244); "></div>
<div style="position: absolute; left: 0px; top: 18%; width: 100%; height: 83%; z-index: -1; background-color: rgb(13, 123, 242); "></div>
<div style="position: absolute; left: 0px; top: 20%; width: 100%; height: 81%; z-index: -1; background-color: rgb(13, 122, 241); "></div>
<div style="position: absolute; left: 0px; top: 22%; width: 100%; height: 79%; z-index: -1; background-color: rgb(13, 121, 240); "></div>
<div style="position: absolute; left: 0px; top: 24%; width: 100%; height: 77%; z-index: -1; background-color: rgb(12, 119, 238); "></div>
<div style="position: absolute; left: 0px; top: 26%; width: 100%; height: 75%; z-index: -1; background-color: rgb(12, 118, 237); "></div>
<div style="position: absolute; left: 0px; top: 28%; width: 100%; height: 73%; z-index: -1; background-color: rgb(12, 116, 235); "></div>
<div style="position: absolute; left: 0px; top: 30%; width: 100%; height: 71%; z-index: -1; background-color: rgb(11, 115, 234); "></div>
<div style="position: absolute; left: 0px; top: 32%; width: 100%; height: 69%; z-index: -1; background-color: rgb(11, 114, 233); "></div>
<div style="position: absolute; left: 0px; top: 34%; width: 100%; height: 67%; z-index: -1; background-color: rgb(11, 112, 231); "></div>
<div style="position: absolute; left: 0px; top: 36%; width: 100%; height: 65%; z-index: -1; background-color: rgb(10, 111, 230); "></div>
<div style="position: absolute; left: 0px; top: 38%; width: 100%; height: 63%; z-index: -1; background-color: rgb(10, 110, 229); "></div>
<div style="position: absolute; left: 0px; top: 40%; width: 100%; height: 61%; z-index: -1; background-color: rgb(10, 108, 227); "></div>
<div style="position: absolute; left: 0px; top: 42%; width: 100%; height: 59%; z-index: -1; background-color: rgb(9, 107, 226); "></div>
<div style="position: absolute; left: 0px; top: 44%; width: 100%; height: 57%; z-index: -1; background-color: rgb(9, 106, 225); "></div>
<div style="position: absolute; left: 0px; top: 46%; width: 100%; height: 55%; z-index: -1; background-color: rgb(9, 104, 223); "></div>
<div style="position: absolute; left: 0px; top: 48%; width: 100%; height: 53%; z-index: -1; background-color: rgb(8, 103, 222); "></div>
<div style="position: absolute; left: 0px; top: 50%; width: 100%; height: 51%; z-index: -1; background-color: rgb(8, 102, 221); "></div>
<div style="position: absolute; left: 0px; top: 52%; width: 100%; height: 49%; z-index: -1; background-color: rgb(8, 100, 219); "></div>
<div style="position: absolute; left: 0px; top: 54%; width: 100%; height: 47%; z-index: -1; background-color: rgb(7, 99, 218); "></div>
<div style="position: absolute; left: 0px; top: 56%; width: 100%; height: 45%; z-index: -1; background-color: rgb(7, 97, 216); "></div>
<div style="position: absolute; left: 0px; top: 58%; width: 100%; height: 43%; z-index: -1; background-color: rgb(7, 96, 215); "></div>
<div style="position: absolute; left: 0px; top: 60%; width: 100%; height: 41%; z-index: -1; background-color: rgb(6, 95, 214); "></div>
<div style="position: absolute; left: 0px; top: 62%; width: 100%; height: 39%; z-index: -1; background-color: rgb(6, 93, 212); "></div>
<div style="position: absolute; left: 0px; top: 64%; width: 100%; height: 37%; z-index: -1; background-color: rgb(6, 92, 211); "></div>
<div style="position: absolute; left: 0px; top: 66%; width: 100%; height: 35%; z-index: -1; background-color: rgb(5, 91, 210); "></div>
<div style="position: absolute; left: 0px; top: 68%; width: 100%; height: 33%; z-index: -1; background-color: rgb(5, 89, 208); "></div>
<div style="position: absolute; left: 0px; top: 70%; width: 100%; height: 31%; z-index: -1; background-color: rgb(5, 88, 207); "></div>
<div style="position: absolute; left: 0px; top: 72%; width: 100%; height: 29%; z-index: -1; background-color: rgb(4, 87, 206); "></div>
<div style="position: absolute; left: 0px; top: 74%; width: 100%; height: 27%; z-index: -1; background-color: rgb(4, 85, 204); "></div>
<div style="position: absolute; left: 0px; top: 76%; width: 100%; height: 25%; z-index: -1; background-color: rgb(4, 84, 203); "></div>
<div style="position: absolute; left: 0px; top: 78%; width: 100%; height: 23%; z-index: -1; background-color: rgb(3, 82, 201); "></div>
<div style="position: absolute; left: 0px; top: 80%; width: 100%; height: 21%; z-index: -1; background-color: rgb(3, 81, 200); "></div>
<div style="position: absolute; left: 0px; top: 82%; width: 100%; height: 19%; z-index: -1; background-color: rgb(3, 80, 199); "></div>
<div style="position: absolute; left: 0px; top: 84%; width: 100%; height: 17%; z-index: -1; background-color: rgb(2, 78, 197); "></div>
<div style="position: absolute; left: 0px; top: 86%; width: 100%; height: 15%; z-index: -1; background-color: rgb(2, 77, 196); "></div>
<div style="position: absolute; left: 0px; top: 88%; width: 100%; height: 13%; z-index: -1; background-color: rgb(2, 76, 195); "></div>
<div style="position: absolute; left: 0px; top: 90%; width: 100%; height: 11%; z-index: -1; background-color: rgb(1, 74, 193); "></div>
<div style="position: absolute; left: 0px; top: 92%; width: 100%; height: 9%; z-index: -1; background-color: rgb(1, 73, 192); "></div>
<div style="position: absolute; left: 0px; top: 94%; width: 100%; height: 7%; z-index: -1; background-color: rgb(1, 72, 191); "></div>
<div style="position: absolute; left: 0px; top: 96%; width: 100%; height: 5%; z-index: -1; background-color: rgb(0, 70, 189); "></div>
<div style="position: absolute; left: 0px; top: 98%; width: 100%; height: 3%; z-index: -1; background-color: rgb(0, 69, 188); "></div>
<div style="position: absolute; left: 0px; top: 100%; width: 100%; height: 1%; z-index: -1; background-color: rgb(0, 68, 187); "></div>
</div>\n};
}

sub get_css {
    return <<'EOD';
/* Stylesheet for Devel::NYTProf::Reader HTML reports */

/* You may modify this file to alter the appearance of your coverage
 * reports. If you do, you should probably flag it read-only to prevent
 * future runs from overwriting it.
 */

/* Note: default values use the color-safe web palette. */
a { color: blue; }
a:visited { color: #6d00E6; }
a:hover { color: red; }

body { font-family: sans-serif; margin: 0px; background-color: white; color:#222; }
.body_content { margin: 8px; }

.header { font-family: sans-serif; padding-left: 0.5em; padding-right: 0.5em; }
.headerForeground { color: white; padding: 10px; padding-top: 50px; }
.siteTitle { font-size: 2em; }
.siteSubTitle { font-size: 1.2em; }

.header_back { 
    position: absolute; 
    padding: 10px;
}
.header_back > a:link,
.header_back > a:visited {
    color: white; 
    text-decoration: none;
    font-size: 0.75em;
}

.jump_to_file {
    margin-top: 20px;
}

.footer,
.footer > a:link,
.footer > a:visited {
    color: #cccccc;
}
.footer { margin: 30px; }

table { 
    border-collapse: collapse; 
    border-spacing: 0px; 
    margin-top: 20px;
}
tr { 
    text-align : center;
    vertical-align: top; 
}
th,.h {
    background-color: #dddddd;
    border: solid 1px #666666;
    padding: 0em 0.4em 0em 0.4em;
    font-size:0.8em;
}
td { 
    border: solid 1px #cccccc; 
    padding: 0em 0.4em 0em 0.4em;
}
caption {
    background-color: #dddddd;
    text-align: left;
    white-space: pre;
    padding: 0.4em;
}

.table_footer { color: gray; }
.table_footer > a:link,
.table_footer > a:visited { color: gray; }
.table_footer > a:hover   { color: red; }

.index { text-align: left; }

.mode_btn_selected {
  font-style: italic;
}

/* subroutine dispatch table */
.sub_name {
  text-align: left;
  font-family: monospace;
  white-space: pre;
  color: gray;
}

/* source code */
th.left_indent_header {
  padding-left: 15px;
  text-align: left;
}

pre,.s {
    text-align: left;
    font-family: monospace;
    white-space: pre;
}
/* plain number */
.n { text-align: right }

/* Classes for color-coding profiling information:
 *   c0  : code not hit
 *   c1  : coverage >= 75%
 *   c2  : coverage >= 90%
 *   c3  : path covered or coverage = 100%
 */
.c0, .c1, .c2, .c3 { text-align: right; }
.c0 { background-color: #ffb3b3; }  /* red */
.c1 { background-color: #ffd9b4; }  /* orange */
.c2 { background-color: #ffffB4; }  /* yellow */
.c3 { background-color: #B4ffB4; }  /* green */

/* warnings */
.warn {
    background-color: #FFFFAA;
    border: 0;
    width: 96%;
    text-align: center;
    padding: 5px 0;
}
.warn_title {
    background-color: #FFFFAA;
    border: 0;
    color: red;
    width: 96%;
    font-size: 2em;
    text-align: center;
    padding: 5px 0;
}

/* summary of calls into and out of a sub */
.calls {
  display: block;
  color: gray;
  padding-top: 5px;
  padding-bottom: 5px;
  text-decoration: none;
}
.calls:hover {
    background-color: #e8e8e8;
    color: black;
}
.calls       a       { color: gray;  text-decoration: none; }
.calls:hover a       { color: black; text-decoration: underline; }
.calls:hover a:hover { color: red; }
/* give a little headroom to the summary of calls into a sub */
.calls .calls_in { margin-top: 5px; }

.vis_header {
    text-align:center;
    font-style: italic;
    padding-top: 5px; color: gray;
}

.flamegraph {
    margin: 20px 0px;
}

EOD
}

__END__

=head1 DESCRIPTION

Devel::NYTProf is a powerful feature-rich Perl source code profiler.
See L<Devel::NYTProf> for details.

C<nytprofhtml> generates a set of html reports from a single data file
generated by L<Devel::NYTProf>. (If your process forks you'll probably have
multiple files. See L<Devel::NYTProf> and L<nytprofmerge>.)

The reports include dynamic runtime analysis wherein each line and each file
is analyzed based on the performance of the other lines and files.  As a
result, you can quickly find the slowest module and the slowest line in a 
module.  Slowness is measured in three ways: total calls, total time, and
average time per call.

Coloring is based on absolute deviations from the median.
See L<http://en.wikipedia.org/wiki/Median_absolute_deviation> for more details.

That might sound complicated, but in reality you can just run the command and
enjoy your report!

=head1 COMMAND-LINE OPTIONS

=over 4

=item -f, --file <filename>

Specifies the location of the file generated by L<Devel::NYTProf>.
Default: ./nytprof.out

=item -o, --out <dir>

The directory in which to place the generated report files. Default: ./nytprof/

=item -d, --delete

Purge any existing contents of the report output directory.

=item -l, --lib <dir>

Add a path to the beginning of @INC to help nytprofhtml find the source files
used by the code. Should not be needed in practice.

=item --open

Make your web browser visit the report after it has been generated.

If this doesn't work well for you, try installing the L<Browser::Open> module.

=item -m, --minimal

Don't generate graphviz .dot files or block/sub-level reports.

=item --no-flame

Disable generation of the flamegraph on the index page.
Also disables calculation of distinct call stacks that are used to produce the
flamegraph.

=item -h, --help

Print the help message.

=back

=head1 SAMPLE OUTPUT

You can see a complete report for a large application at
L<http://timbunce.github.io/devel-nytprof/sample-report/nytprof-20160319/index.html>

The report was generated by profiling L<perlcritic> 1.121 checking its own source code
using perl v5.18.2.

=head1 DIAGNOSTICS

=head2 "Unable to open '... (autosplit into ...)'"

The profiled application executed code in a module that used L<AutoLoader> to
load the code from a separate .al file.  NYTProf automatically recognises this
situation and tries to determine the 'parent' module file so it can associate
the profile data with it.  In order to do that the parent module file must
already be 'known' to NYTProf, typically by already having some code profiled.

You're only likely to see this warning if you're using the C<start> option to
start profiling after compile-time. The effect is that times spent in
autoloaded subs won't be associated with the parent module file and you won't
get annotated reports for them.

You can avoid this by using the default C<start=begin> option, or by ensuring
you execute some non-autoloaded code in the parent module, while the profiler is
running, before an autoloaded sub is called.


=head2 Background

Subroutine-level profilers:

  Devel::DProf        | 1995-10-31 | ILYAZ
  Devel::AutoProfiler | 2002-04-07 | GSLONDON
  Devel::Profiler     | 2002-05-20 | SAMTREGAR
  Devel::Profile      | 2003-04-13 | JAW
  Devel::DProfLB      | 2006-05-11 | JAW
  Devel::WxProf       | 2008-04-14 | MKUTTER

Statement-level profilers:

  Devel::SmallProf    | 1997-07-30 | ASHTED
  Devel::FastProf     | 2005-09-20 | SALVA
  Devel::NYTProf      | 2008-03-04 | AKAPLAN
  Devel::Profit       | 2008-05-19 | LBROCARD

Devel::NYTProf is a (now distant) fork of Devel::FastProf, which was itself an
evolution of Devel::SmallProf.

Adam Kaplan took Devel::FastProf and added html report generation (based on
Devel::Cover) and a test suite - a tricky thing to do for a profiler.
Meanwhile Tim Bunce had been extending Devel::FastProf to add novel
per-sub and per-block timing, plus subroutine caller tracking.

When Devel::NYTProf was released Tim switched to working on Devel::NYTProf
because the html report would be a good way to show the extra profile data, and
the test suite made development much easier and safer.

Then he went a little crazy and added a slew of new features, in addition to
per-sub and per-block timing and subroutine caller tracking. These included the
'opcode interception' method of profiling, ultra-fast and robust inclusive
subroutine timing, doubling performance, plus major changes to html reporting
to display all the extra profile call and timing data in richly annotated and
cross-linked reports.

Steve Peters came on board along the way with patches for portability and to
keep NYTProf working with the latest development Perl versions.

Adam's work is sponsored by The New York Times Co. L<http://open.nytimes.com>.
Tim's work was partly sponsored by Shopzilla. L<http://www.shopzilla.com>.

=head1 SEE ALSO

Mailing list and discussion at L<http://groups.google.com/group/develnytprof-dev>

Public SVN Repository and hacking instructions at L<http://code.google.com/p/perl-devel-nytprof/>

L<Devel::NYTProf>,
L<Devel::NYTProf::Reader>,
L<nytprofcsv>

=head1 AUTHOR

B<Adam Kaplan>, C<< <akaplan at nytimes.com> >>.
B<Tim Bunce>, L<http://www.tim.bunce.name> and L<http://blog.timbunce.org>.
B<Steve Peters>, C<< <steve at fisharerojo.org> >>.

=head1 COPYRIGHT AND LICENSE

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.8 or,
at your option, any later version of Perl 5 you may have available.

=cut

# vim:ts=8:sw=4:expandtab

__END__
:endofperl
