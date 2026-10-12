add_executable(TheGreatTrek WIN32
    main.c
    app.c
    draw.c
    scene.c
    sprites.c
    sound.c
    trips.c
    screen_menu.c
    screen_new.c
    screen_game.c
    screen_hunt.c
    screen_barter.c
    screen_overlay.c
    text_check.c)
target_link_libraries(TheGreatTrek PRIVATE trek_core SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
trek_warnings(TheGreatTrek)
add_dependencies(TheGreatTrek trek_resources)
if(MINGW)
    # Fold the MinGW runtime into the exe so it runs without extra DLLs.
    target_link_options(TheGreatTrek PRIVATE -static -static-libgcc)
endif()
