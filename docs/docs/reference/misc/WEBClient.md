<dl>
  <dt>.. module :  : WEBClient</dt>
  <dd>:synopsis: WEB client can be used both from command line and from queries to check remote systes via WEB(REST)</dd>
</dl>
# :module:`WEBClient` --- WEBClient #

WEB client can be used both from command line and from queries to check remote systes via WEB(REST)

**Queries (Overview)**:

1. list of all available queries (check commands)
   

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Command", "Description"</p>
    <p>:query:`web_exec` | Execute remote script via WEB.</p>
    <p>:query:`web_forward` | Forward the request as-is to remote host via WEB.</p>
    <p>:query:`web_query` | Request remote information via WEB.</p>
    <p>:query:`web_submit` | Submit information to remote host via WEB.</p>
  </dd>
</dl>
**Commands (Overview)**: 

**TODO:** Add a list of all external commands (this is not check commands)

**Configuration (Overview)**:

Common Keys:

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Path / Section", "Key", "Description"</p>
    <p>:confpath:`/settings/NRPE/client` | :confkey:`~/settings/NRPE/client.channel` | CHANNEL</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.address` | TARGET ADDRESS</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.certificate` | SSL CERTIFICATE</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.certificate key` | SSL CERTIFICATE KEY</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.insecure` | Insecure legacy mode</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.payload length` | PAYLOAD LENGTH</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.timeout` | TIMEOUT</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.use ssl` | ENABLE SSL ENCRYPTION</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.verify mode` | VERIFY MODE</p>
  </dd>
</dl>
Advanced keys:

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Path / Section", "Key", "Default Value", "Description"</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.alias` | ALIAS</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.allowed ciphers` | ALLOWED CIPHERS</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.ca` | CA</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.certificate format` | CERTIFICATE FORMAT</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.dh` | DH KEY</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.host` | TARGET HOST</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.is template` | IS TEMPLATE</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.parent` | PARENT</p>
    <p>:confpath:`/settings/NRPE/client/targets/default` | :confkey:`~/settings/NRPE/client/targets/default.port` | TARGET PORT</p>
  </dd>
</dl>
Sample keys:

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Path / Section", "Key", "Default Value", "Description"</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.address` | TARGET ADDRESS</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.alias` | ALIAS</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.allowed ciphers` | ALLOWED CIPHERS</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.ca` | CA</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.certificate` | SSL CERTIFICATE</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.certificate format` | CERTIFICATE FORMAT</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.certificate key` | SSL CERTIFICATE KEY</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.dh` | DH KEY</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.host` | TARGET HOST</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.insecure` | Insecure legacy mode</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.is template` | IS TEMPLATE</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.parent` | PARENT</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.payload length` | PAYLOAD LENGTH</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.port` | TARGET PORT</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.timeout` | TIMEOUT</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.use ssl` | ENABLE SSL ENCRYPTION</p>
    <p>:confpath:`/settings/NRPE/client/targets/sample` | :confkey:`~/settings/NRPE/client/targets/sample.verify mode` | VERIFY MODE</p>
  </dd>
</dl>
## Queries ##

1. quick reference for all available queries (check commands) in the WEBClient module.
   

### :query:`web_exec` ###

    :synopsis: Execute remote script via WEB.

**Usage:**

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Option", "Default Value", "Description"</p>
    <p>:option:`help` | N/A | Show help screen (this screen)</p>
    <p>:option:`help-pb` | N/A | Show help screen as a protocol buffer payload</p>
    <p>:option:`show-default` | N/A | Show default values for a given command</p>
    <p>:option:`help-short` | N/A | Show help screen (short format).</p>
    <p>:option:`host` |  | The host of the host running the server</p>
    <p>:option:`port` |  | The port of the host running the server</p>
    <p>:option:`address` |  | The address (host:port) of the host running the server</p>
    <p>:option:`timeout` |  | Number of seconds before connection times out (default=10)</p>
    <p>:option:`target` |  | Target to use (lookup connection info from config)</p>
    <p>:option:`retry` |  | Number of times ti retry a failed connection attempt (default=2)</p>
    <p>:option:`command` |  | The name of the command that the remote daemon should run</p>
    <p>:option:`arguments` |  | list of arguments</p>
    <p>:option:`no-ssl` | N/A | Do not initial an ssl handshake with the server, talk in plain-text.</p>
    <p>:option:`certificate` |  | Length of payload (has to be same as on the server)</p>
    <p>:option:`dh` |  | The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
    <p>:option:`certificate-key` |  | Client certificate to use</p>
    <p>:option:`certificate-format` |  | Client certificate format (default is PEM)</p>
    <p>:option:`insecure` | N/A | Use insecure legacy mode</p>
    <p>:option:`ca` |  | A file representing the Certificate authority used to validate peer certificates</p>
    <p>:option:`verify` |  | Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
    <p>:option:`allowed-ciphers` |  | Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
    <p>:option:`payload-length` |  | Length of payload (has to be same as on the server)</p>
    <p>:option:`buffer-length` |  | Same as payload-length (used for legacy reasons)</p>
    <p>:option:`ssl` | N/A | Initial an ssl handshake with the server.</p>
  </dd>
