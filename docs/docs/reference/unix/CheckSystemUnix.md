<dl>
  <dt>.. module :  : CheckSystemUnix</dt>
  <dd>:synopsis: Various system related checks, such as CPU load, process state and memory.</dd>
</dl>
# :module:`CheckSystemUnix` --- CheckSystem #

Various system related checks, such as CPU load, process state and memory.

**Queries (Overview)**:

1. list of all available queries (check commands)
   

<dl>
  <dt> : class : contentstable </dt>
  <dd>
    <p>:class: contentstable</p>
    <p>:delim: |</p>
    <p>:header: "Command", "Description"</p>
    <p>:query:`check_memory` | Check free/used memory on the system.</p>
    <p>:query:`check_os_version` | Check the version of the underlaying OS.</p>
    <p>:query:`check_uptime` | Check time since last server re-boot.</p>
  </dd>
</dl>
**Commands (Overview)**: 

**TODO:** Add a list of all external commands (this is not check commands)

## Queries ##

1. quick reference for all available queries (check commands) in the CheckSystemUnix module.
   

### :query:`check_memory` ###

    :synopsis: Check free/used memory on the system.

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
    <p>:option:`debug` | N/A | Show debugging information in the log</p>
    <p>:option:`show-all` | N/A | Show debugging information in the log</p>
    <p>:option:`filter` |  | Filter which marks interesting items.</p>
    <p>:option:`warning` | used > 80% | Filter which marks items which generates a warning state.</p>
    <p>:option:`warn` |  | Short alias for warning</p>
    <p>:option:`critical` | used > 90% | Filter which marks items which generates a critical state.</p>
    <p>:option:`crit` |  | Short alias for critical.</p>
    <p>:option:`ok` |  | Filter which marks items which generates an ok state.</p>
    <p>:option:`empty-state` | ignored | Return status to use when nothing matched filter.</p>
    <p>:option:`perf-config` |  | Performance data generation configuration</p>
    <p>:option:`top-syntax` | ${status}: ${list} | Top level syntax.</p>
    <p>:option:`ok-syntax` |  | ok syntax.</p>
    <p>:option:`empty-syntax` |  | Empty syntax.</p>
    <p>:option:`detail-syntax` | ${type} = ${used} | Detail level syntax.</p>
    <p>:option:`perf-syntax` | ${type} | Performance alias syntax.</p>
    <p>:option:`type` |  | The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)</p>
  </dd>
</dl>
#### Samples ####

