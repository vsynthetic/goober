#pragma once

#include "jni.h"
#include "jvmti.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

enum class load_status : uint8_t {
    OK = 0,
    EXCEPTION_CAUGHT,
    CLASS_NOT_LOADED
};

std::ostream& operator<<(std::ostream& stream, load_status status);

class java {

    std::unordered_map<std::string, jclass> class_map;
    bool dumped;

    JavaVM* m_jvm;
    JNIEnv* m_env;
    jvmtiEnv* m_ti;

    jvmtiCapabilities caps;
    jvmtiEventCallbacks callbacks;

    java();

    jclass define_class(const char* name, jobject class_loader, jbyte* buffer, jsize size);

    void dump();

public:

    static java* get();
    ~java();

    load_status load_jar(std::filesystem::path path, std::string agent_class);

    void cache(std::string name, jclass clazz);

    jclass get_class(std::string name);

    JavaVM* jvm();
    JNIEnv* env();
    jvmtiEnv* ti();

};