</dl>
#### Arguments ####

<dl>
  <dt> : synopsis : Show help screen (this screen)</dt>
  <dd>
    <p>:synopsis: Show help screen (this screen)</p>
    <p>| Show help screen (this screen)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show help screen as a protocol buffer payload</dt>
  <dd>
    <p>:synopsis: Show help screen as a protocol buffer payload</p>
    <p>| Show help screen as a protocol buffer payload</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show default values for a given command</dt>
  <dd>
    <p>:synopsis: Show default values for a given command</p>
    <p>| Show default values for a given command</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show help screen (short format).</dt>
  <dd>
    <p>:synopsis: Show help screen (short format).</p>
    <p>| Show help screen (short format).</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The host of the host running the server</dt>
  <dd>
    <p>:synopsis: The host of the host running the server</p>
    <p>| The host of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The port of the host running the server</dt>
  <dd>
    <p>:synopsis: The port of the host running the server</p>
    <p>| The port of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The address (host : port) of the host running the server</dt>
  <dd>
    <p>:synopsis: The address (host:port) of the host running the server</p>
    <p>| The address (host:port) of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Number of seconds before connection times out (default=10)</dt>
  <dd>
    <p>:synopsis: Number of seconds before connection times out (default=10)</p>
    <p>| Number of seconds before connection times out (default=10)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Target to use (lookup connection info from config)</dt>
  <dd>
    <p>:synopsis: Target to use (lookup connection info from config)</p>
    <p>| Target to use (lookup connection info from config)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Number of times ti retry a failed connection attempt (default=2)</dt>
  <dd>
    <p>:synopsis: Number of times ti retry a failed connection attempt (default=2)</p>
    <p>| Number of times ti retry a failed connection attempt (default=2)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The name of the command that the remote daemon should run</dt>
  <dd>
    <p>:synopsis: The name of the command that the remote daemon should run</p>
    <p>| The name of the command that the remote daemon should run</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : list of arguments</dt>
  <dd>
    <p>:synopsis: list of arguments</p>
    <p>| list of arguments</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Do not initial an ssl handshake with the server, talk in plain-text.</dt>
  <dd>
    <p>:synopsis: Do not initial an ssl handshake with the server, talk in plain-text.</p>
    <p>| Do not initial an ssl handshake with the server, talk in plain-text.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Length of payload (has to be same as on the server)</dt>
  <dd>
    <p>:synopsis: Length of payload (has to be same as on the server)</p>
    <p>| Length of payload (has to be same as on the server)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</dt>
  <dd>
    <p>:synopsis: The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
    <p>| The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Client certificate to use</dt>
  <dd>
    <p>:synopsis: Client certificate to use</p>
    <p>| Client certificate to use</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Client certificate format (default is PEM)</dt>
  <dd>
    <p>:synopsis: Client certificate format (default is PEM)</p>
    <p>| Client certificate format (default is PEM)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Use insecure legacy mode</dt>
  <dd>
    <p>:synopsis: Use insecure legacy mode</p>
    <p>| Use insecure legacy mode</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : A file representing the Certificate authority used to validate peer certificates</dt>
  <dd>
    <p>:synopsis: A file representing the Certificate authority used to validate peer certificates</p>
    <p>| A file representing the Certificate authority used to validate peer certificates</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Which verification mode to use : none : no verification, peer : that peer has a certificate, peer-cert : that peer has a valid certificate, ...</dt>
  <dd>
    <p>:synopsis: Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
    <p>| Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</dt>
  <dd>
    <p>:synopsis: Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
    <p>| Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Length of payload (has to be same as on the server)</dt>
  <dd>
    <p>:synopsis: Length of payload (has to be same as on the server)</p>
    <p>| Length of payload (has to be same as on the server)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Same as payload-length (used for legacy reasons)</dt>
  <dd>
    <p>:synopsis: Same as payload-length (used for legacy reasons)</p>
    <p>| Same as payload-length (used for legacy reasons)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Initial an ssl handshake with the server.</dt>
  <dd>
    <p>:synopsis: Initial an ssl handshake with the server.</p>
    <p>| Initial an ssl handshake with the server.</p>
  </dd>
</dl>
### :query:`web_forward` ###

    :synopsis: Forward the request as-is to remote host via WEB.

**Usage:**

#### Arguments ####

### :query:`web_query` ###

    :synopsis: Request remote information via WEB.