Arguments
*********
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
  <dt> : synopsis : Show debugging information in the log</dt>
  <dd>
    <p>:synopsis: Show debugging information in the log</p>
    <p>| Show debugging information in the log</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show debugging information in the log</dt>
  <dd>
    <p>:synopsis: Show debugging information in the log</p>
    <p>| Show debugging information in the log</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks interesting items.</dt>
  <dd>
    <p>:synopsis: Filter which marks interesting items.</p>
    <p>| Filter which marks interesting items.</p>
    <p>| Interesting items are items which will be included in the check.</p>
    <p>| They do not denote warning or critical state but they are checked use this to filter out unwanted items.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>free           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>size           Total size of memory</p>
    <p>type           The type of memory to check</p>
    <p>used           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates a warning state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates a warning state.</p>
    <p>| Filter which marks items which generates a warning state.</p>
    <p>| If anything matches this filter the return status will be escalated to warning.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>free           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>size           Total size of memory</p>
    <p>type           The type of memory to check</p>
    <p>used           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Short alias for warning</dt>
  <dd>
    <p>:synopsis: Short alias for warning</p>
    <p>| Short alias for warning</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates a critical state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates a critical state.</p>
    <p>| Filter which marks items which generates a critical state.</p>
    <p>| If anything matches this filter the return status will be escalated to critical.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>free           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>size           Total size of memory</p>
    <p>type           The type of memory to check</p>
    <p>used           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Short alias for critical.</dt>
  <dd>
    <p>:synopsis: Short alias for critical.</p>
    <p>| Short alias for critical.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates an ok state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates an ok state.</p>
    <p>| Filter which marks items which generates an ok state.</p>
    <p>| If anything matches this any previous state for this item will be reset to ok.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>free           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>size           Total size of memory</p>
    <p>type           The type of memory to check</p>
    <p>used           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Return status to use when nothing matched filter.</dt>
  <dd>
    <p>:synopsis: Return status to use when nothing matched filter.</p>
    <p>| Return status to use when nothing matched filter.</p>
    <p>| If no filter is specified this will never happen unless the file is empty.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Performance data generation configuration</dt>
  <dd>
    <p>:synopsis: Performance data generation configuration</p>
    <p>| Performance data generation configuration</p>
    <p>| TODO: obj ( key: value; key: value) obj (key:valuer;key:value)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Top level syntax.</dt>
  <dd>
    <p>:synopsis: Top level syntax.</p>
    <p>| Top level syntax.</p>
    <p>| Used to format the message to return can include strings as well as special keywords such as:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(free)           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>%(size)           Total size of memory</p>
    <p>%(type)           The type of memory to check</p>
    <p>%(used)           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : ok syntax.</dt>
  <dd>
    <p>:synopsis: ok syntax.</p>
    <p>| ok syntax.</p>
    <p>| DEPRECATED! This is the syntax for when an ok result is returned.</p>
    <p>| This value will not be used if your syntax contains %(list) or %(count).</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Empty syntax.</dt>
  <dd>
    <p>:synopsis: Empty syntax.</p>
    <p>| Empty syntax.</p>
    <p>| DEPRECATED! This is the syntax for when nothing matches the filter.</p>
    <p>| Possible values are:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(free)           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>%(size)           Total size of memory</p>
    <p>%(type)           The type of memory to check</p>
    <p>%(used)           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Detail level syntax.</dt>
  <dd>
    <p>:synopsis: Detail level syntax.</p>
    <p>| Detail level syntax.</p>
    <p>| This is the syntax of each item in the list of top-syntax (see above).</p>
    <p>| Possible values are:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(free)           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>%(size)           Total size of memory</p>
    <p>%(type)           The type of memory to check</p>
    <p>%(used)           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Performance alias syntax.</dt>
  <dd>
    <p>:synopsis: Performance alias syntax.</p>
    <p>| Performance alias syntax.</p>
    <p>| This is the syntax for the base names of the performance data.</p>
    <p>| Possible values are:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(free)           Free memory in bytes (g,m,k,b) or percentages %</p>
    <p>%(size)           Total size of memory</p>
    <p>%(type)           The type of memory to check</p>
    <p>%(used)           Used memory in bytes (g,m,k,b) or percentages %</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)</dt>
  <dd>
    <p>:synopsis: The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)</p>
    <p>| The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)</p>
  </dd>
