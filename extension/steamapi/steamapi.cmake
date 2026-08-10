
if(NOT EXISTS "${STEAMAPI_DIR}/sdk")
    
    set(STEAMWORKS_SDK "https://partner.steamgames.com/downloads/steamworks_sdk_165.zip")
    set(STEAMWORKS_SDK_ZIP_FILE  "${SOURCE_ROOT}/temp.zip")

    message(STATUS "Steamworks SDK not found. Downloading...")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    file(DOWNLOAD 
        ${STEAMWORKS_SDK}
        ${STEAMWORKS_SDK_ZIP_FILE}
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY OFF
    )
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
        message(FATAL_ERROR "Failed to download Steamworks SDK: ${ERROR_MESSAGE}")
    endif()
    
    message(STATUS "Extracting Steamworks SDK to ${STEAMAPI_DIR}...")
    file(ARCHIVE_EXTRACT INPUT "${STEAMWORKS_SDK_ZIP_FILE}" DESTINATION "${STEAMAPI_DIR}" )
    
    message(STATUS "Cleaning up temporary zip file...")
    file(REMOVE "${STEAMWORKS_SDK_ZIP_FILE}")

    message(STATUS "Steamworks SDK successfully installed.")
else()
    message(STATUS "Steamworks SDK already exists at ${STEAMAPI_DIR}")
endif()

if(EXISTS "${STEAMAPI_DIR}/sdk") 
    set(STEAM_SDK_DIR "${STEAMAPI_DIR}/sdk")
    add_library(steam_api SHARED IMPORTED)
    set_target_properties(steam_api PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${STEAM_SDK_DIR}/public/steam/"
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(STEAM_API_LIB "${STEAM_SDK_DIR}/redistributable_bin/win64/steam_api64.lib")
        set(STEAM_API_DLL "${STEAM_SDK_DIR}/redistributable_bin/win64/steam_api64.dll")

    else()
        set(STEAM_API_LIB "${STEAM_SDK_DIR}/redistributable_bin/steam_api.lib")
        set(STEAM_API_DLL "${STEAM_SDK_DIR}/redistributable_bin/steam_api.dll")
    endif()

    set_target_properties(steam_api PROPERTIES
        IMPORTED_IMPLIB   ${STEAM_API_LIB}
        IMPORTED_LOCATION ${STEAM_API_DLL}
    )

    macro(TARGET_COPY_STEAM_API_DLL TARGT_NAME TARGET_DIR)
        add_custom_command(TARGET ${TARGT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${STEAM_API_DLL} ${TARGET_DIR}
        )
    endmacro()
endif()