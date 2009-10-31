#pragma once
//-------------------------------------------------------------------------------------------------
// <copyright file="precomp.h" company="Microsoft">
//    Copyright (c) Microsoft Corporation.  All rights reserved.
//    
//    The use and distribution terms for this software are covered by the
//    Common Public License 1.0 (http://opensource.org/licenses/cpl.php)
//    which can be found in the file CPL.TXT at the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by
//    the terms of this license.
//    
//    You must not remove this notice, or any other, from this software.
// </copyright>
// 
// <summary>
//    Precompiled header for Firewall CA
// </summary>
//-------------------------------------------------------------------------------------------------

#include <windows.h>
#include <msidefs.h>
#include <msiquery.h>
//#include <strsafe.h>
#ifdef USE_PSDK
#include <netfw.h>
#endif

//#include "wcautil.h"
//#include "fileutil.h"
//#include "pathutil.h"
//#include "strutil.h"

//#include "CustomMsiErrors.h"
#include "cost.h"
