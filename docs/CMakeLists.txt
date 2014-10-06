cmake_minimum_required(VERSION 2.6)

PROJECT(generate_docs)

ADD_CUSTOM_TARGET(generate_doc_sources
	COMMAND ${CMAKE_BINARY_DIR}/nscp client --module PythonScript --no-stderr --settings dummy --load-all --log error --log oneline -- --script scripts/python/docs.py --input docs --format rst --output ext/docs
	WORKING_DIRECTORY ${BUILD_ROOT_FOLDER}
		DEPENDS nscp ${ALL_MODULE_NAMES} copy_files
	COMMENT "Generate RST documentation source")
ADD_DEPENDENCIES(generate_doc_sources nscp ${ALL_MODULE_NAMES})
	SET_TARGET_PROPERTIES(generate_doc_sources PROPERTIES FOLDER "docs")
