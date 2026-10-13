add_executable(TheGreatTrek WIN32
    main.c
    app.c
    draw.c
    scene.c
    sprites.c
    sound.c
    music.c
    trips.c
    screen_menu.c
    screen_new.c
    screen_game.c
    screen_hunt.c
    screen_barter.c
    screen_overlay.c
    text_check.c
    scores.c)
target_link_libraries(TheGreatTrek PRIVATE trek_core SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
trek_warnings(TheGreatTrek)
add_dependencies(TheGreatTrek trek_resources)
if(MINGW)
    # Fold the MinGW runtime into the exe so it runs without extra DLLs.
    target_link_options(TheGreatTrek PRIVATE -static -static-libgcc)
endif()
if(WIN32)
    # The icon and version for Explorer, the taskbar and shortcuts (assets/icon/, drawn by tools/make_icon.py).
    # Built on its own, without the game's include paths: windres cannot take -I paths with spaces.
    # The icon is copied next to the .rc for the same reason.
    enable_language(RC)
    configure_file("${PROJECT_SOURCE_DIR}/assets/icon/icon.ico" "${CMAKE_CURRENT_BINARY_DIR}/icon.ico" COPYONLY)
    set(TREK_ICON_ICO "${CMAKE_CURRENT_BINARY_DIR}/icon.ico")
    configure_file(TheGreatTrek.rc.in "${CMAKE_CURRENT_BINARY_DIR}/TheGreatTrek.rc" @ONLY)
    add_library(trek_resources_rc OBJECT "${CMAKE_CURRENT_BINARY_DIR}/TheGreatTrek.rc")
    target_sources(TheGreatTrek PRIVATE $<TARGET_OBJECTS:trek_resources_rc>)
endif()
