' Author: Mattias Ryrlén (mr@op5.com)
' Website: http://www.op5.com
' Created 2008-10-26
' Updated 2008-10-26
' Description: Class to ease the output from nagios plugins

' Exitcodes
Const OK = 0
Const WARNING = 1
Const CRITICAL = 2
Const UNKNOWN = 3

Dim return_code			' String to set exitcode
Dim msg					' Output string for your check
Dim np					' Object to be used with NagiosPlugin Class
Dim threshold_warning
Dim threshold_critical
Dim Args				' List to hold your arguments
Dim Timeout
Dim ArgList()		' Array for your arguments to be used
Dim tmpArgList()

' Default values
return_code = UNKNOWN
threshold_warning = "N/A"
threshold_critical = "N/A"
Timeout = 10
ReDim tmpArgList(2)

' Create alias for arguments method
Set Args = WScript.Arguments.Named

Class NagiosPlugin
	Public Function nagios_exit (msg, return_code)
		' Function to exit the plugin with text and exitcode
		If return_code = 0 Then
			msg = "OK: " & msg
		End If
		
		If return_code = 1 Then
			msg = "WARNING: " & msg
		End If

		If return_code = 2 Then
			msg = "CRITICAL: " & msg
		End If

		If return_code >= 3 Then
			msg = "UNKNOWN: " & msg
		End If
		Wscript.Echo msg
		Wscript.Quit(return_code)
	End Function
	
	Public Function add_perfdata (label, value, unit, threshold)
		' Adds perfdata to the output
	End Function
	
	Public Function parse_args ()
		' Start the real parsing to see if we meet all required arguments needed.
		totalArg = UBound(ArgList)
		parse_args = 1
		
		i = 0
		Do While i < totalArg
			If ArgList(i,2) = 1 Then
				If Not Args.Exists(ArgList(i,0)) Then
					parse_args = 0
				End If
			End If
			i = i + 1
		Loop
	End Function
	
	Public Function add_arg (parameter, help, required)
		' Add an argument to be used, make it required or optional.
		totalArg = UBound(tmpArgList)
		
		if tmpArgList(2) <> "" Then
			totalArg = totalArg + 3
			ReDim Preserve tmpArgList(totalArg)		
			
			tmpArgList(totalArg - 2) = parameter
			tmpArgList(totalArg - 1) = help
			tmpArgList(totalArg) = required
		Else
			tmpArgList(0) = parameter
			tmpArgList(1) = help
			tmpArgList(2) = required
		End If

		Erase ArgList
		ReDim ArgList(Round(totalArg / 3), 2)		

		i = 0
		subi = 0
		For Each arg In tmpArgList
			ArgList(i, subi) = arg
			If subi >= 2 Then
				subi = 0
				i = i + 1
			Else
				subi = subi + 1
			End if
		Next
	End Function
	
	Public Function set_thresholds (warning, critical)
		' Simple function to set warning/critical thresholds
		threshold_warning = warning
		threshold_critical = critical
	End Function

	Public Function get_threshold (threshold)
		' Simple function to return the warning and critical threshold
		If LCase(threshold) = "warning" Then
			get_threshold = threshold_warning
		End IF
		
		If LCase(threshold) = "critical" Then
			get_threshold = threshold_critical
		End If
	End Function
	
	Public Function escalate_check_threshold (current, value)
		result = check_threshold (value)
		escalate_check_threshold = escalate(current, result)
	End Function
	
	Public Function escalate (current, newValue)
		If newValue > current Then
			escalate = newValue
		Else
			escalate = current
		End If
	End Function



	Public Function get_threshold_perfdat(string)

		Dim cintw0
		Dim cintw
		Dim x
		Dim colon
		
		cintw0=get_threshold(string)
		x=Replace(cintw0,"~","")
		cintw0=Replace(x,"@","")
		
		colon=Instr(cintw0,":")
		
		If (colon > 1) Then
			cintw=Left(cintw0,colon-1)
		Else
			If (colon=1) Then
				cintw=Mid(cintw0,2)
			Else
				cintw=cintw0
			End If
		End If
		
		get_threshold_perfdat=cintw
	
	End Function
	

	Public Function check_threshold (value)
		' Verify the thresholds for warning and critical
		' Return 0 if ok (don't generate alert)
		' Return 1 if within warning (generate alert)
		' Return 2 if within critical (generate alert)
		
		'Option	Range definition			Generate an alert if x...
		'1		10					< 0 or > 10, (outside the range of {0 .. 10})
		'2		10:					< 10, (outside {10 .. infinity})
		'3		~:10					> 10, (outside the range of {-infinity .. 10})
		'4		10:20					< 10 or > 20, (outside the range of {10 .. 20})
		'5		@10:20				>= 10 and <= 20, (inside the range of {10 .. 20})
		
		check_threshold = 0
		
		value = CDbl(value)
		
		Set re = New RegExp
		re.IgnoreCase = True

		' Option 1
		re.Pattern = "^[0-9]+$"
		If re.Test(get_threshold("warning")) Then
			warning_nr = parse_range(get_threshold("warning"), value, 1)
		End If
		If re.Test(get_threshold("critical")) Then
			critical_nr = parse_range(get_threshold("critical"), value, 1)
		End If

		' Option 2
		re.Pattern = "^[0-9]+:$"
		If re.Test(get_threshold("warning")) Then
			warning_nr = parse_range(get_threshold("warning"), value, 2)
		End If
		If re.Test(get_threshold("critical")) Then
			critical_nr = parse_range(get_threshold("critical"), value, 2)
		End If

		' Option 3
		re.Pattern = "^~:[0-9]+$"
		If re.Test(get_threshold("warning")) Then
			warning_nr = parse_range(get_threshold("warning"), value, 3)
		End If
		If re.Test(get_threshold("critical")) Then
			critical_nr = parse_range(get_threshold("critical"), value, 3)
		End If

		' Option 4
		re.Pattern = "^[0-9]+:[0-9]+$"
		If re.Test(get_threshold("warning")) Then
			warning_nr = parse_range(get_threshold("warning"), value, 4)
		End If
		If re.Test(get_threshold("critical")) Then
			critical_nr = parse_range(get_threshold("critical"), value, 4)
		End If

		' Option 5
		re.Pattern = "^@[0-9]+:[0-9]+$"
		If re.Test(get_threshold("warning")) Then
			warning_nr = parse_range(get_threshold("warning"), value, 5)
		End If
		If re.Test(get_threshold("critical")) Then
			critical_nr = parse_range(get_threshold("critical"), value, 5)
		End If
		
		If warning_nr > 0 And critical_nr < 1 Then
			check_threshold = 1
		End If
		
		If critical_nr > 0 Then
			check_threshold = 2
		End if
		
		'Wscript.Echo "warning/critical: " & warning_nr & "/" & critical_nr
	End Function
	
	Private Function parse_range (threshold, value, myOpt)
	
		'Option	Range definition			Generate an alert if x...
		'1		10					< 0 or > 10, (outside the range of {0 .. 10})
		'2		10:					< 10, (outside {10 .. infinity})
		'3		~:10					> 10, (outside the range of {-infinity .. 10})
		'4		10:20					< 10 or > 20, (outside the range of {10 .. 20})
		'5		@10:20				>= 10 and <= 20, (inside the range of {10 .. 20})

		parse_range = 3
		
		Set re = New RegExp
		re.IgnoreCase = True

		' Make sure that "value" is of type Integer 
		value = CDbl(value) 		

		Select Case myOpt
			' Generate an alert if x ...
			Case 1
				' outside the range of 0-threshold
				re.Pattern = "^([0-9]+)$"
				Set threshold = re.Execute(threshold)
				
				If value < 0 Or value > CDbl(threshold(0)) Then
					parse_range = 1
				Else
					parse_range = 0
				End If

			Case 2
				' outside value -> iinfinity
				re.Pattern = "^([0-9]+):$"
				Set threshold = re.Execute(threshold)

				
				For Each thres In threshold
                                        'Wscript.Echo "SubMatches(0): " & thres.SubMatches(0) & " val: " & value
					If value < CDbl(thres.SubMatches(0)) Then
						parse_range = 1
					Else
						parse_range = 0
					End If
				Next
				


			Case 3
				' outside the range infinity <- value
				re.Pattern = "^~:([0-9]+)$"
				Set threshold = re.Execute(threshold)

				For Each thres In threshold
					If value > CDbl(thres.SubMatches(0)) Then
						parse_range = 1
					Else
						parse_range = 0
					End If
				Next


			Case 4
				' outside the range of value:value
				re.Pattern = "^([0-9]+):([0-9]+)$"
				Set threshold = re.Execute(threshold)
				
				For Each thres In threshold
					If value < CDbl(thres.SubMatches(0)) Or value > CDbl(thres.SubMatches(1)) Then
						parse_range = 1
					Else
						parse_range = 0
					End If
				Next
				
			Case 5
				re.Pattern = "^@([0-9]+):([0-9]+)$"
				Set threshold = re.Execute(threshold)
				
				For Each thres In threshold
					If value >= CDbl(thres.SubMatches(0)) And value <= CDbl(thres.SubMatches(1)) Then
						'Wscript.Echo "Bigger than " & thres.SubMatches(0) & " and smaller than " & thres.SubMatches(1)
						parse_range = 1
					Else
						parse_range = 0
					End If
				Next

		End Select
	End Function
	
	Public Function Usage
		' Print the usage output, automaticly build by the add_arg functions.
		i = 0
		r = 0
		o = 0

		Dim reqArgLong()
		Dim optArgLong()
		Dim value

		Do While i <= Ubound(ArgList)

			If ArgList(i,0) <> "" Then
				ReDim Preserve reqArgLong(r)
				ReDim Preserve optArgLong(o)
				value = "<value>"
				
				If Args.Exists(ArgList(i,0)) Then
					value = Args(ArgList(i,0))
				End If
				
				If ArgList(i,2) = 1 Then
					reqArgShort = reqArgShort & "/" & ArgList(i, 0) & ":" & value & " "
					reqArgLong(r) = tabilize("/" & ArgList(i, 0), ArgList(i, 1))
					r = r + 1
				Else
					optArgShort = optArgShort & "[/" & ArgList(i, 0) & ":" & value & "] "
					optArgLong(o) = tabilize("[/" & ArgList(i, 0) & "]", ArgList(i, 1))
					o = o + 1
				End If
			End If
			i = i + 1
		Loop
		Wscript.Echo "Usage: " & PROGNAME & " " & reqArgShort & optArgShort
		Wscript.Echo ""
		
		i = 0
		Do While i <= Ubound(reqArgLong)
			Wscript.Echo reqArgLong(i)
			i = i + 1
		Loop
		
		i = 0
		Do While i <= Ubound(optArgLong)
			Wscript.Echo optArgLong(i)
			i = i + 1
		Loop

		Wscript.Quit(UNKNOWN)
	End Function
	
	Private Function tabilize (name, txt)
		' Add some space to make it pretty
		MaxWith = 30
		command = Len(name)
		MaxWith = MaxWith - command
		
		tabilize = name & space(MaxWith) & txt
	End Function
	
	Public Function simple_WMI_CIMV2(strComputer, strQuery)
		Const wbemFlagReturnImmediately = &h10
		Const wbemFlagForwardOnly       = &h20

		Set objWMIService = GetObject( "winmgmts://" & strComputer & "/root/CIMV2" )
		Set simple_WMI_CIMV2 = objWMIService.ExecQuery(strQuery, "WQL", wbemFlagReturnImmediately + wbemFlagForwardOnly )
'		Set simple_WMI_CIMV2 = objWMIService.ExecQuery(strQuery, "WQL")
	End Function
End Class
