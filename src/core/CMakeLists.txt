# The simulation core: pure C17, no SDL. Linked by the game, the tools and the tests.
add_library(trek_core STATIC
    rng.c
    util.c
    locale.c
    calendar.c
    data.c
    format.c
    game.c
    game_travel.c
    game_events.c
    game_places.c
    game_store.c
    game_save.c
    game_hunt.c)
target_include_directories(trek_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(trek_core PUBLIC cjson)
if(NOT MSVC)
    target_link_libraries(trek_core PUBLIC m)
endif()
trek_warnings(trek_core)
