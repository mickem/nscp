// This is the main DLL file.

#include "stdafx.h"
#using <lib/CsharpSamplePlugin.dll>
#include "SampleManagedPlugin.h"

SampleManagedPlugin<CsharpSamplePlugin::SamplePlugin> gPluginImpl;

NSC_WRAPPERS_MAIN_DEF(gPluginImpl);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gPluginImpl);
