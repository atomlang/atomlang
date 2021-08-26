SET(ATOMLANG_SRC atomlang.c)

# Executable
add_executable(atomlang ${ATOMLANG_SRC})
target_link_libraries(atomlang atomlangapi_s)
target_include_directories(atomlang PUBLIC ${ATOMLANG_INCLUDE_DIR})
set_target_properties(atomlang PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS atomlang ${ATOMLANG_TARGETS}
        RUNTIME DESTINATION ${ATOMLANG_INSTALL_RUNTIME_PATH}
        LIBRARY DESTINATION ${ATOMLANG_INSTALL_LIB_PATH}
        ARCHIVE DESTINATION ${ATOMLANG_INSTALL_LIB_STATIC_PATH})
