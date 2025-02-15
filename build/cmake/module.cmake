source_group("Common Files" REGULAR_EXPRESSION .*include/.*)
source_group("Parser" REGULAR_EXPRESSION .*include/parser/.*)
source_group("NSCP API" REGULAR_EXPRESSION .*include/nscapi/.*)
source_group("Filter" REGULAR_EXPRESSION .*include/parsers/.*)
source_group("Socket" REGULAR_EXPRESSION .*include/socket/.*)

set_target_properties(${TARGET} PROPERTIES FOLDER ${MODULE_SUBFOLDER})
nscp_install_module(${TARGET})
