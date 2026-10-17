# Copies the game's data folders into the iOS app (CMakeLists.txt, SDW_IOS), after every build of SheepD3D, as
#   cmake -DSRC=<the game's data folder> [-DDEST=<the .app>] -P copy_ios_data.cmake
# rsync copies only what changed. Without DEST (the Xcode generator, whose bundle path holds Xcode's own variables,
# such as ${EFFECTIVE_PLATFORM_NAME}) the .app is the one Xcode is building: $TARGET_BUILD_DIR/$WRAPPER_NAME.
if(NOT DEST)
    if(NOT DEFINED ENV{TARGET_BUILD_DIR} OR NOT DEFINED ENV{WRAPPER_NAME})
        message(FATAL_ERROR "copy_ios_data.cmake: no DEST, and not run by Xcode")
    endif()
    set(DEST "$ENV{TARGET_BUILD_DIR}/$ENV{WRAPPER_NAME}")
endif()
# the folders the game reads (the rest of the disc - the Windows executables and DLLs - is left out)
foreach(folder Bonus Install Levels Musics References Voices retexture retexture-psx)
    if(EXISTS "${SRC}/${folder}")
        execute_process(COMMAND rsync -a --delete "${SRC}/${folder}" "${DEST}/" RESULT_VARIABLE result)
        if(result)
            message(FATAL_ERROR "copy_ios_data.cmake: copying ${SRC}/${folder} into ${DEST} failed")
        endif()
    endif()
endforeach()
