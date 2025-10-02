find_package(Java REQUIRED)

file(COPY_FILE ${CMAKE_SOURCE_DIR}/src/java/Utility.java ${CMAKE_BINARY_DIR}/Utility.java)

set(JAVA_FILE ${CMAKE_BINARY_DIR}/Utility.java)
set(CLASS_FILE ${CMAKE_BINARY_DIR}/Utility.class)
set(CPP_FILE ${CMAKE_SOURCE_DIR}/src/java/utility.cpp)

add_custom_command(
    OUTPUT ${CLASS_FILE}
    COMMAND javac ${JAVA_FILE}
    DEPENDS ${JAVA_FILE}
    COMMENT "Compiling ${JAVA_FILE}"
)

add_custom_command(
    OUTPUT ${CPP_FILE}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/generate_cpp.py ${CLASS_FILE} ${JAVA_FILE} ${CPP_FILE}
    DEPENDS ${CLASS_FILE} ${JAVA_FILE}
    COMMENT "Generating C++ source from ${CLASS_FILE}"
)

add_custom_target(generate_utility_cpp ALL
    DEPENDS ${CPP_FILE}
)
