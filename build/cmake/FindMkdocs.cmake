find_program(
    MKDOCS_EXECUTABLE
    NAMES mkdocs mkdocs.exe
    HINTS
        $ENV{MKDOCS_DIR}
        ${MKDOCS_DIR}
        ${Python3_EXECUTABLE}../../scripts
        ${Python3_EXECUTABLE}/../
    PATH_SUFFIXES bin
    DOC "Mkdocs documentation generator"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Mkdocs "MKDocs not found" MKDOCS_EXECUTABLE)

mark_as_advanced(MKDOCS_EXECUTABLE)
if(MKDOCS_EXECUTABLE)
    set(MKDOCS_FOUND TRUE)
endif(MKDOCS_EXECUTABLE)
