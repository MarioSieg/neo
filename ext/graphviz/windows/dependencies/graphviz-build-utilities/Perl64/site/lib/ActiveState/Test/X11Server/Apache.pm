package ActiveState::Test::X11Server::Apache;

use strict;
use ActiveState::Test::X11Server;

use Apache::Const -compile => 'OK';
use Apache::RequestRec;
use Apache::RequestIO;
use Apache::ServerUtil;
use Apache::ServerRec;
use Apache::Process;
use Apache::Connection;
use APR::Pool;

my $manager = ActiveState::Test::X11Server::Apache::Manager->new;

sub handler {
    my $r    = shift;
    my $info = $r->path_info;

    $info =~ m[/(([0-9.]+)/)?(.*)];

    my ($version, $method) = ($2, $3);
    my %args = parse_args($r, $r->args);

    $r->content_type('text/plain');

    if (my $ref = $manager->can($method)) {
        my $answer = $manager->$ref($r, %args);
        print $answer, "\n";
    }
    else {
        print "unknown method '$method'\n";
    }
    return Apache::OK();
}

my $package = __PACKAGE__;
Apache->server->add_config([split /\n/, <<"EOF"]);
  <Location /X11TestServer>
    SetHandler perl-script
    PerlHandler $package
  </Location>
EOF

#no apreq for now
sub parse_args {
    my ($r, $string) = @_;
    return () unless defined $string;

    return map {
        tr/+/ /;
        s/%([0-9a-fA-F]{2})/pack("C",hex($1))/ge;
        $_;
    } split /[=&;]/, $string, -1;
}

package ActiveState::Test::X11Server::Apache::Manager;
use strict;

use Storable qw(freeze thaw);
use MIME::Base64;
use YAML;

use constant LOG => '/var/log/httpd/x11_test_server_log';

sub get {
    my ($self, $r, %args) = @_;
    my $client = $r->connection->get_remote_host;
    my $answer = $self->_request("get $client");
    return $answer;
}

sub release {
    my ($self, $r, %args) = @_;
    my $display = $args{display};
    my $answer  = $self->_request("release $display");
    return "Released $display: $answer";
}

sub status {
    my ($self, $r, %args) = @_;
    my $answer = $self->_request("status");
    my $ref    = thaw(decode_base64($answer));
    return Dump($ref);
}

sub new {
    my $class = shift;

    open(my $log, ">>" . LOG);

    #Pipe to our manager child
    my ($in, $out);
    pipe $in, $out;

    my $old = select $log;
    $| = 1;
    select $in;
    $| = 1;
    select $out;
    $| = 1;
    select $old;

    close(STDERR);
    open STDERR, ">&", $log;
    close(STDOUT);
    open STDOUT, ">&", $log;

    my $self = bless {
                      log => $log,
                      in  => $in,
                      out => $out,
                     }, $class;

    if (my $pid = fork()) {
        $self->{pid} = $pid;

        #reverse the pipes
        $self->log("Created a manager child (pid:$pid)");
        return $self;
    }
    else {    #The child...
        $self->run();
        CORE::exit();
    }
}

sub DESTROY {
    my $self = shift;
    if (my $pid = $self->{pid}) {
        if (kill 0 => $pid) {
            $self->log("Destroying manager (pid:$pid)");

            #Be nice, shutdown please
            $self->_out("QUIT");
            sleep 1;

            #Enough, shutdown now!
            kill TERM => $pid;
            waitpid $pid, 0;

            $self->log("Destroyed manager (pid:$pid)");
        }
    }
}

use Fcntl ':flock';

sub _request {
    my ($self, $msg) = @_;
    flock($self->{out}, LOCK_EX);
    $self->_out($msg);
    my $ret = $self->_in();
    flock($self->{out}, LOCK_UN);
    return $ret;
}

sub _out {
    my ($self, $msg) = @_;
    my $fh = $self->{out};
    print $fh $msg, "\n";
}

sub _in {
    my ($self, @msg) = @_;
    my $fh = $self->{in};
    my $in = <$fh>;
    chomp $in;
    return $in;
}

sub log {
    my ($self, @args) = @_;
    my $time = localtime;
    my $log  = $self->{log};
    print $log "[$time] ", @args, "\n";
}

sub run {
    my $self = shift;
    $self->log("Child($$) Ready to serve!");
    my $in    = $self->{in};
    my $out   = $self->{out};
    my $cache = $self->{cache} ||= {};

    while (<$in>) {
        last if /QUIT/;
        chomp;
        $self->log("Read: '$_'");
        my ($method, @args) = split /\s+/;
        if ($method eq 'get') {
            my $x11 =
              ActiveState::Test::X11Server->new(
                                           order => [qw(local remote)]);
            my $display = $x11->display;
            $self->log("Acquired new X11 server $display");
            $cache->{$display} = {
                                  x11    => $x11,
                                  since  => time,
                                  client => $args[0],
                                 };
            $self->_out("$display");
        }
        elsif ($method eq 'release') {
            my $display = shift @args;
            my $since   = $cache->{$display}{since};
            my $age     = time - $since;
            $self->log("Releasing X11 $display after $age secs");
            delete $cache->{$display};
            $self->_out("OK");
        }
        elsif ($method eq 'status') {
            my $status = freeze($cache);
            $self->log("Sending out status information");
            my $dump = encode_base64($status, '');
            $self->_out($dump);
        }
        else {
            $self->log("Unknown request '$method'");
            $self->_out("No idea what yo want");
        }
        $self->expire_cache();
    }
}

sub expire_cache {
    my $self    = shift;
    my $cache   = $self->{cache};
    my $now     = time;
    my $timeout = 60 * 60 * 3;      #3 hours
    foreach my $ent (keys %$cache) {
        my $life = $now - $cache->{$ent}{since};
        if ($life > $timeout) {
            $self->log("Expiring $ent after $life secs");
            delete $cache->{$ent};
        }
    }
}

1;

=head1 NAME

ActiveState::Test::X11Server::Apache - Dynamic X server allocation

=head1 SYNOPSIS

  in httpd.conf:
    PerlModule ActiveState::Test::X11Server::Apache

=head1 DESCRIPTION

This modules is the dynamic backend companion to L<ActiveState::Test::X11Server>

=head1 SEE ALSO

<ActiveState::Test::X11Server>

=head1 COPYRIGHT
