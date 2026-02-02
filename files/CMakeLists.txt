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
        FILES ${PROJECT_BINARY_DIR}/unix-nsclient.conf
        DESTINATION ${ETC_TARGET_FOLDER}/nsclient
        COMPONENT configs
        RENAME nsclient.ini
    )
    file(MAKE_DIRECTORY ${LOG_TARGET_FOLDER})
    install(DIRECTORY "nsclient" DESTINATION ${LOG_TARGET_FOLDER}
            DIRECTORY_PERMISSIONS
            OWNER_WRITE OWNER_READ OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
            COMPONENT logs
    )
    install(FILES nsclient.service
            DESTINATION lib/systemd/system
            COMPONENT runtime
    )
endif()

add_custom_target(copy_files ALL DEPENDS ${ALL_FILES})
set_target_properties(copy_files PROPERTIES FOLDER "files")
