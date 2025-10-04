find_package(Java REQUIRED)

set(JAVA_SOURCE ${CMAKE_SOURCE_DIR}/javasrc)
set(CPP_FILE ${CMAKE_SOURCE_DIR}/src/java/embedded.cpp)
set(JAR_FILE ${CMAKE_BINARY_DIR}/goober-stdlib.jar)

add_custom_command(
    OUTPUT ${CPP_FILE}
    OUTPUT ${JAR_FILE}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/java_tool.py --source ${JAVA_SOURCE} --jarpath ${JAR_FILE} -o build/java --output ${CPP_FILE}
    COMMENT "Generating C++ source from ${CLASS_FILE}"
)

add_custom_target(generate_utility_cpp ALL
    DEPENDS ${CPP_FILE}
)
