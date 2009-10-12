Main changes and updates in the 0.3.7 version
 + Added argument support to NRPE Client
 * Some additions and fixes CheckWMI
 * Improved installer (works on w2k8 etc)
 * NSCA feature and stability improvments
 * New command line switchs to easily use NSClient++ from external scripts
 * Added "firewall exception" to installer
 * Fixed an issue with the socket data buffer
 * Fixed issue with CheckExternalScripts and script_dir
 * Fixed issue with CheckDisk and paths

Main changes and updates in the 0.3.6 version
 * Improved installer
 * A lot of bugfixes and improvements 
 * Serious memory leak fixed
 * Added a few new options to NSCA module
 * New service name and description
 * Improved CHeckFile2 (new option max-dir-depth, path, pattern)
 * Added support for changing name and description of service from the /install command line
 * Added more filter operatos to all numeric filters so they accept eq:, ne:, gt:, lt: in addition to =, >, <, <>, !, !=, in: (#269)
 * Added better support for numerical hit matching in the eventlog module. You can now use exact and detailed matching.
 * Cleaned up the checkProcState code and it is not a lot better.
 * Added new option 16bit to checkProcState. When set checkProcState will enumerate all 16 bit processes found running under NTVDM.
 * Added new command line options pdhlookup and pdhmatch (to CheckSystem) to lookup index and names.
 * Added new module A_DebugLogMetrics.dll which can be used to generate debug info.
 * Brand new build enviornment based upon boost build!!!
 * Modified /about so it now shows a lot of usefull(?) info.
