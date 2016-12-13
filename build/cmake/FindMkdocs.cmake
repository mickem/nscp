find_program(MKDOCS_EXECUTABLE 
  NAMES mkdocs mkdocs.exe
  HINTS
  $ENV{MKDOCS_DIR}
  ${MKDOCS_DIR}
  ${PYTHON_EXECUTABLE}../../scripts
  
  PATH_SUFFIXES bin
  DOC "Mkdocs documentation generator"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Mkdocs DEFAULT_MSG\r\n  MKDOCS_EXECUTABLE)

mark_as_advanced(
  MKDOCS_EXECUTABLE
)
IF(MKDOCS_EXECUTABLE)
	SET(MKDOCS_FOUND TRUE)
ENDIF(MKDOCS_EXECUTABLE)