</dl>
### :query:`check_os_version` ###

    :synopsis: Check the version of the underlaying OS.

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
    <p>:option:`debug` | N/A | Show debugging information in the log</p>
    <p>:option:`show-all` | N/A | Show debugging information in the log</p>
    <p>:option:`filter` |  | Filter which marks interesting items.</p>
    <p>:option:`warning` | version > 50 | Filter which marks items which generates a warning state.</p>
    <p>:option:`warn` |  | Short alias for warning</p>
    <p>:option:`critical` | version > 50 | Filter which marks items which generates a critical state.</p>
    <p>:option:`crit` |  | Short alias for critical.</p>
    <p>:option:`ok` |  | Filter which marks items which generates an ok state.</p>
    <p>:option:`empty-state` | ignored | Return status to use when nothing matched filter.</p>
    <p>:option:`perf-config` |  | Performance data generation configuration</p>
    <p>:option:`top-syntax` | ${status}: ${list} | Top level syntax.</p>
    <p>:option:`ok-syntax` |  | ok syntax.</p>
    <p>:option:`empty-syntax` |  | Empty syntax.</p>
    <p>:option:`detail-syntax` | ${version} (${major}.${minor}.${build}) | Detail level syntax.</p>
    <p>:option:`perf-syntax` | version | Performance alias syntax.</p>
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
  <dt> : synopsis : Show debugging information in the log</dt>
  <dd>
    <p>:synopsis: Show debugging information in the log</p>
    <p>| Show debugging information in the log</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show debugging information in the log</dt>
  <dd>
    <p>:synopsis: Show debugging information in the log</p>
    <p>| Show debugging information in the log</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks interesting items.</dt>
  <dd>
    <p>:synopsis: Filter which marks interesting items.</p>
    <p>| Filter which marks interesting items.</p>
    <p>| Interesting items are items which will be included in the check.</p>
    <p>| They do not denote warning or critical state but they are checked use this to filter out unwanted items.</p>
    <p>| Available options:</p>
    <p>=============== ===============================================================================</p>
    <p>Key             Value</p>
    <p>--------------- -------------------------------------------------------------------------------</p>
    <p>kernel_name     Kernel name</p>
    <p>kernel_release  Kernel release</p>
    <p>kernel_version  Kernel version</p>
    <p>machine         Machine hardware name</p>
    <p>nodename        Network node hostname</p>
    <p>os              Operating system</p>
    <p>processor       Processor type or unknown</p>
    <p>count           Number of items matching the filter</p>
    <p>total            Total number of items</p>
    <p>ok_count         Number of items matched the ok criteria</p>
    <p>warn_count       Number of items matched the warning criteria</p>
    <p>crit_count       Number of items matched the critical criteria</p>
    <p>problem_count    Number of items matched either warning or critical criteria</p>
    <p>list             A list of all items which matched the filter</p>
    <p>ok_list          A list of all items which matched the ok criteria</p>
    <p>warn_list        A list of all items which matched the warning criteria</p>
    <p>crit_list        A list of all items which matched the critical criteria</p>
    <p>problem_list     A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list      A special list with critical, then warning and fainally ok</p>
    <p>status           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>=============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates a warning state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates a warning state.</p>
    <p>| Filter which marks items which generates a warning state.</p>
    <p>| If anything matches this filter the return status will be escalated to warning.</p>
    <p>| Available options:</p>
    <p>=============== ===============================================================================</p>
    <p>Key             Value</p>
    <p>--------------- -------------------------------------------------------------------------------</p>
    <p>kernel_name     Kernel name</p>
    <p>kernel_release  Kernel release</p>
    <p>kernel_version  Kernel version</p>
    <p>machine         Machine hardware name</p>
    <p>nodename        Network node hostname</p>
    <p>os              Operating system</p>
    <p>processor       Processor type or unknown</p>
    <p>count           Number of items matching the filter</p>
    <p>total            Total number of items</p>
    <p>ok_count         Number of items matched the ok criteria</p>
    <p>warn_count       Number of items matched the warning criteria</p>
    <p>crit_count       Number of items matched the critical criteria</p>
    <p>problem_count    Number of items matched either warning or critical criteria</p>
    <p>list             A list of all items which matched the filter</p>
    <p>ok_list          A list of all items which matched the ok criteria</p>
    <p>warn_list        A list of all items which matched the warning criteria</p>
    <p>crit_list        A list of all items which matched the critical criteria</p>
    <p>problem_list     A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list      A special list with critical, then warning and fainally ok</p>
    <p>status           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>=============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Short alias for warning</dt>
  <dd>
    <p>:synopsis: Short alias for warning</p>
    <p>| Short alias for warning</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates a critical state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates a critical state.</p>
    <p>| Filter which marks items which generates a critical state.</p>
    <p>| If anything matches this filter the return status will be escalated to critical.</p>
    <p>| Available options:</p>
    <p>=============== ===============================================================================</p>
    <p>Key             Value</p>
    <p>--------------- -------------------------------------------------------------------------------</p>
    <p>kernel_name     Kernel name</p>
    <p>kernel_release  Kernel release</p>
    <p>kernel_version  Kernel version</p>
    <p>machine         Machine hardware name</p>
    <p>nodename        Network node hostname</p>
    <p>os              Operating system</p>
    <p>processor       Processor type or unknown</p>
    <p>count           Number of items matching the filter</p>
    <p>total            Total number of items</p>
    <p>ok_count         Number of items matched the ok criteria</p>
    <p>warn_count       Number of items matched the warning criteria</p>
    <p>crit_count       Number of items matched the critical criteria</p>
    <p>problem_count    Number of items matched either warning or critical criteria</p>
    <p>list             A list of all items which matched the filter</p>
    <p>ok_list          A list of all items which matched the ok criteria</p>
    <p>warn_list        A list of all items which matched the warning criteria</p>
    <p>crit_list        A list of all items which matched the critical criteria</p>
    <p>problem_list     A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list      A special list with critical, then warning and fainally ok</p>
    <p>status           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>=============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Short alias for critical.</dt>
  <dd>
    <p>:synopsis: Short alias for critical.</p>
    <p>| Short alias for critical.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates an ok state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates an ok state.</p>
    <p>| Filter which marks items which generates an ok state.</p>
    <p>| If anything matches this any previous state for this item will be reset to ok.</p>
    <p>| Available options:</p>
    <p>=============== ===============================================================================</p>
    <p>Key             Value</p>
    <p>--------------- -------------------------------------------------------------------------------</p>
    <p>kernel_name     Kernel name</p>
    <p>kernel_release  Kernel release</p>
    <p>kernel_version  Kernel version</p>
    <p>machine         Machine hardware name</p>
    <p>nodename        Network node hostname</p>
    <p>os              Operating system</p>
    <p>processor       Processor type or unknown</p>
    <p>count           Number of items matching the filter</p>
    <p>total            Total number of items</p>
    <p>ok_count         Number of items matched the ok criteria</p>
    <p>warn_count       Number of items matched the warning criteria</p>
    <p>crit_count       Number of items matched the critical criteria</p>
    <p>problem_count    Number of items matched either warning or critical criteria</p>
    <p>list             A list of all items which matched the filter</p>
    <p>ok_list          A list of all items which matched the ok criteria</p>
    <p>warn_list        A list of all items which matched the warning criteria</p>
    <p>crit_list        A list of all items which matched the critical criteria</p>
    <p>problem_list     A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list      A special list with critical, then warning and fainally ok</p>
    <p>status           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>=============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Return status to use when nothing matched filter.</dt>
  <dd>
    <p>:synopsis: Return status to use when nothing matched filter.</p>
    <p>| Return status to use when nothing matched filter.</p>
    <p>| If no filter is specified this will never happen unless the file is empty.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Performance data generation configuration</dt>
  <dd>
    <p>:synopsis: Performance data generation configuration</p>
    <p>| Performance data generation configuration</p>
    <p>| TODO: obj ( key: value; key: value) obj (key:valuer;key:value)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Top level syntax.</dt>
  <dd>
    <p>:synopsis: Top level syntax.</p>
    <p>| Top level syntax.</p>
    <p>| Used to format the message to return can include strings as well as special keywords such as:</p>
    <p>================== ===============================================================================</p>
    <p>Key                Value</p>
    <p>------------------ -------------------------------------------------------------------------------</p>
    <p>%(kernel_name)     Kernel name</p>
    <p>%(kernel_release)  Kernel release</p>
    <p>%(kernel_version)  Kernel version</p>
    <p>%(machine)         Machine hardware name</p>
    <p>%(nodename)        Network node hostname</p>
    <p>%(os)              Operating system</p>
    <p>%(processor)       Processor type or unknown</p>
    <p>${count}           Number of items matching the filter</p>
    <p>${total}            Total number of items</p>
    <p>${ok_count}         Number of items matched the ok criteria</p>
    <p>${warn_count}       Number of items matched the warning criteria</p>
    <p>${crit_count}       Number of items matched the critical criteria</p>
    <p>${problem_count}    Number of items matched either warning or critical criteria</p>
    <p>${list}             A list of all items which matched the filter</p>
    <p>${ok_list}          A list of all items which matched the ok criteria</p>
    <p>${warn_list}        A list of all items which matched the warning criteria</p>
    <p>${crit_list}        A list of all items which matched the critical criteria</p>
    <p>${problem_list}     A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}      A special list with critical, then warning and fainally ok</p>
    <p>${status}           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : ok syntax.</dt>
  <dd>
    <p>:synopsis: ok syntax.</p>
    <p>| ok syntax.</p>
    <p>| DEPRECATED! This is the syntax for when an ok result is returned.</p>
    <p>| This value will not be used if your syntax contains %(list) or %(count).</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Empty syntax.</dt>
  <dd>
    <p>:synopsis: Empty syntax.</p>
    <p>| Empty syntax.</p>
    <p>| DEPRECATED! This is the syntax for when nothing matches the filter.</p>
    <p>| Possible values are:</p>
    <p>================== ===============================================================================</p>
    <p>Key                Value</p>
    <p>------------------ -------------------------------------------------------------------------------</p>
    <p>%(kernel_name)     Kernel name</p>
    <p>%(kernel_release)  Kernel release</p>
    <p>%(kernel_version)  Kernel version</p>
    <p>%(machine)         Machine hardware name</p>
    <p>%(nodename)        Network node hostname</p>
    <p>%(os)              Operating system</p>
    <p>%(processor)       Processor type or unknown</p>
    <p>${count}           Number of items matching the filter</p>
    <p>${total}            Total number of items</p>
    <p>${ok_count}         Number of items matched the ok criteria</p>
    <p>${warn_count}       Number of items matched the warning criteria</p>
    <p>${crit_count}       Number of items matched the critical criteria</p>
    <p>${problem_count}    Number of items matched either warning or critical criteria</p>
    <p>${list}             A list of all items which matched the filter</p>
    <p>${ok_list}          A list of all items which matched the ok criteria</p>
    <p>${warn_list}        A list of all items which matched the warning criteria</p>
    <p>${crit_list}        A list of all items which matched the critical criteria</p>
    <p>${problem_list}     A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}      A special list with critical, then warning and fainally ok</p>
    <p>${status}           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Detail level syntax.</dt>
  <dd>
    <p>:synopsis: Detail level syntax.</p>
    <p>| Detail level syntax.</p>
    <p>| This is the syntax of each item in the list of top-syntax (see above).</p>
    <p>| Possible values are:</p>
    <p>================== ===============================================================================</p>
    <p>Key                Value</p>
    <p>------------------ -------------------------------------------------------------------------------</p>
    <p>%(kernel_name)     Kernel name</p>
    <p>%(kernel_release)  Kernel release</p>
    <p>%(kernel_version)  Kernel version</p>
    <p>%(machine)         Machine hardware name</p>
    <p>%(nodename)        Network node hostname</p>
    <p>%(os)              Operating system</p>
    <p>%(processor)       Processor type or unknown</p>
    <p>${count}           Number of items matching the filter</p>
    <p>${total}            Total number of items</p>
    <p>${ok_count}         Number of items matched the ok criteria</p>
    <p>${warn_count}       Number of items matched the warning criteria</p>
    <p>${crit_count}       Number of items matched the critical criteria</p>
    <p>${problem_count}    Number of items matched either warning or critical criteria</p>
    <p>${list}             A list of all items which matched the filter</p>
    <p>${ok_list}          A list of all items which matched the ok criteria</p>
    <p>${warn_list}        A list of all items which matched the warning criteria</p>
    <p>${crit_list}        A list of all items which matched the critical criteria</p>
    <p>${problem_list}     A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}      A special list with critical, then warning and fainally ok</p>
    <p>${status}           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Performance alias syntax.</dt>
  <dd>
    <p>:synopsis: Performance alias syntax.</p>
    <p>| Performance alias syntax.</p>
    <p>| This is the syntax for the base names of the performance data.</p>
    <p>| Possible values are:</p>
    <p>================== ===============================================================================</p>
    <p>Key                Value</p>
    <p>------------------ -------------------------------------------------------------------------------</p>
    <p>%(kernel_name)     Kernel name</p>
    <p>%(kernel_release)  Kernel release</p>
    <p>%(kernel_version)  Kernel version</p>
    <p>%(machine)         Machine hardware name</p>
    <p>%(nodename)        Network node hostname</p>
    <p>%(os)              Operating system</p>
    <p>%(processor)       Processor type or unknown</p>
    <p>${count}           Number of items matching the filter</p>
    <p>${total}            Total number of items</p>
    <p>${ok_count}         Number of items matched the ok criteria</p>
    <p>${warn_count}       Number of items matched the warning criteria</p>
    <p>${crit_count}       Number of items matched the critical criteria</p>
    <p>${problem_count}    Number of items matched either warning or critical criteria</p>
    <p>${list}             A list of all items which matched the filter</p>
    <p>${ok_list}          A list of all items which matched the ok criteria</p>
    <p>${warn_list}        A list of all items which matched the warning criteria</p>
    <p>${crit_list}        A list of all items which matched the critical criteria</p>
    <p>${problem_list}     A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}      A special list with critical, then warning and fainally ok</p>
    <p>${status}           The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================== ===============================================================================</p>
  </dd>
