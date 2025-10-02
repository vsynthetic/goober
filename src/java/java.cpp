#include "java.hpp"
#include "jni.h"
#include "jvmti.h"
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>

java::java() : dumped(false) {
    caps = {};

    if (JNI_GetCreatedJavaVMs(&m_jvm, 1, nullptr) != JNI_OK) {
        std::cerr << "Failed to get created Java VMs." << std::endl;
        exit(1);
    }

    if (m_jvm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&m_env), nullptr) != JNI_OK) {
        std::cerr << "Failed to attach to thread." << std::endl;
        exit(1);
    }

    if (m_jvm->GetEnv(reinterpret_cast<void**>(&m_ti), JVMTI_VERSION_1_2) != JNI_OK) {
        std::cerr << "Failed to get tooling interface." << std::endl;
        exit(1);
    }

    dump();

    caps.can_retransform_any_class = 1;
    caps.can_retransform_classes = 1;
    caps.can_redefine_any_class = 1;
    caps.can_redefine_classes = 1;

    m_ti->AddCapabilities(&caps);
}

java::~java() {
    if (m_ti)
        m_ti->RelinquishCapabilities(&caps);

    if (m_jvm)
        m_jvm->DetachCurrentThread();
}

void java::dump() {
    jint count;
    static thread_local jclass *loaded_classes = nullptr;

    if (loaded_classes != nullptr) {
        m_ti->Deallocate(reinterpret_cast<unsigned char *>(loaded_classes));
        loaded_classes = nullptr;
    }

    // memory leaks but i don't care enough atm
    if (m_ti->GetLoadedClasses(&count, &loaded_classes) != JVMTI_ERROR_NONE) {
        std::cerr << "Failed to dump loaded java classes." << std::endl;
        return;
    }

    const jclass klass = m_env->FindClass("java/lang/Class");
	const jmethodID getName = m_env->GetMethodID(klass, "getName", "()Ljava/lang/String;");

    for (int i = 0; i < count; i++) {
        const auto name = static_cast<jstring>(m_env->CallObjectMethod(loaded_classes[i], getName));
		const char* className = m_env->GetStringUTFChars(name, nullptr);

		class_map.emplace(std::string(className), loaded_classes[i]);

		m_env->ReleaseStringUTFChars(name, className);
    }
}

void java::cache(std::string name, jclass clazz) {
    if (!class_map.contains(name))
        class_map.emplace(name, clazz);
}

jclass java::get_class(std::string name) {
    auto pos = class_map.find(name);
    if (pos == class_map.end()) {
        dump();

        auto pos2 = class_map.find(name);
        if (pos2 == class_map.end()) {
            return nullptr;
        } else {
            return pos2->second;
        }
    } else {
        return pos->second;
    }
}

java* java::get() {
    static thread_local java instance;
    return &instance;
}

#include "utility.cpp"

load_status java::load_jar(std::filesystem::path path, std::string agent_class) {
    auto class_loader = get_class("java.lang.ClassLoader");
    auto get_system_loader = m_env->GetStaticMethodID(class_loader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    auto system_loader = m_env->CallStaticObjectMethod(class_loader, get_system_loader);

    auto clazz = m_env->DefineClass(
        embedded::cat_psychward_goober_Utility_name,
        system_loader,
        reinterpret_cast<jbyte *>(embedded::cat_psychward_goober_Utility),
        embedded::cat_psychward_goober_Utility_size
    );

    auto load_agent = m_env->GetStaticMethodID(clazz, "loadAgent", "(Ljava/lang/String;Ljava/lang/String;)V");
    auto utility = m_env->NewObject(clazz, m_env->GetMethodID(get_class("java.lang.Object"), "<init>", "()V"));
    auto path_str = path.string();

    auto path_j_str = m_env->NewStringUTF(path_str.c_str());
    auto agent_class_j_str = m_env->NewStringUTF(agent_class.c_str());

    m_env->CallVoidMethod(utility, load_agent, path_j_str, agent_class_j_str);

    if (m_env->ExceptionCheck()) {
        m_env->ExceptionDescribe();
    }

    return load_status::OK;
}

std::ostream& operator<<(std::ostream& stream, load_status status) {

    switch (status) {
    case load_status::OK:
        stream << "OK";
        break;
    case load_status::FILE_NOT_FOUND:
        stream << "File not found";
        break;
    case load_status::CLASS_NOT_FOUND:
        stream << "Class not found";
        break;
    case load_status::METHOD_NOT_FOUND:
        stream << "Method not found";
        break;
    case load_status::AGENT_LOAD_NOT_FOUND:
        stream << "onAgentLoad() not found";
        break;
    }

    return stream;
}