**Usage:**

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Option", "Default Value", "Description"</p>
    <p>:option:`help` | N/A | Show help screen (this screen)</p>
    <p>:option:`help-pb` | N/A | Show help screen as a protocol buffer payload</p>
    <p>:option:`show-default` | N/A | Show default values for a given command</p>
    <p>:option:`help-short` | N/A | Show help screen (short format).</p>
    <p>:option:`host` |  | The host of the host running the server</p>
    <p>:option:`port` |  | The port of the host running the server</p>
    <p>:option:`address` |  | The address (host:port) of the host running the server</p>
    <p>:option:`timeout` |  | Number of seconds before connection times out (default=10)</p>
    <p>:option:`target` |  | Target to use (lookup connection info from config)</p>
    <p>:option:`retry` |  | Number of times ti retry a failed connection attempt (default=2)</p>
    <p>:option:`command` |  | The name of the query that the remote daemon should run</p>
    <p>:option:`arguments` |  | list of arguments</p>
    <p>:option:`query-command` |  | The name of the query that the remote daemon should run</p>
    <p>:option:`query-arguments` |  | list of arguments</p>
    <p>:option:`no-ssl` | N/A | Do not initial an ssl handshake with the server, talk in plain-text.</p>
    <p>:option:`certificate` |  | Length of payload (has to be same as on the server)</p>
    <p>:option:`dh` |  | The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
    <p>:option:`certificate-key` |  | Client certificate to use</p>
    <p>:option:`certificate-format` |  | Client certificate format (default is PEM)</p>
    <p>:option:`insecure` | N/A | Use insecure legacy mode</p>
    <p>:option:`ca` |  | A file representing the Certificate authority used to validate peer certificates</p>
    <p>:option:`verify` |  | Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
    <p>:option:`allowed-ciphers` |  | Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
    <p>:option:`payload-length` |  | Length of payload (has to be same as on the server)</p>
    <p>:option:`buffer-length` |  | Same as payload-length (used for legacy reasons)</p>
    <p>:option:`ssl` | N/A | Initial an ssl handshake with the server.</p>
  </dd>
</dl>
#### Arguments ####

<dl>
  <dt> : synopsis : Show help screen (this screen)</dt>
  <dd>
    <p>:synopsis: Show help screen (this screen)</p>
    <p>| Show help screen (this screen)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show help screen as a protocol buffer payload</dt>
  <dd>
    <p>:synopsis: Show help screen as a protocol buffer payload</p>
    <p>| Show help screen as a protocol buffer payload</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show default values for a given command</dt>
  <dd>
    <p>:synopsis: Show default values for a given command</p>
    <p>| Show default values for a given command</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show help screen (short format).</dt>
  <dd>
    <p>:synopsis: Show help screen (short format).</p>
    <p>| Show help screen (short format).</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The host of the host running the server</dt>
  <dd>
    <p>:synopsis: The host of the host running the server</p>
    <p>| The host of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The port of the host running the server</dt>
  <dd>
    <p>:synopsis: The port of the host running the server</p>
    <p>| The port of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The address (host : port) of the host running the server</dt>
  <dd>
    <p>:synopsis: The address (host:port) of the host running the server</p>
    <p>| The address (host:port) of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Number of seconds before connection times out (default=10)</dt>
  <dd>
    <p>:synopsis: Number of seconds before connection times out (default=10)</p>
    <p>| Number of seconds before connection times out (default=10)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Target to use (lookup connection info from config)</dt>
  <dd>
    <p>:synopsis: Target to use (lookup connection info from config)</p>
    <p>| Target to use (lookup connection info from config)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Number of times ti retry a failed connection attempt (default=2)</dt>
  <dd>
    <p>:synopsis: Number of times ti retry a failed connection attempt (default=2)</p>
    <p>| Number of times ti retry a failed connection attempt (default=2)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The name of the query that the remote daemon should run</dt>
  <dd>
    <p>:synopsis: The name of the query that the remote daemon should run</p>
    <p>| The name of the query that the remote daemon should run</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : list of arguments</dt>
  <dd>
    <p>:synopsis: list of arguments</p>
    <p>| list of arguments</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The name of the query that the remote daemon should run</dt>
  <dd>
    <p>:synopsis: The name of the query that the remote daemon should run</p>
    <p>| The name of the query that the remote daemon should run</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : list of arguments</dt>
  <dd>
    <p>:synopsis: list of arguments</p>
    <p>| list of arguments</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Do not initial an ssl handshake with the server, talk in plain-text.</dt>
  <dd>
    <p>:synopsis: Do not initial an ssl handshake with the server, talk in plain-text.</p>
    <p>| Do not initial an ssl handshake with the server, talk in plain-text.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Length of payload (has to be same as on the server)</dt>
  <dd>
    <p>:synopsis: Length of payload (has to be same as on the server)</p>
    <p>| Length of payload (has to be same as on the server)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</dt>
  <dd>
    <p>:synopsis: The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
    <p>| The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Client certificate to use</dt>
  <dd>
    <p>:synopsis: Client certificate to use</p>
    <p>| Client certificate to use</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Client certificate format (default is PEM)</dt>
  <dd>
    <p>:synopsis: Client certificate format (default is PEM)</p>
    <p>| Client certificate format (default is PEM)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Use insecure legacy mode</dt>
  <dd>
    <p>:synopsis: Use insecure legacy mode</p>
    <p>| Use insecure legacy mode</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : A file representing the Certificate authority used to validate peer certificates</dt>
  <dd>
    <p>:synopsis: A file representing the Certificate authority used to validate peer certificates</p>
    <p>| A file representing the Certificate authority used to validate peer certificates</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Which verification mode to use : none : no verification, peer : that peer has a certificate, peer-cert : that peer has a valid certificate, ...</dt>
  <dd>
    <p>:synopsis: Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
    <p>| Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</dt>
  <dd>
    <p>:synopsis: Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
    <p>| Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Length of payload (has to be same as on the server)</dt>
  <dd>
    <p>:synopsis: Length of payload (has to be same as on the server)</p>
    <p>| Length of payload (has to be same as on the server)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Same as payload-length (used for legacy reasons)</dt>
  <dd>
    <p>:synopsis: Same as payload-length (used for legacy reasons)</p>
    <p>| Same as payload-length (used for legacy reasons)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Initial an ssl handshake with the server.</dt>
  <dd>
    <p>:synopsis: Initial an ssl handshake with the server.</p>
    <p>| Initial an ssl handshake with the server.</p>
  </dd>