</dl>
### :query:`check_uptime` ###

    :synopsis: Check time since last server re-boot.

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
    <p>:option:`debug` | N/A | Show debugging information in the log</p>
    <p>:option:`show-all` | N/A | Show debugging information in the log</p>
    <p>:option:`filter` |  | Filter which marks interesting items.</p>
    <p>:option:`warning` | uptime < 2d | Filter which marks items which generates a warning state.</p>
    <p>:option:`warn` |  | Short alias for warning</p>
    <p>:option:`critical` | uptime < 1d | Filter which marks items which generates a critical state.</p>
    <p>:option:`crit` |  | Short alias for critical.</p>
    <p>:option:`ok` |  | Filter which marks items which generates an ok state.</p>
    <p>:option:`empty-state` | ignored | Return status to use when nothing matched filter.</p>
    <p>:option:`perf-config` |  | Performance data generation configuration</p>
    <p>:option:`top-syntax` | ${status}: ${list} | Top level syntax.</p>
    <p>:option:`ok-syntax` |  | ok syntax.</p>
    <p>:option:`empty-syntax` |  | Empty syntax.</p>
    <p>:option:`detail-syntax` | uptime: ${uptime}h, boot: ${boot} (UTC) | Detail level syntax.</p>
    <p>:option:`perf-syntax` | uptime | Performance alias syntax.</p>
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
  <dt> : synopsis : Show debugging information in the log</dt>
  <dd>
    <p>:synopsis: Show debugging information in the log</p>
    <p>| Show debugging information in the log</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Show debugging information in the log</dt>
  <dd>
    <p>:synopsis: Show debugging information in the log</p>
    <p>| Show debugging information in the log</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks interesting items.</dt>
  <dd>
    <p>:synopsis: Filter which marks interesting items.</p>
    <p>| Filter which marks interesting items.</p>
    <p>| Interesting items are items which will be included in the check.</p>
    <p>| They do not denote warning or critical state but they are checked use this to filter out unwanted items.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>boot           System boot time</p>
    <p>uptime         Time since last boot</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates a warning state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates a warning state.</p>
    <p>| Filter which marks items which generates a warning state.</p>
    <p>| If anything matches this filter the return status will be escalated to warning.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>boot           System boot time</p>
    <p>uptime         Time since last boot</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Short alias for warning</dt>
  <dd>
    <p>:synopsis: Short alias for warning</p>
    <p>| Short alias for warning</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates a critical state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates a critical state.</p>
    <p>| Filter which marks items which generates a critical state.</p>
    <p>| If anything matches this filter the return status will be escalated to critical.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>boot           System boot time</p>
    <p>uptime         Time since last boot</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Short alias for critical.</dt>
  <dd>
    <p>:synopsis: Short alias for critical.</p>
    <p>| Short alias for critical.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Filter which marks items which generates an ok state.</dt>
  <dd>
    <p>:synopsis: Filter which marks items which generates an ok state.</p>
    <p>| Filter which marks items which generates an ok state.</p>
    <p>| If anything matches this any previous state for this item will be reset to ok.</p>
    <p>| Available options:</p>
    <p>============== ===============================================================================</p>
    <p>Key            Value</p>
    <p>-------------- -------------------------------------------------------------------------------</p>
    <p>boot           System boot time</p>
    <p>uptime         Time since last boot</p>
    <p>count          Number of items matching the filter</p>
    <p>total           Total number of items</p>
    <p>ok_count        Number of items matched the ok criteria</p>
    <p>warn_count      Number of items matched the warning criteria</p>
    <p>crit_count      Number of items matched the critical criteria</p>
    <p>problem_count   Number of items matched either warning or critical criteria</p>
    <p>list            A list of all items which matched the filter</p>
    <p>ok_list         A list of all items which matched the ok criteria</p>
    <p>warn_list       A list of all items which matched the warning criteria</p>
    <p>crit_list       A list of all items which matched the critical criteria</p>
    <p>problem_list    A list of all items which matched either the critical or the warning criteria</p>
    <p>detail_list     A special list with critical, then warning and fainally ok</p>
    <p>status          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>============== ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Return status to use when nothing matched filter.</dt>
  <dd>
    <p>:synopsis: Return status to use when nothing matched filter.</p>
    <p>| Return status to use when nothing matched filter.</p>
    <p>| If no filter is specified this will never happen unless the file is empty.</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Performance data generation configuration</dt>
  <dd>
    <p>:synopsis: Performance data generation configuration</p>
    <p>| Performance data generation configuration</p>
    <p>| TODO: obj ( key: value; key: value) obj (key:valuer;key:value)</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Top level syntax.</dt>
  <dd>
    <p>:synopsis: Top level syntax.</p>
    <p>| Top level syntax.</p>
    <p>| Used to format the message to return can include strings as well as special keywords such as:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(boot)           System boot time</p>
    <p>%(uptime)         Time since last boot</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : ok syntax.</dt>
  <dd>
    <p>:synopsis: ok syntax.</p>
    <p>| ok syntax.</p>
    <p>| DEPRECATED! This is the syntax for when an ok result is returned.</p>
    <p>| This value will not be used if your syntax contains %(list) or %(count).</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Empty syntax.</dt>
  <dd>
    <p>:synopsis: Empty syntax.</p>
    <p>| Empty syntax.</p>
    <p>| DEPRECATED! This is the syntax for when nothing matches the filter.</p>
    <p>| Possible values are:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(boot)           System boot time</p>
    <p>%(uptime)         Time since last boot</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Detail level syntax.</dt>
  <dd>
    <p>:synopsis: Detail level syntax.</p>
    <p>| Detail level syntax.</p>
    <p>| This is the syntax of each item in the list of top-syntax (see above).</p>
    <p>| Possible values are:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(boot)           System boot time</p>
    <p>%(uptime)         Time since last boot</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
