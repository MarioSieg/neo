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
use strict;
use warnings;
use Pod::Spell;

our $VERSION = '1.20'; # VERSION

if ($] ge '5.008001') {
  # Try to get the encoding from the locale
  my $encoding = eval {
    require POSIX;
    POSIX::setlocale(POSIX::LC_CTYPE(), '');
    require I18N::Langinfo;
    I18N::Langinfo::langinfo(I18N::Langinfo::CODESET())
  };

  if ($encoding) {
    binmode(STDOUT, ":encoding($encoding)");
  }
}

if(@ARGV) {  # iterate over files, sending to STDOUT
  foreach my $x (@ARGV) {
    Pod::Spell->new->parse_from_file($x, '-');
  }
} else {     # take from STDIN, send to STDOUT
  Pod::Spell->new->parse_from_filehandle();
}

# ABSTRACT: pod spell checking command line interface
# PODNAME: podspell

__END__

=pod

=encoding UTF-8

=for :stopwords Sean M. Burke Caleb Cushing Olivier Mengué

=head1 NAME

podspell - pod spell checking command line interface

=head1 VERSION

version 1.20

=head1 SYNOPSIS

	% podspell Thing.pm | ispell

=head1 AUTHORS

=over 4

=item *

Sean M. Burke <sburke@cpan.org>

=item *

Caleb Cushing <xenoterracide@gmail.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2016 by Olivier Mengué.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut

__END__
:endofperl
