# Headless tools: they link only the core, never SDL.
add_executable(datacheck datacheck.c)
target_link_libraries(datacheck PRIVATE trek_core)
trek_warnings(datacheck)

add_executable(trekbot trekbot.c)
target_link_libraries(trekbot PRIVATE trek_core)
trek_warnings(trekbot)
