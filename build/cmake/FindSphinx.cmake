find_program(SPHINX_EXECUTABLE 
  NAMES sphinx-build sphinx-build.exe
  HINTS
  $ENV{SPHINX_DIR}
  ${SPHINX_DIR}
  ${PYTHON_EXECUTABLE}../../scripts
  
  PATH_SUFFIXES bin
  DOC "Sphinx documentation generator"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Sphinx DEFAULT_MSG\r\n  SPHINX_EXECUTABLE)

mark_as_advanced(
  SPHINX_EXECUTABLE
)
IF(SPHINX_EXECUTABLE)
	SET(SPHINX_FOUND TRUE)
ENDIF(SPHINX_EXECUTABLE)