</dl>
### :query:`web_submit` ###

    :synopsis: Submit information to remote host via WEB.

**Usage:**

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Option", "Default Value", "Description"</p>
    <p>:option:`help` | N/A | Show help screen (this screen)</p>
    <p>:option:`help-pb` | N/A | Show help screen as a protocol buffer payload</p>
    <p>:option:`show-default` | N/A | Show default values for a given command</p>
    <p>:option:`help-short` | N/A | Show help screen (short format).</p>
    <p>:option:`host` |  | The host of the host running the server</p>
    <p>:option:`port` |  | The port of the host running the server</p>
    <p>:option:`address` |  | The address (host:port) of the host running the server</p>
    <p>:option:`timeout` |  | Number of seconds before connection times out (default=10)</p>
    <p>:option:`target` |  | Target to use (lookup connection info from config)</p>
    <p>:option:`retry` |  | Number of times ti retry a failed connection attempt (default=2)</p>
    <p>:option:`command` |  | The name of the command that the remote daemon should run</p>
    <p>:option:`alias` |  | Same as command</p>
    <p>:option:`message` |  | Message</p>
    <p>:option:`result` |  | Result code either a number or OK, WARN, CRIT, UNKNOWN</p>
    <p>:option:`no-ssl` | N/A | Do not initial an ssl handshake with the server, talk in plain-text.</p>
    <p>:option:`certificate` |  | Length of payload (has to be same as on the server)</p>
    <p>:option:`dh` |  | The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
    <p>:option:`certificate-key` |  | Client certificate to use</p>
    <p>:option:`certificate-format` |  | Client certificate format (default is PEM)</p>
    <p>:option:`insecure` | N/A | Use insecure legacy mode</p>
    <p>:option:`ca` |  | A file representing the Certificate authority used to validate peer certificates</p>
    <p>:option:`verify` |  | Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
    <p>:option:`allowed-ciphers` |  | Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
    <p>:option:`payload-length` |  | Length of payload (has to be same as on the server)</p>
    <p>:option:`buffer-length` |  | Same as payload-length (used for legacy reasons)</p>
    <p>:option:`ssl` | N/A | Initial an ssl handshake with the server.</p>
  </dd>
</dl>
#### Arguments ####