<dl>
  <dt> : synopsis : Performance alias syntax.</dt>
  <dd>
    <p>:synopsis: Performance alias syntax.</p>
    <p>| Performance alias syntax.</p>
    <p>| This is the syntax for the base names of the performance data.</p>
    <p>| Possible values are:</p>
    <p>================= ===============================================================================</p>
    <p>Key               Value</p>
    <p>----------------- -------------------------------------------------------------------------------</p>
    <p>%(boot)           System boot time</p>
    <p>%(uptime)         Time since last boot</p>
    <p>${count}          Number of items matching the filter</p>
    <p>${total}           Total number of items</p>
    <p>${ok_count}        Number of items matched the ok criteria</p>
    <p>${warn_count}      Number of items matched the warning criteria</p>
    <p>${crit_count}      Number of items matched the critical criteria</p>
    <p>${problem_count}   Number of items matched either warning or critical criteria</p>
    <p>${list}            A list of all items which matched the filter</p>
    <p>${ok_list}         A list of all items which matched the ok criteria</p>
    <p>${warn_list}       A list of all items which matched the warning criteria</p>
    <p>${crit_list}       A list of all items which matched the critical criteria</p>
    <p>${problem_list}    A list of all items which matched either the critical or the warning criteria</p>
    <p>${detail_list}     A special list with critical, then warning and fainally ok</p>
    <p>${status}          The returned status (OK/WARN/CRIT/UNKNOWN)</p>
    <p>================= ===============================================================================</p>
  </dd>
</dl>
