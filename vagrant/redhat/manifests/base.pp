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
if ($::operatingsystemmajrelease < 7) {
	package { "cmake": ensure => present }
	->
	file { "/usr/share/cmake/Modules/CPackRPM.cmake":
		ensure  => "present",
		mode    => 644,
		owner   => 'root',
		group   => 'root',
		source  => "/etc/puppet/files/CPackRPM.cmake"
	}
} else {
	package { "cmake": ensure => present }
}

package { "python-devel": ensure => present }
package { "openssl-devel": ensure => present }
package { "boost-devel": ensure => present }
if ($::operatingsystemmajrelease >= 7) {
	package { "python-sphinx": ensure => present }
	package { "gtest": ensure => present }
	package { "gtest-devel": ensure => present }
}
package { "lua-devel": ensure => present }
package { "redhat-lsb": ensure => present }
yumgroup { "Development Tools": ensure => present  }
file { "/home/vagrant/build.sh":
    ensure  => "present",
    mode    => 755,
	source  => "/etc/puppet/files/build.sh"
}

#exec { "yum-update":
#    command => "/usr/bin/yum -y update"
#}

class { 'epel':
} -> 
package { "protobuf-devel": ensure => present
}
package { "protobuf-compiler": ensure => present
} ->
package { "protobuf-python": ensure => present
} ->
package { "cryptopp": ensure => present
} ->
package { "cryptopp-devel": ensure => present
} ->
package { "python-jinja2": ensure => present
}

if ($::operatingsystemmajrelease < 7) {
	package { "python-argparse": ensure => present
	}
	file { "/usr/lib/python2.6/site-packages/google/protobuf/compiler": 
		ensure    => "directory", 	
	} ->
	file { "/usr/lib/python2.6/site-packages/google/protobuf/compiler/plugin_pb2.py": 
		ensure    => present, 
		source  => "/etc/puppet/files/plugin_pb2.py"
	} ->
	file { "/usr/lib/python2.6/site-packages/google/protobuf/compiler/__init__.py": 
		ensure    => present,
	}
}

#Exec["yum-update"] -> Package <| |>