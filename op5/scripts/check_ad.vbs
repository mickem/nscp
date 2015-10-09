'Script to check the status of a DOMAIN controller and report to Nagios
'requires DCDIAG.EXE 
'Author: Felipe Ferreira
'Version: 3.0
'
'Mauled over by John Jore, j-o-h-n-a-t-j-o-r-e-d-o-t-n-o 16/11/2010 to work on W2K8, x32
'as well as remove some, eh... un-needed lines of code, general optimization as well as adding command parameter support
'This is where i found the original script, http://felipeferreira.net/?p=315&cpage=1#comments
'Tested by JJ on W2K8 SP2, x86
'		 W2K3 R2 SP2, x64
'Version 3.0-JJ-V0.2
'Todo: Proper error handling
'      Add /help parameter
'      Add support for the two tests which require additional input (dcpromo is one such test)
'Version 3.0-JJ-V0.3
'	Removed some surplus language detection code
'		Including non-working English test on a W2K8 x32 DC
'	Added support for multi-partition checks like 'crossrefvalidation'. Previously the last status result would mask previous failures
'	Incorporated Jonathan Vogt's german and multiline tests


'Force all variables to be declared before usage
option explicit

'Array for name and status (Ugly, but redim only works on last dimension, and can't set initial size if redim 
dim name(), status()
redim preserve name(0)
redim preserve status(0)
redim preserve lock(0)

'Debug switch
dim verbose : verbose = 0

'Return variables for NAGIOS
const intOK = 0
const intWarning = 1 'Not used. What dcdiag test would be warning instead of critical?
const intCritical = 2
const intUnknown = 3

'Lang dependend. Default is english
dim strOK : strOK = "passed"
dim strNotOK : strNotOk = "failed"

'Call dcdiag and grab relevant output
exec(cmd)

'Generate NAGIOS compatible output from dcdiag
printout()

'call dcdiag and parse the output
sub exec(strCmd)
	'Declare variables
	dim objShell : Set objShell = WScript.CreateObject("WScript.Shell")
	dim objExecObject, lineout, tmpline
	lineout = ""
	'Command line options we're using
	pt strCmd

	Set objExecObject = objShell.Exec(strCmd)
	'Loop until end of output from dcdiag
	do While not objExecObject.StdOut.AtEndOfStream
		tmpline = lcase(objExecObject.StdOut.ReadLine())

		'Check the version of DCDiag being used and change the global 'passed' / 'failed' strings
		call DetectLang(tmpline)

              	if (instr(tmpline, ".....")) then 
			'testresults start with a couple of dots, reset the lineout buffer
			lineout= tmpline
			'pt "lineout buffer '" & lineout & "'"
		else
			'Else append the next line to the buffer to capture multiline responses
			lineout = lineout + tmpline
			'pt "lineout buffer appended '" & lineout & "'"
		end if

		if instr(lineout, lcase(strOK)) then
			'we have a strOK String which means we have reached the end of a result output (maybe on newline)
			call parse(lineout)
			lineout = ""
		end if 
	loop

	'Why call this at the end? Is it not pointless as we've looped through the entire output from dcdiag in the above loop?!?
	'call parse(lineout)
end sub


sub DetectLang(txtp)

	'Change from looking for English words if we find the string below:
	if (instr(lcase(txtp), lcase("Verzeichnisserverdiagnose"))) then 'German
                pt "Detected German Language, changing the global test strings to look for"
		strOK = "bestanden"
		strNotOk = "nicht bestanden"
	end if

end sub


