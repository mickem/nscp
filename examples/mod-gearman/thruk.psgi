# Thruk, served by plackup instead of by apache with fcgid.
#
# The application itself is the one line at the bottom - everything above it
# is the static content apache would normally map by path: Thruk's own
# javascript and vendored libraries live under /usr/share/thruk/root/thruk,
# its themes under /etc/thruk/themes/themes-enabled. Without this the pages
# render, unstyled, with every icon missing.
use strict;
use warnings;

use Plack::Builder;
use Plack::App::File;
use Thruk;

my $app = Thruk->startup;

builder {
    mount '/thruk/themes' => Plack::App::File->new(root => '/etc/thruk/themes/themes-enabled')->to_app;
    for my $dir (qw(cache images javascript media usercontent vendor docs)) {
        mount "/thruk/$dir" => Plack::App::File->new(root => "/usr/share/thruk/root/thruk/$dir")->to_app;
    }
    mount '/favicon.ico' => Plack::App::File->new(file => '/usr/share/thruk/root/favicon.ico')->to_app;
    mount '/' => $app;
};
