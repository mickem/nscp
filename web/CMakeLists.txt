cmake_minimum_required(VERSION 3.10)

project(copy_web)

# Web bundle ships out-of-band as of the web-bundle-installer rollout
# (docs/design/web-bundle-installer.md, Phase 3). On Linux the .deb/.rpm
# no longer carry the web frontend; operators run `nscp web install-ui`
# at runtime to fetch the matching NSCP-Web-${VERSION}.zip from the
# release page. On Windows the MSI still bundles `dist/` directly.
#
# This file therefore needs to do *something* iff `dist/` was populated
# by a developer running `npm run build` (or by CI staging the bundle
# before CPack), and nothing otherwise. The copy_resources target is
# always defined so the rest of the build graph doesn't have to guard.

file(GLOB_RECURSE resources_HTML "dist/*.html")
file(GLOB_RECURSE resources_JS "dist/*.js")
file(GLOB_RECURSE resources_PNG "dist/*.png")
set(resources_ALL
    ${resources_HTML}
    ${resources_JS}
    ${resources_PNG}
)
set(ALL_RESOURCES)

if(NOT resources_ALL)
    message(
        STATUS
        "web/dist is empty - the web bundle will not be packaged. "
        "Run `npm install && npm run build` in web/ before configuring "
        "CMake to bundle the UI into the package; otherwise install it "
        "at runtime with `nscp web install-ui`."
    )
else()
    foreach(file ${resources_ALL})
        get_filename_component(filename ${file} NAME)
        get_filename_component(filepath ${file} PATH)
        file(
            RELATIVE_PATH
            relpath
            "${CMAKE_CURRENT_SOURCE_DIR}/dist"
            "${filepath}"
        )
        string(
            REPLACE "/"
            "_"
            alias
            "${relpath}"
        )
        string(
            REPLACE "."
            "_"
            alias
            "${alias}"
        )
        copy_single_file(ALL_RESOURCES dist/${relpath}/${filename} web/${relpath})
    endforeach(
        file
        ${resources_ALL}
    )
endif()

add_custom_target(copy_resources ALL DEPENDS ${ALL_RESOURCES})
set_target_properties(
    copy_resources
    PROPERTIES
        FOLDER
            "files"
)
