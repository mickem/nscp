set(CURRENT_MODULE_NAME "NSCP.Plugin.CSharpSample")
if(CSHARP_FOUND AND WIN32)
  set(BUILD_MODULE 0)
else()
  set(BUILD_MODULE_SKIP_REASON "CSharp not found")
endif()
