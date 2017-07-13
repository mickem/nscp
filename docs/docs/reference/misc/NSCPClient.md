# NSCPClient

NSCP client can be used both from command line and from queries to check remote systes via NSCP (REST)



## List of commands

A list of all available queries (check commands)

| Command                                   | Description                                        |
|-------------------------------------------|----------------------------------------------------|
| [check_remote_nscp](#check_remote_nscp)   | Request remote information via NSCP.               |
| [exec_remote_nscp](#exec_remote_nscp)     | Execute remote script via NSCP.                    |
| [remote_nscp_query](#remote_nscp_query)   | Request remote information via NSCP.               |
| [remote_nscpforward](#remote_nscpforward) | Forward the request as-is to remote host via NSCP. |
| [submit_remote_nscp](#submit_remote_nscp) | Submit information to remote host via NSCP.        |




## List of Configuration


### Common Keys

| Path / Section                                                                  | Key                                                                       | Description           |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------|-----------------------|
| [/settings/NSCP/client](#/settings/NSCP/client)                                 | [channel](#/settings/NSCP/client_channel)                                 | CHANNEL               |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [address](#/settings/NSCP/client/targets/default_address)                 | TARGET ADDRESS        |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [allowed ciphers](#/settings/NSCP/client/targets/default_allowed ciphers) | ALLOWED CIPHERS       |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [certificate](#/settings/NSCP/client/targets/default_certificate)         | SSL CERTIFICATE       |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [password](#/settings/NSCP/client/targets/default_password)               | PASSWORD              |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [retries](#/settings/NSCP/client/targets/default_retries)                 | RETRIES               |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [timeout](#/settings/NSCP/client/targets/default_timeout)                 | TIMEOUT               |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [use ssl](#/settings/NSCP/client/targets/default_use ssl)                 | ENABLE SSL ENCRYPTION |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [verify mode](#/settings/NSCP/client/targets/default_verify mode)         | VERIFY MODE           |

### Advanced keys

| Path / Section                                                                  | Key                                                                             | Description        |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------------|--------------------|
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [ca](#/settings/NSCP/client/targets/default_ca)                                 | CA                 |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [certificate format](#/settings/NSCP/client/targets/default_certificate format) | CERTIFICATE FORMAT |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [certificate key](#/settings/NSCP/client/targets/default_certificate key)       | SSL CERTIFICATE    |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [dh](#/settings/NSCP/client/targets/default_dh)                                 | DH KEY             |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [host](#/settings/NSCP/client/targets/default_host)                             | TARGET HOST        |
| [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) | [port](#/settings/NSCP/client/targets/default_port)                             | TARGET PORT        |

### Sample keys

| Path / Section                                                                | Key                                                                            | Description           |
|-------------------------------------------------------------------------------|--------------------------------------------------------------------------------|-----------------------|
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [address](#/settings/NSCP/client/targets/sample_address)                       | TARGET ADDRESS        |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [allowed ciphers](#/settings/NSCP/client/targets/sample_allowed ciphers)       | ALLOWED CIPHERS       |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [ca](#/settings/NSCP/client/targets/sample_ca)                                 | CA                    |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [certificate](#/settings/NSCP/client/targets/sample_certificate)               | SSL CERTIFICATE       |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [certificate format](#/settings/NSCP/client/targets/sample_certificate format) | CERTIFICATE FORMAT    |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [certificate key](#/settings/NSCP/client/targets/sample_certificate key)       | SSL CERTIFICATE       |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [dh](#/settings/NSCP/client/targets/sample_dh)                                 | DH KEY                |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [host](#/settings/NSCP/client/targets/sample_host)                             | TARGET HOST           |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [password](#/settings/NSCP/client/targets/sample_password)                     | PASSWORD              |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [port](#/settings/NSCP/client/targets/sample_port)                             | TARGET PORT           |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [retries](#/settings/NSCP/client/targets/sample_retries)                       | RETRIES               |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [timeout](#/settings/NSCP/client/targets/sample_timeout)                       | TIMEOUT               |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [use ssl](#/settings/NSCP/client/targets/sample_use ssl)                       | ENABLE SSL ENCRYPTION |
| [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) | [verify mode](#/settings/NSCP/client/targets/sample_verify mode)               | VERIFY MODE           |



# Queries

A quick reference for all available queries (check commands) in the NSCPClient module.

## check_remote_nscp

Request remote information via NSCP.


### Usage


| Option                                                      | Default Value | Description                                                                           |
|-------------------------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#check_remote_nscp_help)                             | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#check_remote_nscp_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#check_remote_nscp_show-default)             | N/A           | Show default values for a given command                                               |
| [help-short](#check_remote_nscp_help-short)                 | N/A           | Show help screen (short format).                                                      |
| [host](#check_remote_nscp_host)                             |               | The host of the host running the server                                               |
| [port](#check_remote_nscp_port)                             |               | The port of the host running the server                                               |
| [address](#check_remote_nscp_address)                       |               | The address (host:port) of the host running the server                                |
| [timeout](#check_remote_nscp_timeout)                       |               | Number of seconds before connection times out (default=10)                            |
| [target](#check_remote_nscp_target)                         |               | Target to use (lookup connection info from config)                                    |
| [retry](#check_remote_nscp_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#check_remote_nscp_retries)                       |               | legacy version of retry                                                               |
| [source-host](#check_remote_nscp_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#check_remote_nscp_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#check_remote_nscp_command)                       |               | The name of the command that the remote daemon should run                             |
| [argument](#check_remote_nscp_argument)                     |               | Set command line arguments                                                            |
| [separator](#check_remote_nscp_separator)                   |               | Separator to use for the batch command (default is |)                                 |
| [batch](#check_remote_nscp_batch)                           |               | Add multiple records using the separator format is: command|argument|argument         |
| [certificate](#check_remote_nscp_certificate)               |               | Length of payload (has to be same as on the server)                                   |
| [dh](#check_remote_nscp_dh)                                 |               | Length of payload (has to be same as on the server)                                   |
| [certificate-key](#check_remote_nscp_certificate-key)       |               | Client certificate to use                                                             |
| [certificate-format](#check_remote_nscp_certificate-format) |               | Client certificate format                                                             |
| [ca](#check_remote_nscp_ca)                                 |               | Certificate authority                                                                 |
| [verify](#check_remote_nscp_verify)                         |               | Client certificate format                                                             |
| [allowed-ciphers](#check_remote_nscp_allowed-ciphers)       |               | Client certificate format                                                             |
| [ssl](#check_remote_nscp_ssl)                               | 1             | Initial an ssl handshake with the server.                                             |
| [password](#check_remote_nscp_password)                     |               | Password                                                                              |


<a name="check_remote_nscp_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_remote_nscp_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_remote_nscp_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_remote_nscp_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_remote_nscp_host"/>
### host



**Description:**
The host of the host running the server

<a name="check_remote_nscp_port"/>
### port



**Description:**
The port of the host running the server

<a name="check_remote_nscp_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="check_remote_nscp_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="check_remote_nscp_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="check_remote_nscp_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="check_remote_nscp_retries"/>
### retries



**Description:**
legacy version of retry

<a name="check_remote_nscp_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="check_remote_nscp_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="check_remote_nscp_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="check_remote_nscp_argument"/>
### argument



**Description:**
Set command line arguments

<a name="check_remote_nscp_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="check_remote_nscp_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|argument|argument

<a name="check_remote_nscp_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="check_remote_nscp_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="check_remote_nscp_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="check_remote_nscp_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="check_remote_nscp_ca"/>
### ca



**Description:**
Certificate authority

<a name="check_remote_nscp_verify"/>
### verify



**Description:**
Client certificate format

<a name="check_remote_nscp_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="check_remote_nscp_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="check_remote_nscp_password"/>
### password



**Description:**
Password

## exec_remote_nscp

Execute remote script via NSCP.


### Usage


| Option                                                     | Default Value | Description                                                                           |
|------------------------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#exec_remote_nscp_help)                             | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#exec_remote_nscp_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#exec_remote_nscp_show-default)             | N/A           | Show default values for a given command                                               |
| [help-short](#exec_remote_nscp_help-short)                 | N/A           | Show help screen (short format).                                                      |
| [host](#exec_remote_nscp_host)                             |               | The host of the host running the server                                               |
| [port](#exec_remote_nscp_port)                             |               | The port of the host running the server                                               |
| [address](#exec_remote_nscp_address)                       |               | The address (host:port) of the host running the server                                |
| [timeout](#exec_remote_nscp_timeout)                       |               | Number of seconds before connection times out (default=10)                            |
| [target](#exec_remote_nscp_target)                         |               | Target to use (lookup connection info from config)                                    |
| [retry](#exec_remote_nscp_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#exec_remote_nscp_retries)                       |               | legacy version of retry                                                               |
| [source-host](#exec_remote_nscp_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#exec_remote_nscp_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#exec_remote_nscp_command)                       |               | The name of the command that the remote daemon should run                             |
| [argument](#exec_remote_nscp_argument)                     |               | Set command line arguments                                                            |
| [separator](#exec_remote_nscp_separator)                   |               | Separator to use for the batch command (default is |)                                 |
| [batch](#exec_remote_nscp_batch)                           |               | Add multiple records using the separator format is: command|argument|argument         |
| [certificate](#exec_remote_nscp_certificate)               |               | Length of payload (has to be same as on the server)                                   |
| [dh](#exec_remote_nscp_dh)                                 |               | Length of payload (has to be same as on the server)                                   |
| [certificate-key](#exec_remote_nscp_certificate-key)       |               | Client certificate to use                                                             |
| [certificate-format](#exec_remote_nscp_certificate-format) |               | Client certificate format                                                             |
| [ca](#exec_remote_nscp_ca)                                 |               | Certificate authority                                                                 |
| [verify](#exec_remote_nscp_verify)                         |               | Client certificate format                                                             |
| [allowed-ciphers](#exec_remote_nscp_allowed-ciphers)       |               | Client certificate format                                                             |
| [ssl](#exec_remote_nscp_ssl)                               | 1             | Initial an ssl handshake with the server.                                             |
| [password](#exec_remote_nscp_password)                     |               | Password                                                                              |


<a name="exec_remote_nscp_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="exec_remote_nscp_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="exec_remote_nscp_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="exec_remote_nscp_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="exec_remote_nscp_host"/>
### host



**Description:**
The host of the host running the server

<a name="exec_remote_nscp_port"/>
### port



**Description:**
The port of the host running the server

<a name="exec_remote_nscp_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="exec_remote_nscp_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="exec_remote_nscp_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="exec_remote_nscp_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="exec_remote_nscp_retries"/>
### retries



**Description:**
legacy version of retry

<a name="exec_remote_nscp_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="exec_remote_nscp_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="exec_remote_nscp_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="exec_remote_nscp_argument"/>
### argument



**Description:**
Set command line arguments

<a name="exec_remote_nscp_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="exec_remote_nscp_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|argument|argument

<a name="exec_remote_nscp_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="exec_remote_nscp_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="exec_remote_nscp_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="exec_remote_nscp_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="exec_remote_nscp_ca"/>
### ca



**Description:**
Certificate authority

<a name="exec_remote_nscp_verify"/>
### verify



**Description:**
Client certificate format

<a name="exec_remote_nscp_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="exec_remote_nscp_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="exec_remote_nscp_password"/>
### password



**Description:**
Password

## remote_nscp_query

Request remote information via NSCP.


### Usage


| Option                                                      | Default Value | Description                                                                           |
|-------------------------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#remote_nscp_query_help)                             | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#remote_nscp_query_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#remote_nscp_query_show-default)             | N/A           | Show default values for a given command                                               |
| [help-short](#remote_nscp_query_help-short)                 | N/A           | Show help screen (short format).                                                      |
| [host](#remote_nscp_query_host)                             |               | The host of the host running the server                                               |
| [port](#remote_nscp_query_port)                             |               | The port of the host running the server                                               |
| [address](#remote_nscp_query_address)                       |               | The address (host:port) of the host running the server                                |
| [timeout](#remote_nscp_query_timeout)                       |               | Number of seconds before connection times out (default=10)                            |
| [target](#remote_nscp_query_target)                         |               | Target to use (lookup connection info from config)                                    |
| [retry](#remote_nscp_query_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#remote_nscp_query_retries)                       |               | legacy version of retry                                                               |
| [source-host](#remote_nscp_query_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#remote_nscp_query_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#remote_nscp_query_command)                       |               | The name of the command that the remote daemon should run                             |
| [argument](#remote_nscp_query_argument)                     |               | Set command line arguments                                                            |
| [separator](#remote_nscp_query_separator)                   |               | Separator to use for the batch command (default is |)                                 |
| [batch](#remote_nscp_query_batch)                           |               | Add multiple records using the separator format is: command|argument|argument         |
| [certificate](#remote_nscp_query_certificate)               |               | Length of payload (has to be same as on the server)                                   |
| [dh](#remote_nscp_query_dh)                                 |               | Length of payload (has to be same as on the server)                                   |
| [certificate-key](#remote_nscp_query_certificate-key)       |               | Client certificate to use                                                             |
| [certificate-format](#remote_nscp_query_certificate-format) |               | Client certificate format                                                             |
| [ca](#remote_nscp_query_ca)                                 |               | Certificate authority                                                                 |
| [verify](#remote_nscp_query_verify)                         |               | Client certificate format                                                             |
| [allowed-ciphers](#remote_nscp_query_allowed-ciphers)       |               | Client certificate format                                                             |
| [ssl](#remote_nscp_query_ssl)                               | 1             | Initial an ssl handshake with the server.                                             |
| [password](#remote_nscp_query_password)                     |               | Password                                                                              |


<a name="remote_nscp_query_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="remote_nscp_query_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="remote_nscp_query_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="remote_nscp_query_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="remote_nscp_query_host"/>
### host



**Description:**
The host of the host running the server

<a name="remote_nscp_query_port"/>
### port



**Description:**
The port of the host running the server

<a name="remote_nscp_query_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="remote_nscp_query_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="remote_nscp_query_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="remote_nscp_query_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="remote_nscp_query_retries"/>
### retries



**Description:**
legacy version of retry

<a name="remote_nscp_query_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="remote_nscp_query_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="remote_nscp_query_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="remote_nscp_query_argument"/>
### argument



**Description:**
Set command line arguments

<a name="remote_nscp_query_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="remote_nscp_query_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|argument|argument

<a name="remote_nscp_query_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="remote_nscp_query_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="remote_nscp_query_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="remote_nscp_query_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="remote_nscp_query_ca"/>
### ca



**Description:**
Certificate authority

<a name="remote_nscp_query_verify"/>
### verify



**Description:**
Client certificate format

<a name="remote_nscp_query_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="remote_nscp_query_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="remote_nscp_query_password"/>
### password



**Description:**
Password

## remote_nscpforward

Forward the request as-is to remote host via NSCP.


### Usage




## submit_remote_nscp

Submit information to remote host via NSCP.


### Usage


| Option                                                       | Default Value | Description                                                                           |
|--------------------------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#submit_remote_nscp_help)                             | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#submit_remote_nscp_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#submit_remote_nscp_show-default)             | N/A           | Show default values for a given command                                               |
| [help-short](#submit_remote_nscp_help-short)                 | N/A           | Show help screen (short format).                                                      |
| [host](#submit_remote_nscp_host)                             |               | The host of the host running the server                                               |
| [port](#submit_remote_nscp_port)                             |               | The port of the host running the server                                               |
| [address](#submit_remote_nscp_address)                       |               | The address (host:port) of the host running the server                                |
| [timeout](#submit_remote_nscp_timeout)                       |               | Number of seconds before connection times out (default=10)                            |
| [target](#submit_remote_nscp_target)                         |               | Target to use (lookup connection info from config)                                    |
| [retry](#submit_remote_nscp_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#submit_remote_nscp_retries)                       |               | legacy version of retry                                                               |
| [source-host](#submit_remote_nscp_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#submit_remote_nscp_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#submit_remote_nscp_command)                       |               | The name of the command that the remote daemon should run                             |
| [alias](#submit_remote_nscp_alias)                           |               | Same as command                                                                       |
| [message](#submit_remote_nscp_message)                       |               | Message                                                                               |
| [result](#submit_remote_nscp_result)                         |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| [separator](#submit_remote_nscp_separator)                   |               | Separator to use for the batch command (default is |)                                 |
| [batch](#submit_remote_nscp_batch)                           |               | Add multiple records using the separator format is: command|result|message            |
| [certificate](#submit_remote_nscp_certificate)               |               | Length of payload (has to be same as on the server)                                   |
| [dh](#submit_remote_nscp_dh)                                 |               | Length of payload (has to be same as on the server)                                   |
| [certificate-key](#submit_remote_nscp_certificate-key)       |               | Client certificate to use                                                             |
| [certificate-format](#submit_remote_nscp_certificate-format) |               | Client certificate format                                                             |
| [ca](#submit_remote_nscp_ca)                                 |               | Certificate authority                                                                 |
| [verify](#submit_remote_nscp_verify)                         |               | Client certificate format                                                             |
| [allowed-ciphers](#submit_remote_nscp_allowed-ciphers)       |               | Client certificate format                                                             |
| [ssl](#submit_remote_nscp_ssl)                               | 1             | Initial an ssl handshake with the server.                                             |
| [password](#submit_remote_nscp_password)                     |               | Password                                                                              |


<a name="submit_remote_nscp_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="submit_remote_nscp_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="submit_remote_nscp_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="submit_remote_nscp_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="submit_remote_nscp_host"/>
### host



**Description:**
The host of the host running the server

<a name="submit_remote_nscp_port"/>
### port



**Description:**
The port of the host running the server

<a name="submit_remote_nscp_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="submit_remote_nscp_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="submit_remote_nscp_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="submit_remote_nscp_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="submit_remote_nscp_retries"/>
### retries



**Description:**
legacy version of retry

<a name="submit_remote_nscp_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_remote_nscp_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_remote_nscp_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="submit_remote_nscp_alias"/>
### alias



**Description:**
Same as command

<a name="submit_remote_nscp_message"/>
### message



**Description:**
Message

<a name="submit_remote_nscp_result"/>
### result



**Description:**
Result code either a number or OK, WARN, CRIT, UNKNOWN

<a name="submit_remote_nscp_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="submit_remote_nscp_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|result|message

<a name="submit_remote_nscp_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_remote_nscp_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_remote_nscp_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="submit_remote_nscp_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="submit_remote_nscp_ca"/>
### ca



**Description:**
Certificate authority

<a name="submit_remote_nscp_verify"/>
### verify



**Description:**
Client certificate format

<a name="submit_remote_nscp_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="submit_remote_nscp_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="submit_remote_nscp_password"/>
### password



**Description:**
Password



# Configuration

<a name="/settings/NSCP/client"/>
## NSCP CLIENT SECTION

Section for NSCP active/passive check module.

```ini
# Section for NSCP active/passive check module.
[/settings/NSCP/client]
channel=NSCP

```


| Key                                       | Default Value | Description |
|-------------------------------------------|---------------|-------------|
| [channel](#/settings/NSCP/client_channel) | NSCP          | CHANNEL     |




<a name="/settings/NSCP/client_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCP/client](#/settings/NSCP/client) |
| Key:           | channel                                         |
| Default value: | `NSCP`                                          |
| Used by:       | NSCPClient                                      |


#### Sample

```
[/settings/NSCP/client]
# CHANNEL
channel=NSCP
```


<a name="/settings/NSCP/client/handlers"/>
## CLIENT HANDLER SECTION



```ini
# 
[/settings/NSCP/client/handlers]

```






<a name="/settings/NSCP/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/NSCP/client/targets]

```






<a name="/settings/NSCP/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/NSCP/client/targets/default]
retries=3
timeout=30

```


| Key                                                                             | Default Value | Description           |
|---------------------------------------------------------------------------------|---------------|-----------------------|
| [address](#/settings/NSCP/client/targets/default_address)                       |               | TARGET ADDRESS        |
| [allowed ciphers](#/settings/NSCP/client/targets/default_allowed ciphers)       |               | ALLOWED CIPHERS       |
| [ca](#/settings/NSCP/client/targets/default_ca)                                 |               | CA                    |
| [certificate](#/settings/NSCP/client/targets/default_certificate)               |               | SSL CERTIFICATE       |
| [certificate format](#/settings/NSCP/client/targets/default_certificate format) |               | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NSCP/client/targets/default_certificate key)       |               | SSL CERTIFICATE       |
| [dh](#/settings/NSCP/client/targets/default_dh)                                 |               | DH KEY                |
| [host](#/settings/NSCP/client/targets/default_host)                             |               | TARGET HOST           |
| [password](#/settings/NSCP/client/targets/default_password)                     |               | PASSWORD              |
| [port](#/settings/NSCP/client/targets/default_port)                             |               | TARGET PORT           |
| [retries](#/settings/NSCP/client/targets/default_retries)                       | 3             | RETRIES               |
| [timeout](#/settings/NSCP/client/targets/default_timeout)                       | 30            | TIMEOUT               |
| [use ssl](#/settings/NSCP/client/targets/default_use ssl)                       |               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NSCP/client/targets/default_verify mode)               |               | VERIFY MODE           |




<a name="/settings/NSCP/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | address                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/NSCP/client/targets/default_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | allowed ciphers                                                                 |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# ALLOWED CIPHERS
allowed ciphers=
```


<a name="/settings/NSCP/client/targets/default_ca"/>
### ca

**CA**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | ca                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# CA
ca=
```


<a name="/settings/NSCP/client/targets/default_certificate"/>
### certificate

**SSL CERTIFICATE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | certificate                                                                     |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# SSL CERTIFICATE
certificate=
```


<a name="/settings/NSCP/client/targets/default_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | certificate format                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# CERTIFICATE FORMAT
certificate format=
```


<a name="/settings/NSCP/client/targets/default_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | certificate key                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NSCP/client/targets/default_dh"/>
### dh

**DH KEY**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | dh                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# DH KEY
dh=
```


<a name="/settings/NSCP/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | host                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/NSCP/client/targets/default_password"/>
### password

**PASSWORD**

The password to use to authenticate towards the server.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | password                                                                        |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# PASSWORD
password=
```


<a name="/settings/NSCP/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | port                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/NSCP/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | retries                                                                         |
| Default value: | `3`                                                                             |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/NSCP/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | timeout                                                                         |
| Default value: | `30`                                                                            |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/NSCP/client/targets/default_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | use ssl                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# ENABLE SSL ENCRYPTION
use ssl=
```


<a name="/settings/NSCP/client/targets/default_verify mode"/>
### verify mode

**VERIFY MODE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/default](#/settings/NSCP/client/targets/default) |
| Key:           | verify mode                                                                     |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCPClient                                                                      |


#### Sample

```
[/settings/NSCP/client/targets/default]
# VERIFY MODE
verify mode=
```


<a name="/settings/NSCP/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/NSCP/client/targets/sample]
retries=3
timeout=30

```


| Key                                                                            | Default Value | Description           |
|--------------------------------------------------------------------------------|---------------|-----------------------|
| [address](#/settings/NSCP/client/targets/sample_address)                       |               | TARGET ADDRESS        |
| [allowed ciphers](#/settings/NSCP/client/targets/sample_allowed ciphers)       |               | ALLOWED CIPHERS       |
| [ca](#/settings/NSCP/client/targets/sample_ca)                                 |               | CA                    |
| [certificate](#/settings/NSCP/client/targets/sample_certificate)               |               | SSL CERTIFICATE       |
| [certificate format](#/settings/NSCP/client/targets/sample_certificate format) |               | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NSCP/client/targets/sample_certificate key)       |               | SSL CERTIFICATE       |
| [dh](#/settings/NSCP/client/targets/sample_dh)                                 |               | DH KEY                |
| [host](#/settings/NSCP/client/targets/sample_host)                             |               | TARGET HOST           |
| [password](#/settings/NSCP/client/targets/sample_password)                     |               | PASSWORD              |
| [port](#/settings/NSCP/client/targets/sample_port)                             |               | TARGET PORT           |
| [retries](#/settings/NSCP/client/targets/sample_retries)                       | 3             | RETRIES               |
| [timeout](#/settings/NSCP/client/targets/sample_timeout)                       | 30            | TIMEOUT               |
| [use ssl](#/settings/NSCP/client/targets/sample_use ssl)                       |               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NSCP/client/targets/sample_verify mode)               |               | VERIFY MODE           |




<a name="/settings/NSCP/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | address                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/NSCP/client/targets/sample_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | allowed ciphers                                                               |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# ALLOWED CIPHERS
allowed ciphers=
```


<a name="/settings/NSCP/client/targets/sample_ca"/>
### ca

**CA**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | ca                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# CA
ca=
```


<a name="/settings/NSCP/client/targets/sample_certificate"/>
### certificate

**SSL CERTIFICATE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | certificate                                                                   |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# SSL CERTIFICATE
certificate=
```


<a name="/settings/NSCP/client/targets/sample_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | certificate format                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# CERTIFICATE FORMAT
certificate format=
```


<a name="/settings/NSCP/client/targets/sample_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | certificate key                                                               |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NSCP/client/targets/sample_dh"/>
### dh

**DH KEY**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | dh                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# DH KEY
dh=
```


<a name="/settings/NSCP/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | host                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/NSCP/client/targets/sample_password"/>
### password

**PASSWORD**

The password to use to authenticate towards the server.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | password                                                                      |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# PASSWORD
password=
```


<a name="/settings/NSCP/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | port                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/NSCP/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | retries                                                                       |
| Default value: | `3`                                                                           |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/NSCP/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | timeout                                                                       |
| Default value: | `30`                                                                          |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# TIMEOUT
timeout=30
```


<a name="/settings/NSCP/client/targets/sample_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | use ssl                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# ENABLE SSL ENCRYPTION
use ssl=
```


<a name="/settings/NSCP/client/targets/sample_verify mode"/>
### verify mode

**VERIFY MODE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCP/client/targets/sample](#/settings/NSCP/client/targets/sample) |
| Key:           | verify mode                                                                   |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCPClient                                                                    |


#### Sample

```
[/settings/NSCP/client/targets/sample]
# VERIFY MODE
verify mode=
```


