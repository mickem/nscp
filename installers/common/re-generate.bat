"C:/Program Files (x86)/WiX Toolset v3.9\bin\heat.exe" dir ..\..\web -cg WEBResources -gg -out web.wxs -var var.WebSource
"C:/Program Files (x86)/WiX Toolset v3.9\bin\heat.exe" dir ..\..\scripts -cg ScriptResources -gg -out scripts.wxs -var var.ScriptSource
"C:/Program Files (x86)/WiX Toolset v3.9\bin\heat.exe" dir ..\..\op5\scripts -cg Op5ScriptResources -gg -out op5_scripts.wxs -var var.OP5ScriptSource
"C:/Program Files (x86)/WiX Toolset v3.9\bin\heat.exe" dir ..\..\op5\config -cg Op5ConfigResources -gg -out op5_config.wxs -var var.OP5ConfigSource
"C:/Program Files (x86)/WiX Toolset v3.9\bin\heat.exe" dir "C:\source\build\x64\dev\docs\html" -cg HTMLHelp -gg -out html_help.wxs -var var.WebHelpSource
