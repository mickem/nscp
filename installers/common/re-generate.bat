"C:/Program Files (x86)/WiX Toolset v3.10\bin\heat.exe" dir ..\..\web -cg WEBResources -gg -out web.wxs
"C:/Program Files (x86)/WiX Toolset v3.10\bin\heat.exe" dir ..\..\scripts -cg ScriptResources -gg -out scripts.wxs
"C:/Program Files (x86)/WiX Toolset v3.10\bin\heat.exe" dir ..\..\op5\scripts -cg Op5ScriptResources -gg -out op5_scripts.wxs
"C:/Program Files (x86)/WiX Toolset v3.10\bin\heat.exe" dir ..\..\op5\config -cg Op5ConfigResources -gg -out op5_config.wxs
"C:/Program Files (x86)/WiX Toolset v3.10\bin\heat.exe" dir "C:\source\build\x64\dist\ext\docs\html" -cg HTMLHelp -gg -out html_help.wxs
