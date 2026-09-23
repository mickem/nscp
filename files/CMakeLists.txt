cmake_minimum_required(VERSION 3.10)
project(copy_files)

set(ALL_FILES)
copy_single_file(ALL_FILES license.txt .)
copy_single_file(ALL_FILES nrpe_dh_2048.pem security)
copy_single_file(ALL_FILES old-settings.map .)
copy_single_test_file(ALL_FILES nsclient.dist .)

# Ship the third-party notices and the full text of every bundled license, so the
# distributed package reproduces each component's license as its terms require.
# copy_single_file both stages into the build tree (for the WiX/MSI to pick up via
# $(var.Source)) and emits an install() rule (for the CPack ZIP/DEB/RPM).
copy_single_file(ALL_FILES ../THIRD-PARTY-NOTICES.md .)
file(GLOB _license_texts RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSES/*.txt)
foreach(_lic ${_license_texts})
    copy_single_file(ALL_FILES ${_lic} licenses)
endforeach()

if(NOT WIN32)
    configure_file(
        unix-nsclient.conf.in
        ${PROJECT_BINARY_DIR}/unix-nsclient.conf
        @ONLY
    )
    # nsclient.ini holds the web admin password, the NRPE/NSCA passwords and
    # every submit-client token, in plaintext by design (CVE-2025-34078, notice
    # 040): the file *is* the credential store, so the boundary around it is the
    # filesystem. install() with no PERMISSIONS gave it the default 0644, which
    # put all of that in reach of every local account - and those credentials
    # are accepted by the agent's own listeners, which run checks and external
    # scripts. 0640 here, and the post-install scripts give the group to the
    # service account so the daemon can still read its own configuration.
    install(
        FILES
            ${PROJECT_BINARY_DIR}/unix-nsclient.conf
        DESTINATION ${ETC_TARGET_FOLDER}/nsclient
        COMPONENT configs
        PERMISSIONS
            OWNER_READ
            OWNER_WRITE
            GROUP_READ
        RENAME nsclient.ini
    )
    # The directory itself: 0750, so a local account cannot even enumerate what
    # configuration fragments exist. Ownership is set in the post-install
    # scripts - the nsclient group does not exist at package build time.
    install(
        DIRECTORY
        DESTINATION ${ETC_TARGET_FOLDER}/nsclient
        DIRECTORY_PERMISSIONS
            OWNER_READ
            OWNER_WRITE
            OWNER_EXECUTE
            GROUP_READ
            GROUP_EXECUTE
        COMPONENT configs
    )
    # ${LOG_TARGET_FOLDER} is the full log dir (e.g. /var/log/nsclient). Create
    # it empty: a DIRECTORY argument with no source names only the destination.
    #
    # This used to copy the contents of files/nsclient/, whose sole purpose was
    # to carry a placeholder nsclient.log so the directory existed in the staged
    # tree. That placeholder shipped as a root-owned file, and the post-install
    # scripts chown the directory but not its contents - so the service, running
    # unprivileged, could never open its own log file and every log line was
    # dropped. See docs/design/linux-writable-state.md.
    #
    # 0750, not 0755: debug logs echo check arguments and settings paths, which
    # is the same class of disclosure as the configuration file above. The
    # service account owns the directory (set in the post-install scripts) and
    # the log file itself is created 0640 by simple_file_logger.
    install(
        DIRECTORY
        DESTINATION ${LOG_TARGET_FOLDER}
        DIRECTORY_PERMISSIONS
            OWNER_WRITE
            OWNER_READ
            OWNER_EXECUTE
            GROUP_READ
            GROUP_EXECUTE
        COMPONENT logs
    )
    # Template the unit so WorkingDirectory/ExecStart track the chosen install
    # prefix (NSCP_* values) instead of being baked to /usr.
    configure_file(
        nsclient.service.in
        ${PROJECT_BINARY_DIR}/nsclient.service
        @ONLY
    )
    install(
        FILES
            ${PROJECT_BINARY_DIR}/nsclient.service
        DESTINATION ${SYSTEMD_TARGET_FOLDER}
        COMPONENT runtime
    )
endif()

add_custom_target(copy_files ALL DEPENDS ${ALL_FILES})
set_target_properties(
    copy_files
    PROPERTIES
        FOLDER
            "files"
)
