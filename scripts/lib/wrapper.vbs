' =========================================================
' Example file of setting up a script to use NagiosPlugin.vbs as base.
' =========================================================
' Setting up usage of NagiosPlugin Class
Dim ret
ret = includeFile("scripts\lib\NagiosPlugins.vbs")
If ret(0) <> 0 Then
	WScript.Echo "Failed to load core: " & ret(0) & " - " & ret(1)
	Wscript.Quit(3)
End If

' If we have no args or arglist contains /help or not all of the required arguments are fulfilled show the usage output,.
If WScript.Arguments.Count = 0 Then
	WScript.Echo "Usage: cscript //NOLOGO [c-script options] wrapper.vbs <script.vbs> [...]" & WScript.Arguments.Count
	Wscript.Quit(3)
End If

ret = includeFile(WScript.Arguments(0))
If ret(0) <> 0 Then
	WScript.Echo "Failed to run script: " & ret(0) & " - " & ret(1)
	Wscript.Quit(3)
End If

Function includeFile (file)
    Dim oFSO, oFile, strFile, eFileName
  
    Set oFSO = CreateObject ("Scripting.FileSystemObject")
	eFileName = file
    If Not oFSO.FileExists(eFileName) Then
		eFileName = oFSO.getFolder(".") & "\" & file
		If Not oFSO.FileExists(eFileName) Then
			eFileName = oFSO.getFolder(".") & "\scripts\" & file
			If Not oFSO.FileExists(eFileName) Then
				includeFile = Array("1", "The specified file " & file & " does not exist")
				Exit Function
			End If
		End If
	End If
    On Error Resume Next
    Err.Clear

	Set oFile = oFSO.OpenTextFile(eFileName)
	If Err.Number <> 0 Then
		includeFile = Array(Err.Number, "The specified file " & file & " could not be opened for reading.")
		Exit Function
	End If
	On Error GoTo 0
	strFile = oFile.ReadAll
	oFile.close
	Set oFso = Nothing
	Set oFile = Nothing
	ExecuteGlobal strFile
	includeFile = Array("0", "")
End Function
