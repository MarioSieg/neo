package ActiveState::Test::X11Server;

use strict;
my %defaults = (
    debug   => 0,
    managed => [
        qw(
          http://plow.activestate.com/X11TestServer
          )
    ],
    remote => [
        qw(
          plow.activestate.com:5
          plow.activestate.com:6
          )
    ],
    local => 20,
    order => [
        qw(
          local
          managed
          remote
          )
    ],
);

sub new {
    my $class = shift;
    my %args  = (%defaults, @_);
    my $self  = bless \%args, $class;

    $self->{impl} = ActiveState::Test::X11Server::Impl->new($self);

    if (not $self->{impl}) {
        die "Couldn't find a way to provide a X server, sorry!";
    }

    $self;
}

sub display {
    return shift->{impl}->display();
}

#Base interface for a running server
package ActiveState::Test::X11Server::Impl;

sub debug { shift->{debug} }

sub new {
    my ($class, $args) = @_;

    my $instance;
    foreach my $impl (@{$args->{order} || []}) {
        print STDERR "Trying to get an X server as $impl\n"
          if $args->{debug};
        my $pkg = join('::', $class, ucfirst $impl);
        last if $instance = $pkg->instance($args);
    }
    return $instance;
}

sub display {
    shift->{display};
}

#Just so there is always an instance method
sub instance { return }

#Locally running X server (Xvfb)
package ActiveState::Test::X11Server::Impl::Local;
use base qw(ActiveState::Test::X11Server::Impl);

use IPC::Open3;
use File::Spec;
use POSIX ":sys_wait_h";
use Sys::Hostname;

#We try local display 1 .. local in random order
sub displays {
    my ($class, $args) = @_;
    #Randomize
    return map { $_->[1] }
           sort {$a->[0] <=> $b->[0] }
           map { [ rand() => $_ ] }
           1 .. $args->{local};
}

sub instance {
    my $class = shift;
    my $args  = shift;
    my @bin   = qw(Xvfb);
    foreach my $bin (@bin) {
        foreach my $p (qw(/usr/X11R6/bin)) {
            my $x11 = File::Spec->catfile($p, $bin);
            if (-x $x11) {
                foreach my $d ($class->displays($args)) {
                    my $pid = open3(
                                    my $in,  my $out,
                                    my $err, $x11,
                                    '-ac',   ":$d"
                                   );
                    if ($pid) {

                        #Annoying way to figure out the process failed
                        sleep 1;
                        waitpid $pid, WNOHANG;
                        if ($? > 128) { #Failed to start X, so try again
                            next;
                        }
                        return
                          bless {
                                 x11     => $x11,
                                 pid     => $pid,
                                 display => join(':', hostname(), $d),
                                }, $class;
                    }
                }
            }
        }
    }
    return;
}

sub DESTROY {
    my $self = shift;
    if (my $pid = $self->{pid}) {
        local $?;
        if (kill 0 => $pid) {
            kill TERM => $pid;
            my $pid = waitpid($pid, 0);
            print STDERR "Child $pid terminated\n" if $self->debug;
        }
    }
}

#Remotely requested X11
package ActiveState::Test::X11Server::Impl::Managed;
use base qw(ActiveState::Test::X11Server::Impl);

use strict;
our $VERSION = 0.01;

use LWP::Simple;

sub instance {
    my $class = shift;
    my $args  = shift;
    foreach my $base (@{$args->{managed} || []}) {
        my $url     = $base . "/$VERSION/get";
        my $display = get($url);
        next unless $display;
        #Make sure returned display is sane
        next unless $display =~ /:[0-9.]+$/;
        chomp $display;
        print STDERR "Managed display = $display\n";
        if ($display) {
            my $self = bless {
                              display => $display,
                              url     => $base
                             }, $class;
            return $self;
        }
    }
    return;
}

sub DESTROY {
    my $self = shift;
    my $url  =
      $self->{url} . "/$VERSION/release?display=$self->{display}";
    local $@;
    get($url);
}

#Good old shared, already running X11
package ActiveState::Test::X11Server::Impl::Remote;
use base qw(ActiveState::Test::X11Server::Impl);

sub instance {
    my ($class, $args) = @_;
    #Randomize
    my @displays = map { $_->[1] }
           sort {$a->[0] <=> $b->[0] }
           map { [ rand() => $_ ] }
           @{$args->{remote}};
    my $display = $displays[0];
    my $self = bless {display => $display}, $class;
    return $self;
}

1;

=head1 NAME

ActiveState::Test::X11Server - Provides an available X11 Server

=head1 SYNOPSIS

 use ActiveState::Test::X11Server;
 my $x11 = ActiveState::Test::X11Server->new;
 local $ENV{DISPLAY} = $x11->display;

=head1 DESCRIPTION

This modules tries to produces a valid X11 server from thin air.

It will try to locate Xvfb and start one. Failing that, it will
try and request a new X11 server remotely, thru a simple REST API,
failing that, it will default to some shared X11 server.

This module was designed with testing in mind, but might have usages
beyond that. Keep in mind that if it does fallback to a shared X11
server, you might experience test failures as many people might be
connecting to that server simultaneously. Otherwise, the provided
X11 server is your alone.

=head1 CONFIGURATIONS

Configuration options are passed to the constructor and are: 

=over 4

=item debug (BOOLEAN)

Wether to be verbose or not

Default: Off

=item order (ARRAY REF)

Determines the order in which to try to acquire an X server

Default: local, managed, remote

=item local (Integer)

Specifies the range of DISPLAY setting to try before giving up. Higher
values might take longer (up to a few second each)

Default: 20

=item managed (ARRAY REF)

This is the recommended mechanism. It dynamically requests a X11 server
from a remote server, and gets a newly created one.

Configured as a remote URL implemented by L<ActiveState::Test::X11Server::Apache>

=item remote (ARRAY REF)

A shared remote X11 server that requires no setting up. Most likely shared
by others, and only good as a last resort.

=back

=head1 SEE ALSO

<ActiveState::Test::X11Server::Apache>

=head1 COPYRIGHT

Copyright (C) 2005 ActiveState Corp.  All rights reserved.