<dl>
  <dt> : synopsis : Show help screen (this screen)</dt>
  <dd>
    <p>:synopsis: Show help screen (this screen)</p>
    <p>| Show help screen (this screen)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show help screen as a protocol buffer payload</dt>
  <dd>
    <p>:synopsis: Show help screen as a protocol buffer payload</p>
    <p>| Show help screen as a protocol buffer payload</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show default values for a given command</dt>
  <dd>
    <p>:synopsis: Show default values for a given command</p>
    <p>| Show default values for a given command</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show help screen (short format).</dt>
  <dd>
    <p>:synopsis: Show help screen (short format).</p>
    <p>| Show help screen (short format).</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The host of the host running the server</dt>
  <dd>
    <p>:synopsis: The host of the host running the server</p>
    <p>| The host of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The port of the host running the server</dt>
  <dd>
    <p>:synopsis: The port of the host running the server</p>
    <p>| The port of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The address (host : port) of the host running the server</dt>
  <dd>
    <p>:synopsis: The address (host:port) of the host running the server</p>
    <p>| The address (host:port) of the host running the server</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Number of seconds before connection times out (default=10)</dt>
  <dd>
    <p>:synopsis: Number of seconds before connection times out (default=10)</p>
    <p>| Number of seconds before connection times out (default=10)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Target to use (lookup connection info from config)</dt>
  <dd>
    <p>:synopsis: Target to use (lookup connection info from config)</p>
    <p>| Target to use (lookup connection info from config)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Number of times ti retry a failed connection attempt (default=2)</dt>
  <dd>
    <p>:synopsis: Number of times ti retry a failed connection attempt (default=2)</p>
    <p>| Number of times ti retry a failed connection attempt (default=2)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The name of the command that the remote daemon should run</dt>
  <dd>
    <p>:synopsis: The name of the command that the remote daemon should run</p>
    <p>| The name of the command that the remote daemon should run</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Same as command</dt>
  <dd>
    <p>:synopsis: Same as command</p>
    <p>| Same as command</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Message</dt>
  <dd>
    <p>:synopsis: Message</p>
    <p>| Message</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Result code either a number or OK, WARN, CRIT, UNKNOWN</dt>
  <dd>
    <p>:synopsis: Result code either a number or OK, WARN, CRIT, UNKNOWN</p>
    <p>| Result code either a number or OK, WARN, CRIT, UNKNOWN</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Do not initial an ssl handshake with the server, talk in plain-text.</dt>
  <dd>
    <p>:synopsis: Do not initial an ssl handshake with the server, talk in plain-text.</p>
    <p>| Do not initial an ssl handshake with the server, talk in plain-text.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Length of payload (has to be same as on the server)</dt>
  <dd>
    <p>:synopsis: Length of payload (has to be same as on the server)</p>
    <p>| Length of payload (has to be same as on the server)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</dt>
  <dd>
    <p>:synopsis: The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
    <p>| The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Client certificate to use</dt>
  <dd>
    <p>:synopsis: Client certificate to use</p>
    <p>| Client certificate to use</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Client certificate format (default is PEM)</dt>
  <dd>
    <p>:synopsis: Client certificate format (default is PEM)</p>
    <p>| Client certificate format (default is PEM)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Use insecure legacy mode</dt>
  <dd>
    <p>:synopsis: Use insecure legacy mode</p>
    <p>| Use insecure legacy mode</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : A file representing the Certificate authority used to validate peer certificates</dt>
  <dd>
    <p>:synopsis: A file representing the Certificate authority used to validate peer certificates</p>
    <p>| A file representing the Certificate authority used to validate peer certificates</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Which verification mode to use : none : no verification, peer : that peer has a certificate, peer-cert : that peer has a valid certificate, ...</dt>
  <dd>
    <p>:synopsis: Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
    <p>| Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</dt>
  <dd>
    <p>:synopsis: Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
    <p>| Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Length of payload (has to be same as on the server)</dt>
  <dd>
    <p>:synopsis: Length of payload (has to be same as on the server)</p>
    <p>| Length of payload (has to be same as on the server)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Same as payload-length (used for legacy reasons)</dt>
  <dd>
    <p>:synopsis: Same as payload-length (used for legacy reasons)</p>
    <p>| Same as payload-length (used for legacy reasons)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Initial an ssl handshake with the server.</dt>
  <dd>
    <p>:synopsis: Initial an ssl handshake with the server.</p>
    <p>| Initial an ssl handshake with the server.</p>
  </dd>
</dl>
### / settings/ NRPE/ client ###

    :synopsis: WEB CLIENT SECTION

<dl>
  <dt>**WEB CLIENT SECTION**</dt>
  <dd>
    <p>| Section for WEB active/passive check module.</p>
    <p>.. csv-table::</p>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Key", "Default Value", "Description"</p>
    <p></p>
    <p>:confkey:`channel` | NRPE | CHANNEL</p>
    <p>**Sample**::</p>
    <p># WEB CLIENT SECTION</p>
    <p># Section for WEB active/passive check module.</p>
    <p>[/settings/NRPE/client]</p>
    <p>channel=NRPE</p>
    <p>.. confkey:: channel</p>
    <p>:synopsis: CHANNEL</p>
    <p>**CHANNEL**</p>
    <p>| The channel to listen to.</p>
    <p>**Path**: /settings/NRPE/client</p>
    <p>**Key**: channel</p>
    <p>**Default value**: NRPE</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client]</p>
    <p># CHANNEL</p>
    <p>channel=NRPE</p>
  </dd>
</dl>
### …  / handlers ###

    :synopsis: CLIENT HANDLER SECTION

<dl>
  <dt>**CLIENT HANDLER SECTION**</dt>
  <dd>
    <p>**Sample**::</p>
    <p># CLIENT HANDLER SECTION</p>
    <p>#</p>
    <p>[/settings/NRPE/client/handlers]</p>
  </dd>
</dl>
### …  / targets ###

    :synopsis: REMOTE TARGET DEFINITIONS

<dl>
  <dt>**REMOTE TARGET DEFINITIONS**</dt>
  <dd>
    <p>**Sample**::</p>
    <p># REMOTE TARGET DEFINITIONS</p>
    <p>#</p>
    <p>[/settings/NRPE/client/targets]</p>
  </dd>
</dl>
### …  / targets / default ###

    :synopsis: TARGET DEFENITION

