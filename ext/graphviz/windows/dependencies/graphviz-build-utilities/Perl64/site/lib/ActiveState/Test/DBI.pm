package ActiveState::Test::DBI;

use strict;
our $VERSION = "0.01";

use base qw(Exporter);
our @EXPORT_OK = qw(servers server_info);

my $server_hash;
sub _server_hash {
    return $server_hash ||= do {
	require LWP::UserAgent;
	my $ua = LWP::UserAgent->new(agent => "$0 ", show_progress => 0);
	my $res = $ua->get("http://plow.activestate.com/cgi-bin/connect");
	die $res->request->url . ": " . $res->status_line unless $res->is_success;
        my $hash = eval $res->decoded_content;
       	die if $@;
	die "Did not eval to a hash reference" unless ref($hash) eq "HASH"; 
        $hash;
    };  
}

sub servers {
    return sort keys %{_server_hash()};
}

sub server_info {
    my $name = shift;
    my $cinfo = _server_hash()->{$name} || return;
    if (@$cinfo > 3) {
	# Multiple alternatives, pick a random one
	my $n = int(rand(int(@$cinfo / 3)));
	splice(@$cinfo, 0, $n*3);
    }
    my($dbi_dsn, $user, $pass) = @$cinfo;
    my %hash = (  
	dbi_dsn => $dbi_dsn,
        user => $user,
        defined($pass) ? (password => $pass) : (),
    );
    if ($dbi_dsn =~ s/^dbi:(\w+):?//i) {
	$hash{dbi_driver} = "DBH::$1";
        for (split(/\s*;\s*/, $dbi_dsn)) {
	    my($k,$v) = split(/=/, $_, 2);
	    $hash{$k} = $v;
        }
    }
    return \%hash;
}

sub connect_info {
    my $name = shift;
    my $hash = server_info($name);
    #use Data::Dump; dd $hash;
    my @info;
    if ($hash->{dbi_dsn}) {
	push(@info, $hash->{dbi_dsn}, $hash->{user});
	push(@info, $hash->{password}) if exists $hash->{password};
    }
    return @info;
}

sub connect {
    my $class = shift;
    my $name = shift;
    my @cargs = connect_info($name);
    die "Unrecognized server name '$name'" unless @cargs;
    require DBI;
    return DBI->connect(@cargs, @_);
}

1;

__END__

=head1 NAME

ActiveState::Test::DBI - Database test resources

=head1 SYNOPSIS

  use ActiveState::Test::DBI;
  my $dbh = ActiveState::Test::DBI->connect("oracle-112");

=head1 DESCRIPTION

This module provide the information about the test databases we have available.
These are scratch databases that can be messed with from database regression
test scripts.  The test script ought to use unique prefixes for the objects (tables, etc.)
it creates so that multiple tests can run in parallell without conflict.

The following functions and methods are provided:

=over

=item servers()

This returns a list of names of the test servers provided.

=item server_info( $name )

This returns a hash describing various attributes of the given test server.
Returns C<undef> if no such test server exists.

The hash fields are:

=over

=item host

The host where the test database is found.

=item port

The port to connect to.  If missing use the default for the database.

=item user

The user name to be used for login

=item password

The password to be used for login

=item database

The database name on the host.  Might be missing if the user/password
combination is enough to select the database.

=item dbi_driver

The name of the C<DBH::>-module required to connect to the database with DBI.

=item dbi_dsn

The $data_source string that you need to pass to C<< DBI->connect >>.

=back

=item ActiveState::Test::DBI->connect( $name )

Try to connect to the given server and return a database handle object.
See L<DBI> for what you can do with it.
Croaks if no such test server exists.  DBI will croak if the driver is
missing or the server fails to respond.

=back


=head1 SEE ALSO

L<DBI>

=head1 BUGS

none.

=cut
