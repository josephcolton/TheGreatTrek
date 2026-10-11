foreach(name rng locale game)
    add_executable(test_${name} test_${name}.c)
    target_link_libraries(test_${name} PRIVATE trek_core)
    target_include_directories(test_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_${name} PRIVATE TREK_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
    trek_warnings(test_${name})
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

# Data files must validate, and the bot must finish games with saves round-tripping.
add_test(NAME datacheck COMMAND datacheck ${CMAKE_SOURCE_DIR}/data)
add_test(NAME trekbot COMMAND trekbot --runs 10 --savecheck --data ${CMAKE_SOURCE_DIR}/data)