<dl>
  <dt>**TARGET DEFENITION**</dt>
  <dd>
    <p>| Target definition for: default</p>
    <p>.. csv-table::</p>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Key", "Default Value", "Description"</p>
    <p></p>
    <p>:confkey:`address` |  | TARGET ADDRESS</p>
    <p>:confkey:`alias` |  | ALIAS</p>
    <p>:confkey:`allowed ciphers` | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH | ALLOWED CIPHERS</p>
    <p>:confkey:`ca` |  | CA</p>
    <p>:confkey:`certificate` |  | SSL CERTIFICATE</p>
    <p>:confkey:`certificate format` | PEM | CERTIFICATE FORMAT</p>
    <p>:confkey:`certificate key` |  | SSL CERTIFICATE KEY</p>
    <p>:confkey:`dh` |  | DH KEY</p>
    <p>:confkey:`host` |  | TARGET HOST</p>
    <p>:confkey:`insecure` |  | Insecure legacy mode</p>
    <p>:confkey:`is template` | 0 | IS TEMPLATE</p>
    <p>:confkey:`parent` | default | PARENT</p>
    <p>:confkey:`payload length` | 1024 | PAYLOAD LENGTH</p>
    <p>:confkey:`port` | 0 | TARGET PORT</p>
    <p>:confkey:`timeout` | 30 | TIMEOUT</p>
    <p>:confkey:`use ssl` | 1 | ENABLE SSL ENCRYPTION</p>
    <p>:confkey:`verify mode` | none | VERIFY MODE</p>
    <p>**Sample**::</p>
    <p># TARGET DEFENITION</p>
    <p># Target definition for: default</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p>address=</p>
    <p>alias=</p>
    <p>allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH</p>
    <p>ca=</p>
    <p>certificate=</p>
    <p>certificate format=PEM</p>
    <p>certificate key=</p>
    <p>dh=</p>
    <p>host=</p>
    <p>insecure=</p>
    <p>is template=0</p>
    <p>parent=default</p>
    <p>payload length=1024</p>
    <p>port=0</p>
    <p>timeout=30</p>
    <p>use ssl=1</p>
    <p>verify mode=none</p>
    <p>.. confkey:: address</p>
    <p>:synopsis: TARGET ADDRESS</p>
    <p>**TARGET ADDRESS**</p>
    <p>| Target host address</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: address</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># TARGET ADDRESS</p>
    <p>address=</p>
    <p>.. confkey:: alias</p>
    <p>:synopsis: ALIAS</p>
    <p>**ALIAS**</p>
    <p>| The alias (service name) to report to server</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: alias</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># ALIAS</p>
    <p>alias=</p>
    <p>.. confkey:: allowed ciphers</p>
    <p>:synopsis: ALLOWED CIPHERS</p>
    <p>**ALLOWED CIPHERS**</p>
    <p>| The allowed list of ciphers (setting insecure wil override this to only support ADH</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: allowed ciphers</p>
    <p>**Default value**: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># ALLOWED CIPHERS</p>
    <p>allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH</p>
    <p>.. confkey:: ca</p>
    <p>:synopsis: CA</p>
    <p>**CA**</p>
    <p>| The certificate authority to use to authenticate remote certificate</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: ca</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># CA</p>
    <p>ca=</p>
    <p>.. confkey:: certificate</p>
    <p>:synopsis: SSL CERTIFICATE</p>
    <p>**SSL CERTIFICATE**</p>
    <p>| The ssl certificate to use to encrypt the communication</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: certificate</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># SSL CERTIFICATE</p>
    <p>certificate=</p>
    <p>.. confkey:: certificate format</p>
    <p>:synopsis: CERTIFICATE FORMAT</p>
    <p>**CERTIFICATE FORMAT**</p>
    <p>| Format of SSL certificate</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: certificate format</p>
    <p>**Default value**: PEM</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># CERTIFICATE FORMAT</p>
    <p>certificate format=PEM</p>
    <p>.. confkey:: certificate key</p>
    <p>:synopsis: SSL CERTIFICATE KEY</p>
    <p>**SSL CERTIFICATE KEY**</p>
    <p>| Key for the SSL certificate</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: certificate key</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># SSL CERTIFICATE KEY</p>
    <p>certificate key=</p>
    <p>.. confkey:: dh</p>
    <p>:synopsis: DH KEY</p>
    <p>**DH KEY**</p>
    <p>| The diffi-hellman perfect forwarded secret to use setting --insecure will override this</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: dh</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># DH KEY</p>
    <p>dh=</p>
    <p>.. confkey:: host</p>
    <p>:synopsis: TARGET HOST</p>
    <p>**TARGET HOST**</p>
    <p>| The target server to report results to.</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: host</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># TARGET HOST</p>
    <p>host=</p>
    <p>.. confkey:: insecure</p>
    <p>:synopsis: Insecure legacy mode</p>
    <p>**Insecure legacy mode**</p>
    <p>| Use insecure legacy mode to connect to old NRPE server</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: insecure</p>
    <p>**Default value**:</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># Insecure legacy mode</p>
    <p>insecure=</p>
    <p>.. confkey:: is template</p>
    <p>:synopsis: IS TEMPLATE</p>
    <p>**IS TEMPLATE**</p>
    <p>| Declare this object as a template (this means it will not be available as a separate object)</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: is template</p>
    <p>**Default value**: 0</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># IS TEMPLATE</p>
    <p>is template=0</p>
    <p>.. confkey:: parent</p>
    <p>:synopsis: PARENT</p>
    <p>**PARENT**</p>
    <p>| The parent the target inherits from</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: parent</p>
    <p>**Default value**: default</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># PARENT</p>
    <p>parent=default</p>
    <p>.. confkey:: payload length</p>
    <p>:synopsis: PAYLOAD LENGTH</p>
    <p>**PAYLOAD LENGTH**</p>
    <p>| Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: payload length</p>
    <p>**Default value**: 1024</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># PAYLOAD LENGTH</p>
    <p>payload length=1024</p>
    <p>.. confkey:: port</p>
    <p>:synopsis: TARGET PORT</p>
    <p>**TARGET PORT**</p>
    <p>| The target server port</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: port</p>
    <p>**Default value**: 0</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># TARGET PORT</p>
    <p>port=0</p>
    <p>.. confkey:: timeout</p>
    <p>:synopsis: TIMEOUT</p>
    <p>**TIMEOUT**</p>
    <p>| Timeout when reading/writing packets to/from sockets.</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: timeout</p>
    <p>**Default value**: 30</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># TIMEOUT</p>
    <p>timeout=30</p>
    <p>.. confkey:: use ssl</p>
    <p>:synopsis: ENABLE SSL ENCRYPTION</p>
    <p>**ENABLE SSL ENCRYPTION**</p>
    <p>| This option controls if SSL should be enabled.</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: use ssl</p>
    <p>**Default value**: 1</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># ENABLE SSL ENCRYPTION</p>
    <p>use ssl=1</p>
    <p>.. confkey:: verify mode</p>
    <p>:synopsis: VERIFY MODE</p>
    <p>**VERIFY MODE**</p>
    <p>| What to verify default is non, to validate remote certificate use remote-peer</p>
    <p>**Path**: /settings/NRPE/client/targets/default</p>
    <p>**Key**: verify mode</p>
    <p>**Default value**: none</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/default]</p>
    <p># VERIFY MODE</p>
    <p>verify mode=none</p>
  </dd>
