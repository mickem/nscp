' Services.vbs
' Script to List running autostarted services
' www.computerperformance.co.uk/
' Author Guy Thomas http://computerperformance.co.uk/
' Version 1.5 December 2005
'
' Modified by Per Asberg Dec 2010, op5 AB, http://www.op5.com
' Modified by Peter Ostlin May 2011, op5 AB, http://www.op5.com
' -------------------------------------------------------'
Option Explicit
Dim icnt, cnt, page, start, objWMIService, objItem, objService, strServiceList
Dim colListOfServices, strComputer, strService, Args

'On Error Resume Next

' ---------------------------------------------------------

cnt 	= 0	' tot count
icnt 	= 0	' count listed (returned) services
page 	= 20	' nr of services to include (pagination)
start 	= 0	' where to start (pagination)

Set Args  = WScript.Arguments.Named

If Args.Exists("start") Then start = Cint(Args("start"))

strComputer = "."
Set objWMIService = GetObject("winmgmts:" _
& "{impersonationLevel=impersonate}!\\" _
& strComputer & "\root\cimv2")
Set colListOfServices = objWMIService.ExecQuery _
("Select * from Win32_Service WHERE StartMode='auto' AND name != 'NSClientpp'")

' WMI and VBScript loop
For Each objService in colListOfServices
  If icnt < page AND cnt >= start THEN
	strServiceList = strServiceList & objService.name & ","
	icnt = icnt +1
  End if
  cnt = cnt + 1
Next

WScript.Echo strServiceList

' End of WMI script to list services