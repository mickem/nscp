cmake_minimum_required(VERSION 3.10)

if(MKDOCS_FOUND)
    project(generate_docs)
    # --path-override key=value redirects a path-resolver token directly at the path_manager.
    # (Renamed from --path; the old name collided with `settings --path`.)
    if(WIN32)
        set(MODULE_ARG "module-path=\${base-path}/modules")
        set(COMMAND_LINE
            ${CMAKE_BINARY_DIR}/nscp
            client
            --module
            PythonScript
            --settings
            dummy
            --load-all
            --log
            info
            --log
            oneline
            --path-override
            "${MODULE_ARG}"
            --
            execute
            --script
            scripts/python/docs.py
            --input
            docs
            --output
            docs
        )
    else()
        set(MODULE_ARG "module-path=${CMAKE_BINARY_DIR}/modules")
        set(LOG_ARG "log-path=.")
        set(DATA_ARGS "data-path=.")
        set(COMMAND_LINE
            ${CMAKE_BINARY_DIR}/nscp
            client
            --module
            PythonScript
            --settings
            dummy
            --load-all
            --log
            info
            --log
            oneline
            --path-override
            "\"${MODULE_ARG}\""
            --path-override
            "\"${LOG_ARG}\""
            --path-override
            "\"${DATA_ARGS}\""
            --
            execute
            --script
            scripts/python/docs.py
            --input
            docs
            --output
            docs
        )
    endif()
    add_custom_target(
        generate_doc_sources
        COMMAND
            ${COMMAND_LINE}
        WORKING_DIRECTORY ${BUILD_ROOT_FOLDER}
        DEPENDS
            nscp
            ${ALL_MODULE_NAMES}
            copy_files
        COMMENT "Generate RST documentation source"
    )
    add_dependencies(
        generate_doc_sources
        nscp
        ${ALL_MODULE_NAMES}
        copy_files
        copy_scripts
        copy_resources
    )
    set_target_properties(
        generate_doc_sources
        PROPERTIES
            FOLDER
                "docs"
    )

    add_custom_target(
        build_docs_html
        ALL
        ${MKDOCS_EXECUTABLE} build --site-dir "${CMAKE_CURRENT_BINARY_DIR}/html"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building HTML documentation"
    )
    add_dependencies(build_docs_html generate_doc_sources)

    set_target_properties(
        build_docs_html
        PROPERTIES
            FOLDER
                "docs"
    )
    if(WIN32)
        install(
            DIRECTORY
                "${CMAKE_CURRENT_BINARY_DIR}/html/"
            DESTINATION "${INSTALL_FILES_BASE}web/help"
        )
    else()
        # docs.py also emits roff man pages (man1/man5) alongside the markdown,
        # into the source tree under docs/man. Install them into the standard
        # man hierarchy. generate_doc_sources (a dependency of the default
        # build_docs_html target) produces them before install runs.
        install(
            DIRECTORY
                "${CMAKE_CURRENT_SOURCE_DIR}/man/"
            DESTINATION "share/man"
            FILES_MATCHING
                PATTERN "*.1"
                PATTERN "*.5"
        )
    endif()
else(MKDOCS_FOUND)
    message(STATUS "mkdocs not found: No documentation will be built")
endif(MKDOCS_FOUND)