sub parse(txtp)
	'Parse output of dcdiag command and change state of checks
	dim loop1

        'Is this really required? Or is it for pretty debug output only?
	txtp = Replace(txtp,chr(10),"") ' Newline
	txtp = Replace(txtp,chr(13),"") ' CR
	txtp = Replace(txtp,chr(9),"")  ' Tab
	do while instr(txtp, "  ")
	  txtp = Replace(txtp,"  "," ") ' Some tidy up
	loop
	
	' We have to test twice because some localized (e.g. German) outputs simply use 'not', or 'nicht' as a prefix instead of 'passed' / 'failed'
	if instr(lcase(txtp), lcase(strOK)) then
		'What are we testing for now?
		pt "Checking :" & txtp & "' as it contains '" & strOK & "'"
		'What services are ok? 'By using instr we don't need to strip down text, remove vbCr, VbLf, or get the hostname
		for loop1 = 0 to Ubound(name)-1
			if (instr(lcase(txtp), lcase(name(loop1)))) AND (lock(loop1) = FALSE) then 
				status(loop1)="OK"
				pt "Set the status for test '" & name(loop1) & "' to '" & status(loop1) & "'"
			end if
		next
	end if

	' if we find the strNotOK string then reset to CRITICAL
	if instr(lcase(txtp), lcase(strNotOK)) then
		'What are we testing for now?
		pt txtp
		for loop1 = 0 to Ubound(name)-1
			if (instr(lcase(txtp), lcase(name(loop1)))) then 
				status(loop1)="CRITICAL"
				'Lock the variable so it can't be reset back to success. Required for multi-partition tests like 'crossrefvalidation'
				lock(loop1)=TRUE
				pt "Reset the status for test '" & name(loop1) & "' to '" & status(loop1) & "' with a lock '" & lock(loop1) & "'"
			end if
		next
	end if
end sub

'outputs result for NAGIOS
sub printout()
	dim loop1, msg : msg = ""

	for loop1 = 0 to ubound(name)-1
		msg = msg & name(loop1) & ": " & status(loop1) & ". "
	next

	'What state are we in? Show and then quit with NAGIOS compatible exit code
	if instr(msg,"CRITICAL") then
		wscript.echo "CRITICAL - " & msg
		wscript.quit(intCritical)
	else
		wscript.echo "OK - " & msg
		wscript.quit(intOK)
	end if
end sub

'Print messages to screen for debug purposes
sub pt(msgTxt)
	if verbose then
		wscript.echo msgTXT
	end if
end sub

'What tests do we run?
function cmd()
	dim loop1, test, tests
	const intDefaultTests = 6

	cmd = "dcdiag " 'Start with this

	'If no command line parameters, then go with these defaults
	if Wscript.Arguments.Count = 0 Then
		redim preserve name(intDefaultTests)
		redim preserve status(intDefaultTests)
		redim preserve lock(intDefaultTests)
		'Test name
		name(0) = "services"
		name(1) = "replications"
		name(2) = "advertising"
		name(3) = "fsmocheck"
		name(4) = "ridmanager"
		name(5) = "machineaccount"

		'Set default status for each named test
		for loop1 = 0 to (ubound(name)-1)
			status(loop1) = "CRITICAL"
			lock(loop1) = FALSE
			cmd = cmd & "/test:" & name(loop1) & " "
		next
	else
		'User specified which tests to perform.

		for loop1 = 0 to wscript.arguments.count - 1
			if (instr(lcase(wscript.Arguments(loop1)), lcase("/test"))) then
			
			'If parameter is wrong, give some hints
			if len(wscript.arguments(loop1)) < 6 then
				wscript.echo "Unknown parameter. Provide name of tests to perform like this:"
				wscript.echo vbTAB & "'cscript //nologo " & Wscript.ScriptName & " /test:advertising,dfsevent'"
				wscript.quit(intUnknown)
			end if
			
			'Strip down the test to individual items
			tests = right(wscript.arguments(loop1), len(wscript.arguments(loop1))-6)
			pt "Tests: '" & tests & "'"

			tests = split(tests,",")
			for each test in tests
				cmd = cmd  & " /test:" & test

				'Expand the array to make room for one more test
				redim preserve name(ubound(name)+1)
				redim preserve status(ubound(status)+1)
				redim preserve lock(ubound(lock)+1)

				'Store name of test and status
				name(Ubound(name)-1) = test
				status(Ubound(status)-1) = "CRITICAL" 'Default status. Change to OK if test is ok
				lock(Ubound(lock)-1) = FALSE 'Don't lock the variable yet.

				'pt "Contents: " & name(Ubound(name)-1) & " " & status(Ubound(status)-1)
			next
			end if
		next
	end if
	'We end up with this to test:
	pt "Command to run: " & cmd
end function