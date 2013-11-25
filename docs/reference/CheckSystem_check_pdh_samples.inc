Checking specific Counter (\System\System Up Time) ::

	check_pdh "counter=\\System\\System Up Time" "warn=value > 5" "crit=value > 9999"
	\System\System Up Time = 204213
	'\System\System Up Time value'=204213;5;9999

Using the **expand index** to check for translated counters::

	check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999" expand-index
	Everything looks good
	'\Minne\Dedikationsgräns value'=-2147483648;5;9999

Checking **translated counters** without expanding indexes::

	check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999"
	Everything looks good
	'\4\30 value'=-2147483648;5;9999

Checking **large values** using the type=large keyword::

	check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999" flags=nocap100 expand-index type=large
	\Minne\Dedikationsgräns = 25729224704
	'\Minne\Dedikationsgräns value'=25729224704;5;9999

Using real-time checks to check avergae values over time.

Here we configure a counter to be checked at regular intervals and the value is added to a rrd buffer.
The configuration from nsclient.ini::

	[/settings/system/windows/counters/foo]
	collection strategy=rrd
	type=large
	counter=counter=\Processor(_total)\% Processor Time

Then we can check the value (average over time)::

	check_pdh "counter=foo" "warn=value > 80" "crit=value > 90"
	Everything looks good
	'foo value'=18;80;90
	
To specify a time you use the time option::

	check_pdh "counter=foo" "warn=value > 80" "crit=value > 90" time=30s
	Everything looks good
	'foo value'=3;80;90