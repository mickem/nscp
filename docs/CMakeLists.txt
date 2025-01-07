cmake_minimum_required(VERSION 3.5)

if(MKDOCS_FOUND)
  project(generate_docs)
  set(DEFINEARG "/paths:module-path=%(base-path)/modules")
  add_custom_target(
    generate_doc_sources
    COMMAND
      ${CMAKE_BINARY_DIR}/nscp client --module PythonScript --settings dummy
      --load-all --log info --log oneline --define "\"${DEFINEARG}\"" -- execute
      --script scripts/python/docs.py --input docs --output docs
    WORKING_DIRECTORY ${BUILD_ROOT_FOLDER}
    DEPENDS nscp ${ALL_MODULE_NAMES} copy_files
    COMMENT "Generate RST documentation source")
  add_dependencies(generate_doc_sources nscp ${ALL_MODULE_NAMES} copy_files
                   copy_scripts copy_resources)
  set_target_properties(generate_doc_sources PROPERTIES FOLDER "docs")

  add_custom_target(
    build_docs_html ALL
    ${MKDOCS_EXECUTABLE} build --site-dir "${CMAKE_CURRENT_BINARY_DIR}/html"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building HTML documentation")
  add_dependencies(build_docs_html generate_doc_sources)

  set_target_properties(build_docs_html PROPERTIES FOLDER "docs")
  install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/"
          DESTINATION "${INSTALL_FILES_BASE}web/help")
else(MKDOCS_FOUND)
  message(STATUS "mkdocs not found: No documentation will be built")
endif(MKDOCS_FOUND)
