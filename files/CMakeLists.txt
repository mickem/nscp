cmake_minimum_required(VERSION 3.10)
project(copy_files)

set(ALL_FILES)
copy_single_file(ALL_FILES license.txt .)
copy_single_file(ALL_FILES nrpe_dh_512.pem security)
copy_single_file(ALL_FILES nrpe_dh_2048.pem security)
copy_single_file(ALL_FILES old-settings.map .)
copy_single_test_file(ALL_FILES nsclient.dist .)

if(NOT WIN32)
    configure_file(
        unix-nsclient.conf.in
        ${PROJECT_BINARY_DIR}/unix-nsclient.conf
        @ONLY
    )
    install(
        FILES
            ${PROJECT_BINARY_DIR}/unix-nsclient.conf
        DESTINATION ${ETC_TARGET_FOLDER}/nsclient
        COMPONENT configs
        RENAME nsclient.ini
    )
    # ${LOG_TARGET_FOLDER} is the full log dir (e.g. /var/log/nsclient). The
    # trailing slash on the source copies the *contents* of files/nsclient/
    # into it, so the staged tree keeps <logdir>/nsclient.log.
    install(
        DIRECTORY
            "nsclient/"
        DESTINATION ${LOG_TARGET_FOLDER}
        DIRECTORY_PERMISSIONS
            OWNER_WRITE
            OWNER_READ
            OWNER_EXECUTE
            GROUP_READ
            GROUP_EXECUTE
            WORLD_READ
            WORLD_EXECUTE
        COMPONENT logs
    )
    # Template the unit so WorkingDirectory/PIDFile/ExecStart track the chosen
    # install prefix (NSCP_* values) instead of being baked to /usr.
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
