#pragma once

#include "jni.h"
#include "jvmti.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

enum class load_status : uint8_t {
    OK = 0,
    FILE_NOT_FOUND,
    CLASS_NOT_FOUND,
    METHOD_NOT_FOUND,
    AGENT_LOAD_NOT_FOUND
};

std::ostream& operator<<(std::ostream& stream, load_status status);

class java {

    std::unordered_map<std::string, jclass> class_map;
    bool dumped;

    JavaVM *m_jvm;
    JNIEnv *m_env;
    jvmtiEnv *m_ti;

    jvmtiCapabilities caps;

    java();

    void dump();

public:

    static java* get();
    ~java();

    load_status load_jar(std::filesystem::path path, std::string agent_class);

    void cache(std::string name, jclass clazz);

    jclass get_class(std::string name);

};
