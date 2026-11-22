if(DEFINED ENABLE_USERAPPS)
    set(USERAPP_TYPES ${ENABLE_USERAPPS} CACHE STRING "List of user apps to build into the firmware")
else ()
    # CyberWatch: Steps, Navigation (for vibration directions later) and the CyberBox placeholder.
    # Removed: Timer, StopWatch, Paint, Paddle, 2048, Dice, Tennis, Calculator, Weather, HeartRate, Metronome, Alarm, Music
    set(DEFAULT_USER_APP_TYPES "Apps::Steps")
    set(DEFAULT_USER_APP_TYPES "${DEFAULT_USER_APP_TYPES}, Apps::Navigation")
    set(DEFAULT_USER_APP_TYPES "${DEFAULT_USER_APP_TYPES}, Apps::CyberBox")
    #set(DEFAULT_USER_APP_TYPES "${DEFAULT_USER_APP_TYPES}, Apps::Motion")
    set(USERAPP_TYPES "${DEFAULT_USER_APP_TYPES}" CACHE STRING "List of user apps to build into the firmware")
endif ()

if(DEFINED ENABLE_WATCHFACES)
    set(WATCHFACE_TYPES ${ENABLE_WATCHFACES} CACHE STRING "List of watch faces to build into the firmware")
else()
    # CyberWatch: the CyberWatch heart screen is the only watch face (Terminal is one swipe away)
    set(DEFAULT_WATCHFACE_TYPES "WatchFace::CyberWatch")
    set(WATCHFACE_TYPES "${DEFAULT_WATCHFACE_TYPES}" CACHE STRING "List of watch faces to build into the firmware")
endif()

add_library(infinitime_apps INTERFACE)
target_sources(infinitime_apps INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/Apps.h")
target_include_directories(infinitime_apps INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/")

# Generate the list of user apps to be compiled into the firmware
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Apps.h.in ${CMAKE_CURRENT_BINARY_DIR}/Apps.h)
