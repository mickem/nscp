# NRPEClient

NRPE client can be used both from command line and from queries to check remote systes via NRPE as well as configure the NRPE server



## List of commands

A list of all available queries (check commands)

| Command                       | Description                                                                    |
|-------------------------------|--------------------------------------------------------------------------------|
| [check_nrpe](#check_nrpe)     | Request remote information via NRPE.                                           |
| [exec_nrpe](#exec_nrpe)       | Execute remote script via NRPE. (Most likely you want nrpe_query).             |
| [nrpe_forward](#nrpe_forward) | Forward the request as-is to remote host via NRPE.                             |
| [nrpe_query](#nrpe_query)     | Request remote information via NRPE.                                           |
| [submit_nrpe](#submit_nrpe)   | Submit information to remote host via NRPE. (Most likely you want nrpe_query). |




## List of Configuration


### Common Keys

| Path / Section                                                                  | Key                                                                       | Description           |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------|-----------------------|
| [/settings/NRPE/client](#/settings/NRPE/client)                                 | [channel](#/settings/NRPE/client_channel)                                 | CHANNEL               |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [address](#/settings/NRPE/client/targets/default_address)                 | TARGET ADDRESS        |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [allowed ciphers](#/settings/NRPE/client/targets/default_allowed ciphers) | ALLOWED CIPHERS       |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [certificate](#/settings/NRPE/client/targets/default_certificate)         | SSL CERTIFICATE       |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [insecure](#/settings/NRPE/client/targets/default_insecure)               | Insecure legacy mode  |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [payload length](#/settings/NRPE/client/targets/default_payload length)   | PAYLOAD LENGTH        |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [retries](#/settings/NRPE/client/targets/default_retries)                 | RETRIES               |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [timeout](#/settings/NRPE/client/targets/default_timeout)                 | TIMEOUT               |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [use ssl](#/settings/NRPE/client/targets/default_use ssl)                 | ENABLE SSL ENCRYPTION |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [verify mode](#/settings/NRPE/client/targets/default_verify mode)         | VERIFY MODE           |

### Advanced keys

| Path / Section                                                                  | Key                                                                             | Description        |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------------|--------------------|
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [ca](#/settings/NRPE/client/targets/default_ca)                                 | CA                 |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [certificate format](#/settings/NRPE/client/targets/default_certificate format) | CERTIFICATE FORMAT |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [certificate key](#/settings/NRPE/client/targets/default_certificate key)       | SSL CERTIFICATE    |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [dh](#/settings/NRPE/client/targets/default_dh)                                 | DH KEY             |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [host](#/settings/NRPE/client/targets/default_host)                             | TARGET HOST        |
| [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) | [port](#/settings/NRPE/client/targets/default_port)                             | TARGET PORT        |

### Sample keys

| Path / Section                                                                | Key                                                                            | Description           |
|-------------------------------------------------------------------------------|--------------------------------------------------------------------------------|-----------------------|
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [address](#/settings/NRPE/client/targets/sample_address)                       | TARGET ADDRESS        |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [allowed ciphers](#/settings/NRPE/client/targets/sample_allowed ciphers)       | ALLOWED CIPHERS       |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [ca](#/settings/NRPE/client/targets/sample_ca)                                 | CA                    |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [certificate](#/settings/NRPE/client/targets/sample_certificate)               | SSL CERTIFICATE       |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [certificate format](#/settings/NRPE/client/targets/sample_certificate format) | CERTIFICATE FORMAT    |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [certificate key](#/settings/NRPE/client/targets/sample_certificate key)       | SSL CERTIFICATE       |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [dh](#/settings/NRPE/client/targets/sample_dh)                                 | DH KEY                |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [host](#/settings/NRPE/client/targets/sample_host)                             | TARGET HOST           |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [insecure](#/settings/NRPE/client/targets/sample_insecure)                     | Insecure legacy mode  |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [payload length](#/settings/NRPE/client/targets/sample_payload length)         | PAYLOAD LENGTH        |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [port](#/settings/NRPE/client/targets/sample_port)                             | TARGET PORT           |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [retries](#/settings/NRPE/client/targets/sample_retries)                       | RETRIES               |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [timeout](#/settings/NRPE/client/targets/sample_timeout)                       | TIMEOUT               |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [use ssl](#/settings/NRPE/client/targets/sample_use ssl)                       | ENABLE SSL ENCRYPTION |
| [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) | [verify mode](#/settings/NRPE/client/targets/sample_verify mode)               | VERIFY MODE           |



# Queries

A quick reference for all available queries (check commands) in the NRPEClient module.

## check_nrpe

Request remote information via NRPE.


### Usage


| Option                                               | Default Value | Description                                                                                                                                                               |
|------------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [help](#check_nrpe_help)                             | N/A           | Show help screen (this screen)                                                                                                                                            |
| [help-pb](#check_nrpe_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| [show-default](#check_nrpe_show-default)             | N/A           | Show default values for a given command                                                                                                                                   |
| [help-short](#check_nrpe_help-short)                 | N/A           | Show help screen (short format).                                                                                                                                          |
| [host](#check_nrpe_host)                             |               | The host of the host running the server                                                                                                                                   |
| [port](#check_nrpe_port)                             |               | The port of the host running the server                                                                                                                                   |
| [address](#check_nrpe_address)                       |               | The address (host:port) of the host running the server                                                                                                                    |
| [timeout](#check_nrpe_timeout)                       |               | Number of seconds before connection times out (default=10)                                                                                                                |
| [target](#check_nrpe_target)                         |               | Target to use (lookup connection info from config)                                                                                                                        |
| [retry](#check_nrpe_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| [retries](#check_nrpe_retries)                       |               | legacy version of retry                                                                                                                                                   |
| [source-host](#check_nrpe_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [sender-host](#check_nrpe_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [command](#check_nrpe_command)                       |               | The name of the command that the remote daemon should run                                                                                                                 |
| [argument](#check_nrpe_argument)                     |               | Set command line arguments                                                                                                                                                |
| [separator](#check_nrpe_separator)                   |               | Separator to use for the batch command (default is |)                                                                                                                     |
| [batch](#check_nrpe_batch)                           |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| [certificate](#check_nrpe_certificate)               |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [dh](#check_nrpe_dh)                                 |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [certificate-key](#check_nrpe_certificate-key)       |               | Client certificate to use                                                                                                                                                 |
| [certificate-format](#check_nrpe_certificate-format) |               | Client certificate format                                                                                                                                                 |
| [ca](#check_nrpe_ca)                                 |               | Certificate authority                                                                                                                                                     |
| [verify](#check_nrpe_verify)                         |               | Client certificate format                                                                                                                                                 |
| [allowed-ciphers](#check_nrpe_allowed-ciphers)       |               | Client certificate format                                                                                                                                                 |
| [ssl](#check_nrpe_ssl)                               | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#check_nrpe_insecure)                     | N/A           | Use insecure legacy mode                                                                                                                                                  |
| [payload-length](#check_nrpe_payload-length)         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [buffer-length](#check_nrpe_buffer-length)           |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |


<a name="check_nrpe_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_nrpe_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_nrpe_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_nrpe_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_nrpe_host"/>
### host



**Description:**
The host of the host running the server

<a name="check_nrpe_port"/>
### port



**Description:**
The port of the host running the server

<a name="check_nrpe_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="check_nrpe_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="check_nrpe_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="check_nrpe_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="check_nrpe_retries"/>
### retries



**Description:**
legacy version of retry

<a name="check_nrpe_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="check_nrpe_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="check_nrpe_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="check_nrpe_argument"/>
### argument



**Description:**
Set command line arguments

<a name="check_nrpe_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="check_nrpe_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|argument|argument

<a name="check_nrpe_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="check_nrpe_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="check_nrpe_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="check_nrpe_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="check_nrpe_ca"/>
### ca



**Description:**
Certificate authority

<a name="check_nrpe_verify"/>
### verify



**Description:**
Client certificate format

<a name="check_nrpe_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="check_nrpe_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="check_nrpe_insecure"/>
### insecure



**Description:**
Use insecure legacy mode

<a name="check_nrpe_payload-length"/>
### payload-length



**Description:**
Length of payload (has to be same as on the server)

<a name="check_nrpe_buffer-length"/>
### buffer-length



**Description:**
Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.

## exec_nrpe

Execute remote script via NRPE. (Most likely you want nrpe_query).


### Usage


| Option                                              | Default Value | Description                                                                                                                                                               |
|-----------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [help](#exec_nrpe_help)                             | N/A           | Show help screen (this screen)                                                                                                                                            |
| [help-pb](#exec_nrpe_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| [show-default](#exec_nrpe_show-default)             | N/A           | Show default values for a given command                                                                                                                                   |
| [help-short](#exec_nrpe_help-short)                 | N/A           | Show help screen (short format).                                                                                                                                          |
| [host](#exec_nrpe_host)                             |               | The host of the host running the server                                                                                                                                   |
| [port](#exec_nrpe_port)                             |               | The port of the host running the server                                                                                                                                   |
| [address](#exec_nrpe_address)                       |               | The address (host:port) of the host running the server                                                                                                                    |
| [timeout](#exec_nrpe_timeout)                       |               | Number of seconds before connection times out (default=10)                                                                                                                |
| [target](#exec_nrpe_target)                         |               | Target to use (lookup connection info from config)                                                                                                                        |
| [retry](#exec_nrpe_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| [retries](#exec_nrpe_retries)                       |               | legacy version of retry                                                                                                                                                   |
| [source-host](#exec_nrpe_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [sender-host](#exec_nrpe_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [command](#exec_nrpe_command)                       |               | The name of the command that the remote daemon should run                                                                                                                 |
| [argument](#exec_nrpe_argument)                     |               | Set command line arguments                                                                                                                                                |
| [separator](#exec_nrpe_separator)                   |               | Separator to use for the batch command (default is |)                                                                                                                     |
| [batch](#exec_nrpe_batch)                           |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| [certificate](#exec_nrpe_certificate)               |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [dh](#exec_nrpe_dh)                                 |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [certificate-key](#exec_nrpe_certificate-key)       |               | Client certificate to use                                                                                                                                                 |
| [certificate-format](#exec_nrpe_certificate-format) |               | Client certificate format                                                                                                                                                 |
| [ca](#exec_nrpe_ca)                                 |               | Certificate authority                                                                                                                                                     |
| [verify](#exec_nrpe_verify)                         |               | Client certificate format                                                                                                                                                 |
| [allowed-ciphers](#exec_nrpe_allowed-ciphers)       |               | Client certificate format                                                                                                                                                 |
| [ssl](#exec_nrpe_ssl)                               | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#exec_nrpe_insecure)                     | N/A           | Use insecure legacy mode                                                                                                                                                  |
| [payload-length](#exec_nrpe_payload-length)         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [buffer-length](#exec_nrpe_buffer-length)           |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |


<a name="exec_nrpe_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="exec_nrpe_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="exec_nrpe_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="exec_nrpe_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="exec_nrpe_host"/>
### host



**Description:**
The host of the host running the server

<a name="exec_nrpe_port"/>
### port



**Description:**
The port of the host running the server

<a name="exec_nrpe_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="exec_nrpe_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="exec_nrpe_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="exec_nrpe_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="exec_nrpe_retries"/>
### retries



**Description:**
legacy version of retry

<a name="exec_nrpe_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="exec_nrpe_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="exec_nrpe_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="exec_nrpe_argument"/>
### argument



**Description:**
Set command line arguments

<a name="exec_nrpe_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="exec_nrpe_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|argument|argument

<a name="exec_nrpe_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="exec_nrpe_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="exec_nrpe_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="exec_nrpe_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="exec_nrpe_ca"/>
### ca



**Description:**
Certificate authority

<a name="exec_nrpe_verify"/>
### verify



**Description:**
Client certificate format

<a name="exec_nrpe_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="exec_nrpe_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="exec_nrpe_insecure"/>
### insecure



**Description:**
Use insecure legacy mode

<a name="exec_nrpe_payload-length"/>
### payload-length



**Description:**
Length of payload (has to be same as on the server)

<a name="exec_nrpe_buffer-length"/>
### buffer-length



**Description:**
Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.

## nrpe_forward

Forward the request as-is to remote host via NRPE.


### Usage


| Option               | Default Value | Description |
|----------------------|---------------|-------------|
| [*](#nrpe_forward_*) |               |             |


<a name="nrpe_forward_*"/>
### *



**Description:**


## nrpe_query

Request remote information via NRPE.


### Usage


| Option                                               | Default Value | Description                                                                                                                                                               |
|------------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [help](#nrpe_query_help)                             | N/A           | Show help screen (this screen)                                                                                                                                            |
| [help-pb](#nrpe_query_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| [show-default](#nrpe_query_show-default)             | N/A           | Show default values for a given command                                                                                                                                   |
| [help-short](#nrpe_query_help-short)                 | N/A           | Show help screen (short format).                                                                                                                                          |
| [host](#nrpe_query_host)                             |               | The host of the host running the server                                                                                                                                   |
| [port](#nrpe_query_port)                             |               | The port of the host running the server                                                                                                                                   |
| [address](#nrpe_query_address)                       |               | The address (host:port) of the host running the server                                                                                                                    |
| [timeout](#nrpe_query_timeout)                       |               | Number of seconds before connection times out (default=10)                                                                                                                |
| [target](#nrpe_query_target)                         |               | Target to use (lookup connection info from config)                                                                                                                        |
| [retry](#nrpe_query_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| [retries](#nrpe_query_retries)                       |               | legacy version of retry                                                                                                                                                   |
| [source-host](#nrpe_query_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [sender-host](#nrpe_query_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [command](#nrpe_query_command)                       |               | The name of the command that the remote daemon should run                                                                                                                 |
| [argument](#nrpe_query_argument)                     |               | Set command line arguments                                                                                                                                                |
| [separator](#nrpe_query_separator)                   |               | Separator to use for the batch command (default is |)                                                                                                                     |
| [batch](#nrpe_query_batch)                           |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| [certificate](#nrpe_query_certificate)               |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [dh](#nrpe_query_dh)                                 |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [certificate-key](#nrpe_query_certificate-key)       |               | Client certificate to use                                                                                                                                                 |
| [certificate-format](#nrpe_query_certificate-format) |               | Client certificate format                                                                                                                                                 |
| [ca](#nrpe_query_ca)                                 |               | Certificate authority                                                                                                                                                     |
| [verify](#nrpe_query_verify)                         |               | Client certificate format                                                                                                                                                 |
| [allowed-ciphers](#nrpe_query_allowed-ciphers)       |               | Client certificate format                                                                                                                                                 |
| [ssl](#nrpe_query_ssl)                               | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#nrpe_query_insecure)                     | N/A           | Use insecure legacy mode                                                                                                                                                  |
| [payload-length](#nrpe_query_payload-length)         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [buffer-length](#nrpe_query_buffer-length)           |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |


<a name="nrpe_query_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="nrpe_query_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="nrpe_query_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="nrpe_query_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="nrpe_query_host"/>
### host



**Description:**
The host of the host running the server

<a name="nrpe_query_port"/>
### port



**Description:**
The port of the host running the server

<a name="nrpe_query_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="nrpe_query_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="nrpe_query_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="nrpe_query_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="nrpe_query_retries"/>
### retries



**Description:**
legacy version of retry

<a name="nrpe_query_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="nrpe_query_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="nrpe_query_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="nrpe_query_argument"/>
### argument



**Description:**
Set command line arguments

<a name="nrpe_query_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="nrpe_query_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|argument|argument

<a name="nrpe_query_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="nrpe_query_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="nrpe_query_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="nrpe_query_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="nrpe_query_ca"/>
### ca



**Description:**
Certificate authority

<a name="nrpe_query_verify"/>
### verify



**Description:**
Client certificate format

<a name="nrpe_query_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="nrpe_query_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="nrpe_query_insecure"/>
### insecure



**Description:**
Use insecure legacy mode

<a name="nrpe_query_payload-length"/>
### payload-length



**Description:**
Length of payload (has to be same as on the server)

<a name="nrpe_query_buffer-length"/>
### buffer-length



**Description:**
Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.

## submit_nrpe

Submit information to remote host via NRPE. (Most likely you want nrpe_query).


### Usage


| Option                                                | Default Value | Description                                                                                                                                                               |
|-------------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [help](#submit_nrpe_help)                             | N/A           | Show help screen (this screen)                                                                                                                                            |
| [help-pb](#submit_nrpe_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| [show-default](#submit_nrpe_show-default)             | N/A           | Show default values for a given command                                                                                                                                   |
| [help-short](#submit_nrpe_help-short)                 | N/A           | Show help screen (short format).                                                                                                                                          |
| [host](#submit_nrpe_host)                             |               | The host of the host running the server                                                                                                                                   |
| [port](#submit_nrpe_port)                             |               | The port of the host running the server                                                                                                                                   |
| [address](#submit_nrpe_address)                       |               | The address (host:port) of the host running the server                                                                                                                    |
| [timeout](#submit_nrpe_timeout)                       |               | Number of seconds before connection times out (default=10)                                                                                                                |
| [target](#submit_nrpe_target)                         |               | Target to use (lookup connection info from config)                                                                                                                        |
| [retry](#submit_nrpe_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| [retries](#submit_nrpe_retries)                       |               | legacy version of retry                                                                                                                                                   |
| [source-host](#submit_nrpe_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [sender-host](#submit_nrpe_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [command](#submit_nrpe_command)                       |               | The name of the command that the remote daemon should run                                                                                                                 |
| [alias](#submit_nrpe_alias)                           |               | Same as command                                                                                                                                                           |
| [message](#submit_nrpe_message)                       |               | Message                                                                                                                                                                   |
| [result](#submit_nrpe_result)                         |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| [separator](#submit_nrpe_separator)                   |               | Separator to use for the batch command (default is |)                                                                                                                     |
| [batch](#submit_nrpe_batch)                           |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| [certificate](#submit_nrpe_certificate)               |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [dh](#submit_nrpe_dh)                                 |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [certificate-key](#submit_nrpe_certificate-key)       |               | Client certificate to use                                                                                                                                                 |
| [certificate-format](#submit_nrpe_certificate-format) |               | Client certificate format                                                                                                                                                 |
| [ca](#submit_nrpe_ca)                                 |               | Certificate authority                                                                                                                                                     |
| [verify](#submit_nrpe_verify)                         |               | Client certificate format                                                                                                                                                 |
| [allowed-ciphers](#submit_nrpe_allowed-ciphers)       |               | Client certificate format                                                                                                                                                 |
| [ssl](#submit_nrpe_ssl)                               | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#submit_nrpe_insecure)                     | N/A           | Use insecure legacy mode                                                                                                                                                  |
| [payload-length](#submit_nrpe_payload-length)         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [buffer-length](#submit_nrpe_buffer-length)           |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |


<a name="submit_nrpe_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="submit_nrpe_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="submit_nrpe_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="submit_nrpe_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="submit_nrpe_host"/>
### host



**Description:**
The host of the host running the server

<a name="submit_nrpe_port"/>
### port



**Description:**
The port of the host running the server

<a name="submit_nrpe_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="submit_nrpe_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="submit_nrpe_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="submit_nrpe_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="submit_nrpe_retries"/>
### retries



**Description:**
legacy version of retry

<a name="submit_nrpe_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_nrpe_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_nrpe_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="submit_nrpe_alias"/>
### alias



**Description:**
Same as command

<a name="submit_nrpe_message"/>
### message



**Description:**
Message

<a name="submit_nrpe_result"/>
### result



**Description:**
Result code either a number or OK, WARN, CRIT, UNKNOWN

<a name="submit_nrpe_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="submit_nrpe_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|result|message

<a name="submit_nrpe_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_nrpe_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_nrpe_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="submit_nrpe_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="submit_nrpe_ca"/>
### ca



**Description:**
Certificate authority

<a name="submit_nrpe_verify"/>
### verify



**Description:**
Client certificate format

<a name="submit_nrpe_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="submit_nrpe_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="submit_nrpe_insecure"/>
### insecure



**Description:**
Use insecure legacy mode

<a name="submit_nrpe_payload-length"/>
### payload-length



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_nrpe_buffer-length"/>
### buffer-length



**Description:**
Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.



# Configuration

<a name="/settings/NRPE/client"/>
## NRPE CLIENT SECTION

Section for NRPE active/passive check module.

```ini
# Section for NRPE active/passive check module.
[/settings/NRPE/client]
channel=NRPE

```


| Key                                       | Default Value | Description |
|-------------------------------------------|---------------|-------------|
| [channel](#/settings/NRPE/client_channel) | NRPE          | CHANNEL     |




<a name="/settings/NRPE/client_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/client](#/settings/NRPE/client) |
| Key:           | channel                                         |
| Default value: | `NRPE`                                          |
| Used by:       | NRPEClient                                      |


#### Sample

```
[/settings/NRPE/client]
# CHANNEL
channel=NRPE
```


<a name="/settings/NRPE/client/handlers"/>
## CLIENT HANDLER SECTION



```ini
# 
[/settings/NRPE/client/handlers]

```






<a name="/settings/NRPE/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/NRPE/client/targets]

```






<a name="/settings/NRPE/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/NRPE/client/targets/default]
retries=3
timeout=30

```


| Key                                                                             | Default Value | Description           |
|---------------------------------------------------------------------------------|---------------|-----------------------|
| [address](#/settings/NRPE/client/targets/default_address)                       |               | TARGET ADDRESS        |
| [allowed ciphers](#/settings/NRPE/client/targets/default_allowed ciphers)       |               | ALLOWED CIPHERS       |
| [ca](#/settings/NRPE/client/targets/default_ca)                                 |               | CA                    |
| [certificate](#/settings/NRPE/client/targets/default_certificate)               |               | SSL CERTIFICATE       |
| [certificate format](#/settings/NRPE/client/targets/default_certificate format) |               | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NRPE/client/targets/default_certificate key)       |               | SSL CERTIFICATE       |
| [dh](#/settings/NRPE/client/targets/default_dh)                                 |               | DH KEY                |
| [host](#/settings/NRPE/client/targets/default_host)                             |               | TARGET HOST           |
| [insecure](#/settings/NRPE/client/targets/default_insecure)                     |               | Insecure legacy mode  |
| [payload length](#/settings/NRPE/client/targets/default_payload length)         |               | PAYLOAD LENGTH        |
| [port](#/settings/NRPE/client/targets/default_port)                             |               | TARGET PORT           |
| [retries](#/settings/NRPE/client/targets/default_retries)                       | 3             | RETRIES               |
| [timeout](#/settings/NRPE/client/targets/default_timeout)                       | 30            | TIMEOUT               |
| [use ssl](#/settings/NRPE/client/targets/default_use ssl)                       |               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NRPE/client/targets/default_verify mode)               |               | VERIFY MODE           |




<a name="/settings/NRPE/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | address                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/NRPE/client/targets/default_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | allowed ciphers                                                                 |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# ALLOWED CIPHERS
allowed ciphers=
```


<a name="/settings/NRPE/client/targets/default_ca"/>
### ca

**CA**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | ca                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# CA
ca=
```


<a name="/settings/NRPE/client/targets/default_certificate"/>
### certificate

**SSL CERTIFICATE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | certificate                                                                     |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# SSL CERTIFICATE
certificate=
```


<a name="/settings/NRPE/client/targets/default_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | certificate format                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# CERTIFICATE FORMAT
certificate format=
```


<a name="/settings/NRPE/client/targets/default_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | certificate key                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NRPE/client/targets/default_dh"/>
### dh

**DH KEY**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | dh                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# DH KEY
dh=
```


<a name="/settings/NRPE/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | host                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/NRPE/client/targets/default_insecure"/>
### insecure

**Insecure legacy mode**

Use insecure legacy mode to connect to old NRPE server





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | insecure                                                                        |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# Insecure legacy mode
insecure=
```


<a name="/settings/NRPE/client/targets/default_payload length"/>
### payload length

**PAYLOAD LENGTH**

Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | payload length                                                                  |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# PAYLOAD LENGTH
payload length=
```


<a name="/settings/NRPE/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | port                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/NRPE/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | retries                                                                         |
| Default value: | `3`                                                                             |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/NRPE/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | timeout                                                                         |
| Default value: | `30`                                                                            |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/NRPE/client/targets/default_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | use ssl                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# ENABLE SSL ENCRYPTION
use ssl=
```


<a name="/settings/NRPE/client/targets/default_verify mode"/>
### verify mode

**VERIFY MODE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/default](#/settings/NRPE/client/targets/default) |
| Key:           | verify mode                                                                     |
| Default value: | _N/A_                                                                           |
| Used by:       | NRPEClient                                                                      |


#### Sample

```
[/settings/NRPE/client/targets/default]
# VERIFY MODE
verify mode=
```


<a name="/settings/NRPE/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/NRPE/client/targets/sample]
retries=3
timeout=30

```


| Key                                                                            | Default Value | Description           |
|--------------------------------------------------------------------------------|---------------|-----------------------|
| [address](#/settings/NRPE/client/targets/sample_address)                       |               | TARGET ADDRESS        |
| [allowed ciphers](#/settings/NRPE/client/targets/sample_allowed ciphers)       |               | ALLOWED CIPHERS       |
| [ca](#/settings/NRPE/client/targets/sample_ca)                                 |               | CA                    |
| [certificate](#/settings/NRPE/client/targets/sample_certificate)               |               | SSL CERTIFICATE       |
| [certificate format](#/settings/NRPE/client/targets/sample_certificate format) |               | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NRPE/client/targets/sample_certificate key)       |               | SSL CERTIFICATE       |
| [dh](#/settings/NRPE/client/targets/sample_dh)                                 |               | DH KEY                |
| [host](#/settings/NRPE/client/targets/sample_host)                             |               | TARGET HOST           |
| [insecure](#/settings/NRPE/client/targets/sample_insecure)                     |               | Insecure legacy mode  |
| [payload length](#/settings/NRPE/client/targets/sample_payload length)         |               | PAYLOAD LENGTH        |
| [port](#/settings/NRPE/client/targets/sample_port)                             |               | TARGET PORT           |
| [retries](#/settings/NRPE/client/targets/sample_retries)                       | 3             | RETRIES               |
| [timeout](#/settings/NRPE/client/targets/sample_timeout)                       | 30            | TIMEOUT               |
| [use ssl](#/settings/NRPE/client/targets/sample_use ssl)                       |               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NRPE/client/targets/sample_verify mode)               |               | VERIFY MODE           |




<a name="/settings/NRPE/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | address                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/NRPE/client/targets/sample_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | allowed ciphers                                                               |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# ALLOWED CIPHERS
allowed ciphers=
```


<a name="/settings/NRPE/client/targets/sample_ca"/>
### ca

**CA**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | ca                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# CA
ca=
```


<a name="/settings/NRPE/client/targets/sample_certificate"/>
### certificate

**SSL CERTIFICATE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | certificate                                                                   |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# SSL CERTIFICATE
certificate=
```


<a name="/settings/NRPE/client/targets/sample_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | certificate format                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# CERTIFICATE FORMAT
certificate format=
```


<a name="/settings/NRPE/client/targets/sample_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | certificate key                                                               |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NRPE/client/targets/sample_dh"/>
### dh

**DH KEY**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | dh                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# DH KEY
dh=
```


<a name="/settings/NRPE/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | host                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/NRPE/client/targets/sample_insecure"/>
### insecure

**Insecure legacy mode**

Use insecure legacy mode to connect to old NRPE server





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | insecure                                                                      |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# Insecure legacy mode
insecure=
```


<a name="/settings/NRPE/client/targets/sample_payload length"/>
### payload length

**PAYLOAD LENGTH**

Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | payload length                                                                |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# PAYLOAD LENGTH
payload length=
```


<a name="/settings/NRPE/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | port                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/NRPE/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | retries                                                                       |
| Default value: | `3`                                                                           |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/NRPE/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | timeout                                                                       |
| Default value: | `30`                                                                          |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# TIMEOUT
timeout=30
```


<a name="/settings/NRPE/client/targets/sample_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | use ssl                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# ENABLE SSL ENCRYPTION
use ssl=
```


<a name="/settings/NRPE/client/targets/sample_verify mode"/>
### verify mode

**VERIFY MODE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NRPE/client/targets/sample](#/settings/NRPE/client/targets/sample) |
| Key:           | verify mode                                                                   |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NRPEClient                                                                    |


#### Sample

```
[/settings/NRPE/client/targets/sample]
# VERIFY MODE
verify mode=
```


