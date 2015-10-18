' Author: Mattias Ryrlén (mr@op5.com) 
' Website: http://www.op5.com 
' Created: 2008-09-18 
' Updated: 2008-10-09 
' Version: 0.9.1
' Description: Check the offset of your server vs your Active Directory Domain.
' 
' Usage cscript /NoLogo check_ad_time.vbs <domain> "<offset>"
'
' Changelog:
'
' 0.9.1:
' * Fixed timeformat (i think, needs feedback).
' * Changed /domain to /computers, still works to use the AD domain. eg domain.com
'
' 0.9:
' Initial Release
'

Err = 3
msg = "UNKNOWN"

Set Args = WScript.Arguments
If WScript.Arguments.Count <= 1 Then
	Usage()
Else
	domain = Args.Item(0)

	offset = Args.Item(1)

	offset = Replace(offset,",",".")

	Set objShell = CreateObject("Wscript.Shell")
	strCommand = "C:\windows\system32\w32tm.exe /monitor /computers:" & domain
	set objProc = objShell.Exec(strCommand)

	input = ""
	strOutput = ""

	Do While Not objProc.StdOut.AtEndOfStream
		input = objProc.StdOut.ReadLine
	
		If InStr(input, "NTP") Then
			strOutput = input
		End If
	Loop

	Set myRegExp = New RegExp
	myRegExp.IgnoreCase = True
	myRegExp.Global = True
	myRegExp.Pattern = "    NTP: ([+-]+)([0-9]+).([0-9]+)s offset"
	
	Set myMatches = myRegExp.Execute(strOutput)
	
	result = ""
	dir = ""
	
	For Each myMatch in myMatches
		If myMatch.SubMatches.Count > 0 Then
			For I = 0 To myMatch.SubMatches.Count - 1
				If I = 0 Then
					dir = myMatch.SubMatches(I)
				ElseIf I > 1 Then
					result = result & "." & myMatch.SubMatches(I)
				Else
					result = result & myMatch.SubMatches(I)
				End If
			Next
		End If
	Next
	
	If Left(dir, 1) = "-" Then
		result = CDbl(result)
	Else
		result = CDbl("-" & result)
	End If

	If result > CDbl(offset) OR result < -CDbl(offset) Then
		Err = 2
		msg = "NTP CRITICAL: Offset " & result & " secs|offset: " & result & ";0;" & Replace(CDbl(offset),",",".") & ";"
	Else
		Err = 0
		msg = "NTP OK: Offset " & result & " secs|offset: " & result & ";0;" & Replace(CDbl(offset),",",".") & ";"
	End If
End If

Wscript.Echo msg
Wscript.Quit(Err)

Function Usage()
	Err = 3
	WScript.Echo "Usage cscript /NoLogo check_ad_time.vbs <domain> ""<offset>"""
	Wscript.Echo ""
	Wscript.Echo "domain			Name of domain to check roles on"
	Wscript.Echo ""
	Wscript.Echo "offset			total number of seconds offset allowed."
	Wscript.Echo "			will check both positive and negative"
	Wscript.Echo ""
	Wscript.Echo "Example: cscript /NoLogo check_ad_time.vbs mydomain.com ""0.4"""
	Wscript.Quit(Err)
End Function