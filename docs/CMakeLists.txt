cmake_minimum_required(VERSION 3.10)

if(MKDOCS_FOUND)
    project(generate_docs)
    # --path-override key=value redirects a path-resolver token directly at the path_manager.
    # (Renamed from --path; the old name collided with `settings --path`.)
    if(WIN32)
        set(MODULE_ARG "module-path=\${base-path}/modules")
        # docs_extract.py imports the generated *_pb2.py, which PythonScript adds to
        # sys.path from ${scripts}/python/lib. Point ${scripts} at the build tree
        # (where protoc generated them) so this works without `make install`.
        set(SCRIPTS_ARG "scripts=\${base-path}/scripts")
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
            --path-override
            "${SCRIPTS_ARG}"
            --
            execute
            --script
            scripts/python/docs_extract.py
            --input
            docs
            --output
            docs
        )
    else()
        set(MODULE_ARG "module-path=${CMAKE_BINARY_DIR}/modules")
        set(LOG_ARG "log-path=.")
        set(DATA_ARGS "data-path=.")
        # docs_extract.py imports the generated *_pb2.py, which PythonScript adds to
        # sys.path from ${scripts}/python/lib. Point ${scripts} at the build tree
        # (where protoc generated them) so this works without `make install`.
        set(SCRIPTS_ARG "scripts=${CMAKE_BINARY_DIR}/scripts")
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
            --path-override
            "\"${SCRIPTS_ARG}\""
            --
            execute
            --script
            scripts/python/docs_extract.py
            --input
            docs
            --output
            docs
        )
    endif()
    # Step 1: extract per-module documentation into git-committed YAML
    # (docs/reference/*.yaml). Needs a running nscp to introspect the loaded
    # modules, so it runs under the PythonScript harness. It only rewrites the
    # slice for the current platform, so run it on both Linux and Windows to keep
    # both slices current. NOT part of `ALL`: invoke explicitly to refresh YAML.
    add_custom_target(
        extract_doc_sources
        COMMAND
            ${COMMAND_LINE}
        WORKING_DIRECTORY ${BUILD_ROOT_FOLDER}
        DEPENDS
            nscp
            ${ALL_MODULE_NAMES}
            copy_files
        COMMENT "Extract documentation into per-module YAML"
    )
    add_dependencies(
        extract_doc_sources
        nscp
        ${ALL_MODULE_NAMES}
        copy_files
        copy_scripts
        copy_resources
    )
    set_target_properties(
        extract_doc_sources
        PROPERTIES
            FOLDER
                "docs"
    )

    # Step 2: render Markdown from the committed YAML. Standalone Python
    # (PyYAML + jinja2 only) -- no nscp needed -- so HTML builds work from the
    # committed YAML without first building nscp.
    add_custom_target(
        generate_doc_sources
        COMMAND
            ${Python3_EXECUTABLE}
            scripts/python/docs_generate.py
            --input
            docs
            --output
            docs
        WORKING_DIRECTORY ${BUILD_ROOT_FOLDER}
        DEPENDS
            copy_files
            copy_scripts
        COMMENT "Generate Markdown documentation from YAML"
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
    endif()
else(MKDOCS_FOUND)
    message(STATUS "mkdocs not found: No documentation will be built")
endif(MKDOCS_FOUND)
