=head1 NAME

common::sense - save a tree AND a kitten, use common::sense!

=head1 SYNOPSIS

 use common::sense;

 # roughly the same as, with much lower memory usage:
 #
 # use strict qw(vars subs);
 # use feature qw(say state switch);
 # no warnings;

=head1 DESCRIPTION

This module implements some sane defaults for Perl programs, as defined by
two typical (or not so typical - use your common sense) specimens of
Perl coders.

=over 4

=item no warnings

The dreaded warnings. Even worse, the horribly dreaded C<-w> switch. Even
though we don't care if other people use warnings (and certainly there are
useful ones), a lot of warnings simply go against the spirit of Perl, most
prominently, the warnings related to C<undef>. There is nothing wrong with
C<undef>: it has well-defined semantics, it is useful, and spitting out
warnings you never asked for is just evil.

So every module needs C<no warnings> to avoid somebody accidentally using
C<-w> and forcing his bad standards on our code. No will do.

Funnily enough, L<perllexwarn> explicitly mentions C<-w> (and not in a
favourable way), but standard utilities, such as L<prove>, or MakeMaker
when running C<make test> enable them blindly.

=item use strict qw(subs vars)

Using C<use strict> is definitely common sense, but C<use strict
'refs'> definitely overshoots it's usefulness. After almost two
decades of Perl hacking, we decided that it does more harm than being
useful. Specifically, constructs like these:

   @{ $var->[0] }

Must be written like this (or similarly), when C<use strict 'refs'> is in
scope, and C<$var> can legally be C<undef>:

   @{ $var->[0] || [] }

This is annoying, and doesn't shield against obvious mistakes such as
using C<"">, so one would even have to write:

   @{ defined $var->[0] ? $var->[0] :  [] }

... which nobody with a bit of common sense would consider
writing. Curiously enough, sometimes, perl is not so strict, as this works
even with C<use strict> in scope:

   for (@{ $var->[0] }) { ...

If that isnt hipocrasy! And all that from a mere program!

=item use feature qw(say state given)

We found it annoying that we always have to enable extra features. If
something breaks because it didn't anticipate future changes, so be
it. 5.10 broke almost all our XS modules and nobody cared either - and few
modules that are no longer maintained work with newer versions of Perl,
regardless of use feature.

If your code isn't alive, it's dead, jim.

=item much less memory

Just using all those pragmas together waste <blink>I<< B<776> kilobytes
>></blink> of precious memory in my perl, for I<every single perl process
using our code>, which on our machines, is a lot. In comparison, this
module only uses I<< B<four> >> kilobytes (I even had to write it out so
it looks like more) of memory on the same platform.

The money/time/effort/electricity invested in these gigabytes (probably
petabytes globally!) of wasted memory could easily save 42 trees, and a
kitten!

=cut

package common::sense;

our $VERSION = '0.04';

sub import {
   # no warnings
   ${^WARNING_BITS} ^= ${^WARNING_BITS};

   # use strict vars subs
   $^H |= 0x00000600;

   # use feature
   $^H{feature_switch} =
   $^H{feature_say}    =
   $^H{feature_state}  = 1;
}

1;

=back

=head1 THERE IS NO 'no common::sense'!!!! !!!! !!

This module doesn't offer an unimport. First of all, it wastes even more
memory, second, and more importantly, who with even a bit of common sense
would want no common sense?

=head1 STABILITY AND FUTURE VERSIONS

Future versions might change just about everything in this module. We
might test our modules and upload new ones working with newer versions of
this module, and leave you standing in the rain because we didn't tell
you.

Most likely, we will pick a few useful warnings, instead of just disabling
all of them. And maybe we will load some nifty modules that try to emulate
C<say> or so with perls older than 5.10 (this module, of course, should
work with older perl versions - supporting 5.8 for example is just common
sense at this time. Maybe not in the future, but of course you can trust
our common sense).


=head1 WHAT OTHER PEOPLE HAVE TO SAY ABOUT THIS MODULE

Pista Palo

   "Something in short supply these days..."

Steffen Schwigon

   "This module is quite for sure *not* just a repetition of all the other
   'use strict, use warnings'-approaches, and it's also not the opposite.
   [...] And for its chosen middle-way it's also not the worst name ever.
   And everything is documented."

BKB

   "[Deleted - thanks to Steffen Schwigon for pointing out this review was
   in error.]"

Somni

   "the arrogance of the guy"
   "I swear he tacked somenoe else's name onto the module
   just so he could use the royal 'we' in the documentation"

dngor

   "Heh.  '"<elmex at ta-sa.org>"'  The quotes are semantic
   distancing from that e-mail address."

Jerad Pierce

   "Awful name (not a proper pragma), and the SYNOPSIS doesn't tell you
   anything either. Nor is it clear what features have to do with "common
   sense" or discipline."

acme

   "THERE IS NO 'no common::sense'!!!! !!!! !!"

crab

   "i wonder how it would be if joerg schilling wrote perl modules."

=head1 AUTHOR

 Marc Lehmann <schmorp@schmorp.de>
 http://home.schmorp.de/

 Robin Redeker, "<elmex at ta-sa.org>".


=cut

