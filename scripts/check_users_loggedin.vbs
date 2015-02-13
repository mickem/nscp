'########################################################################
'# Name: Michel van Son (michelvs@phison.nl)				#
'# Company: Phison Technologies         				#
'# Date: 12-02-2010							#
'# Purpose: External check script to get number of users logged in	#
'# Changes: [dd-mm-yyyy] Name | changes					#
'########################################################################

'################################################################################################################
'# Without arguments, the script will only return the number of users logged in with exit code 0                #
'# When supplying arguments, the first argument is the warning treshold and the second is the critical treshold	#
'# More or less then 2 arguments will result in an error with status Unknown(3)					#
'################################################################################################################

Option Explicit

'Declare variables.
Dim objWMIService
Dim objUsers
Dim valUsers

' Required Variables
Const PROGNAME = "check_users_loggedin"
Const VERSION = "0.0.1"

' Default settings for your script.
threshold_warning = 1
threshold_critical = 2

' Create the NagiosPlugin object
Set np = New NagiosPlugin

' Define what args that should be used
np.add_arg "warning", "warning threshold", 0
np.add_arg "critical", "critical threshold", 0

' If we have no args or arglist contains /help or not all of the required arguments are fulfilled show the usage output,.
If Args.Exists("help") Then
	np.Usage
End If

' If we define /warning /critical on commandline it should override the script default.
If Args.Exists("warning") Then threshold_warning = Args("warning")
If Args.Exists("critical") Then threshold_critical = Args("critical")
np.set_thresholds threshold_warning, threshold_critical


'Query the WMI service for the users.
'Use 2 WHERE clauses for Interactive(2) and RemoteInteractive(10) logons.
'More information: http://msdn.microsoft.com/en-us/library/aa394189%28VS.85%29.aspx
Set objWMIService = GetObject("winmgmts:" & "{impersonationLevel=impersonate}!\\localhost\root\cimv2")
Set objUsers = objWMIService.ExecQuery ("Select * from Win32_LogonSession Where LogonType = 2 OR LogonType = 10")
valUsers = objUsers.Count
valUsers = CInt(valUsers)

return_code = OK

'Check the user count, echo it and exit with proper exitcode.
if valUsers > 0 then
	return_code = np.check_threshold(valUsers)
	if Not IsEmpty(threshold_critical) then
		if valUsers >= threshold_critical then
			np.nagios_exit "Critical: Users logged in: " & valUsers & "|'users'=" & valUsers & ";" & threshold_warning & ";" & threshold_critical, return_code
		elseif valUsers >= threshold_warning then
			np.nagios_exit "Warning: Users logged in: " & valUsers & "|'users'=" & valUsers & ";" & threshold_warning & ";" & threshold_critical, return_code
		else
			np.nagios_exit "OK: Users logged in: " & valUsers & "|'users'=" & valUsers & ";" & threshold_warning & ";" & threshold_critical, return_code
		end if
	else
                np.nagios_exit "OK: Users logged in: " & valUsers & "|'users'=" & valUsers, OK
	end if
else
	np.nagios_exit "OK: No users logged in.|'users'=" & valUsers, OK
end if
