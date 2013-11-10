# Based on http://serverfault.com/questions/127460/how-do-i-install-a-yum-package-group-with-puppet
define yumgroup(
  $ensure   = 'present',
  $optional = false,
  $schedule = 'daily',
  $usecache = true
) {
  $pkg_types_arg = $optional ? {
    true    => '--setopt=group_package_types=optional,default,mandatory',
    default => '',
  }
  $cache = $usecache ? {
    true    => '-C',
    default => '',
  }
  case $ensure {
    present,installed: {
      exec { "Installing ${name} yum group":
        command  => "/usr/bin/yum -y groupinstall ${pkg_types_arg} '${name}'",
        unless   => "/usr/bin/yum $cache grouplist 2>/dev/null | /usr/bin/perl -ne 'last if /^Available/o; next if /^\\w/o; print' | /bin/grep -qw '${name}'",
        timeout  => 600,
        schedule => $schedule,
      }
    }
    absent: {
      exec { "Removing ${name} yum group":
        command  => "/usr/bin/yum -y groupremove ${pkg_types_arg} '${name}'",
        unless   => "/usr/bin/yum $cache grouplist 2>/dev/null | /usr/bin/perl -ne 'last if /^Available/o; next if /^\\w/o; print' | /bin/grep -qw '${name}'",
        timeout  => 600,
        schedule => $schedule,
      }
    }
    default: {
      fail('Unknown ensure value - valid values are: present, installed or absent')
    }
  }
}
package { "git": ensure => present }
package { "cmake": ensure => present }
package { "python-devel": ensure => present }
package { "openssl-devel": ensure => present }
package { "boost-devel": ensure => present }
#TODO package { "rst2pdf": ensure => present }
package { "python-sphinx": ensure => present }
package { "lua-devel": ensure => present }
#TODO package { "libgtest-dev": ensure => present }

yumgroup { "Development Tools": ensure => present  }
file { "/home/vagrant/build.sh":
    ensure  => "present",
    mode    => 755,
	source  => "/etc/puppet/files/build.sh"
}
file { "/home/vagrant/build-protobuf.sh":
    ensure  => "present",
    mode    => 755,
	source  => "/etc/puppet/files/build-protobuf.sh"
}
file { "/home/vagrant/build-cryptopp.sh":
    ensure  => "present",
    mode    => 755,
	source  => "/etc/puppet/files/build-cryptopp.sh"
}

exec { "yum-update":
    command => "/usr/bin/yum -y update"
}

Exec["yum-update"] -> Package <| |>