cmake_minimum_required(VERSION 3.10)
project(copy_files)

set(ALL_FILES)
copy_single_file(ALL_FILES license.txt .)
copy_single_file(ALL_FILES nrpe_dh_512.pem security)
copy_single_file(ALL_FILES nrpe_dh_2048.pem security)
copy_single_file(ALL_FILES old-settings.map .)
copy_single_file(ALL_FILES nsclient.dist .)
copy_single_file(ALL_FILES ../changelog .)

if(NOT WIN32)
  configure_file(unix-nsclient.conf.in ${PROJECT_BINARY_DIR}/unix-nsclient.conf
                 @ONLY)
  install(
    FILES ${PROJECT_BINARY_DIR}/unix-nsclient.conf
    DESTINATION ${ETC_TARGET_FOLDER}/nsclient
    RENAME nsclient.ini)
  file(MAKE_DIRECTORY ${LOG_TARGET_FOLDER})
  install(DIRECTORY "nsclient" DESTINATION ${LOG_TARGET_FOLDER})
endif()
if(SYSTEMD)
  install(
    FILES nsclient.service
    DESTINATION ${SYSTEMD_TARGET_FOLDER}
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ) # 775
elseif(INITD)
  if(REDHAT_CLONE)
    install(
      FILES init.centos.sh
      DESTINATION ${ETC_TARGET_FOLDER}/init.d
      RENAME nsclient
      PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE) # 775
  elseif(DEBIAN_CLONE)
    install(
      FILES init.debian.sh
      DESTINATION ${ETC_TARGET_FOLDER}/init.d
      RENAME nsclient
      PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE) # 775
  else()
    message(FATAL "Unknown distribution")
  endif()
endif()

add_custom_target(copy_files ALL DEPENDS ${ALL_FILES})
set_target_properties(copy_files PROPERTIES FOLDER "files")