</dl>
### …  / targets / sample ###

    :synopsis: TARGET DEFENITION

<dl>
  <dt>**TARGET DEFENITION**</dt>
  <dd>
    <p>| Target definition for: sample</p>
    <p>.. csv-table::</p>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Key", "Default Value", "Description"</p>
    <p></p>
    <p>:confkey:`address` |  | TARGET ADDRESS</p>
    <p>:confkey:`alias` |  | ALIAS</p>
    <p>:confkey:`allowed ciphers` | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH | ALLOWED CIPHERS</p>
    <p>:confkey:`ca` |  | CA</p>
    <p>:confkey:`certificate` |  | SSL CERTIFICATE</p>
    <p>:confkey:`certificate format` | PEM | CERTIFICATE FORMAT</p>
    <p>:confkey:`certificate key` |  | SSL CERTIFICATE KEY</p>
    <p>:confkey:`dh` |  | DH KEY</p>
    <p>:confkey:`host` |  | TARGET HOST</p>
    <p>:confkey:`insecure` |  | Insecure legacy mode</p>
    <p>:confkey:`is template` | 0 | IS TEMPLATE</p>
    <p>:confkey:`parent` | default | PARENT</p>
    <p>:confkey:`payload length` | 1024 | PAYLOAD LENGTH</p>
    <p>:confkey:`port` | 0 | TARGET PORT</p>
    <p>:confkey:`timeout` | 30 | TIMEOUT</p>
    <p>:confkey:`use ssl` | 1 | ENABLE SSL ENCRYPTION</p>
    <p>:confkey:`verify mode` | none | VERIFY MODE</p>
    <p>**Sample**::</p>
    <p># TARGET DEFENITION</p>
    <p># Target definition for: sample</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p>address=</p>
    <p>alias=</p>
    <p>allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH</p>
    <p>ca=</p>
    <p>certificate=</p>
    <p>certificate format=PEM</p>
    <p>certificate key=</p>
    <p>dh=</p>
    <p>host=</p>
    <p>insecure=</p>
    <p>is template=0</p>
    <p>parent=default</p>
    <p>payload length=1024</p>
    <p>port=0</p>
    <p>timeout=30</p>
    <p>use ssl=1</p>
    <p>verify mode=none</p>
    <p>.. confkey:: address</p>
    <p>:synopsis: TARGET ADDRESS</p>
    <p>**TARGET ADDRESS**</p>
    <p>| Target host address</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: address</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># TARGET ADDRESS</p>
    <p>address=</p>
    <p>.. confkey:: alias</p>
    <p>:synopsis: ALIAS</p>
    <p>**ALIAS**</p>
    <p>| The alias (service name) to report to server</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: alias</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># ALIAS</p>
    <p>alias=</p>
    <p>.. confkey:: allowed ciphers</p>
    <p>:synopsis: ALLOWED CIPHERS</p>
    <p>**ALLOWED CIPHERS**</p>
    <p>| The allowed list of ciphers (setting insecure wil override this to only support ADH</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: allowed ciphers</p>
    <p>**Default value**: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># ALLOWED CIPHERS</p>
    <p>allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH</p>
    <p>.. confkey:: ca</p>
    <p>:synopsis: CA</p>
    <p>**CA**</p>
    <p>| The certificate authority to use to authenticate remote certificate</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: ca</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># CA</p>
    <p>ca=</p>
    <p>.. confkey:: certificate</p>
    <p>:synopsis: SSL CERTIFICATE</p>
    <p>**SSL CERTIFICATE**</p>
    <p>| The ssl certificate to use to encrypt the communication</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: certificate</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># SSL CERTIFICATE</p>
    <p>certificate=</p>
    <p>.. confkey:: certificate format</p>
    <p>:synopsis: CERTIFICATE FORMAT</p>
    <p>**CERTIFICATE FORMAT**</p>
    <p>| Format of SSL certificate</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: certificate format</p>
    <p>**Default value**: PEM</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># CERTIFICATE FORMAT</p>
    <p>certificate format=PEM</p>
    <p>.. confkey:: certificate key</p>
    <p>:synopsis: SSL CERTIFICATE KEY</p>
    <p>**SSL CERTIFICATE KEY**</p>
    <p>| Key for the SSL certificate</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: certificate key</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># SSL CERTIFICATE KEY</p>
    <p>certificate key=</p>
    <p>.. confkey:: dh</p>
    <p>:synopsis: DH KEY</p>
    <p>**DH KEY**</p>
    <p>| The diffi-hellman perfect forwarded secret to use setting --insecure will override this</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: dh</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># DH KEY</p>
    <p>dh=</p>
    <p>.. confkey:: host</p>
    <p>:synopsis: TARGET HOST</p>
    <p>**TARGET HOST**</p>
    <p>| The target server to report results to.</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: host</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># TARGET HOST</p>
    <p>host=</p>
    <p>.. confkey:: insecure</p>
    <p>:synopsis: Insecure legacy mode</p>
    <p>**Insecure legacy mode**</p>
    <p>| Use insecure legacy mode to connect to old NRPE server</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: insecure</p>
    <p>**Default value**:</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># Insecure legacy mode</p>
    <p>insecure=</p>
    <p>.. confkey:: is template</p>
    <p>:synopsis: IS TEMPLATE</p>
    <p>**IS TEMPLATE**</p>
    <p>| Declare this object as a template (this means it will not be available as a separate object)</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: is template</p>
    <p>**Default value**: 0</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># IS TEMPLATE</p>
    <p>is template=0</p>
    <p>.. confkey:: parent</p>
    <p>:synopsis: PARENT</p>
    <p>**PARENT**</p>
    <p>| The parent the target inherits from</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: parent</p>
    <p>**Default value**: default</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># PARENT</p>
    <p>parent=default</p>
    <p>.. confkey:: payload length</p>
    <p>:synopsis: PAYLOAD LENGTH</p>
    <p>**PAYLOAD LENGTH**</p>
    <p>| Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: payload length</p>
    <p>**Default value**: 1024</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># PAYLOAD LENGTH</p>
    <p>payload length=1024</p>
    <p>.. confkey:: port</p>
    <p>:synopsis: TARGET PORT</p>
    <p>**TARGET PORT**</p>
    <p>| The target server port</p>
    <p>**Advanced** (means it is not commonly used)</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: port</p>
    <p>**Default value**: 0</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># TARGET PORT</p>
    <p>port=0</p>
    <p>.. confkey:: timeout</p>
    <p>:synopsis: TIMEOUT</p>
    <p>**TIMEOUT**</p>
    <p>| Timeout when reading/writing packets to/from sockets.</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: timeout</p>
    <p>**Default value**: 30</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># TIMEOUT</p>
    <p>timeout=30</p>
    <p>.. confkey:: use ssl</p>
    <p>:synopsis: ENABLE SSL ENCRYPTION</p>
    <p>**ENABLE SSL ENCRYPTION**</p>
    <p>| This option controls if SSL should be enabled.</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: use ssl</p>
    <p>**Default value**: 1</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># ENABLE SSL ENCRYPTION</p>
    <p>use ssl=1</p>
    <p>.. confkey:: verify mode</p>
    <p>:synopsis: VERIFY MODE</p>
    <p>**VERIFY MODE**</p>
    <p>| What to verify default is non, to validate remote certificate use remote-peer</p>
    <p>**Path**: /settings/NRPE/client/targets/sample</p>
    <p>**Key**: verify mode</p>
    <p>**Default value**: none</p>
    <p>**Sample key**: This key is provided as a sample to show how to configure objects</p>
    <p>**Used by**: :module:`NRPEClient`,  :module:`WEBClient`</p>
    <p>**Sample**::</p>
    <p>[/settings/NRPE/client/targets/sample]</p>
    <p># VERIFY MODE</p>
    <p>verify mode=none</p>
  </dd>
</dl>
