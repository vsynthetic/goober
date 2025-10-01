#include "java.hpp"
#include "jni.h"
#include <filesystem>
#include <iostream>
#include <string>

java::java() : dumped(false) {
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
}

java::~java() {
    m_jvm->DetachCurrentThread();
}

void java::dump() {
    if (dumped) {
        std::cout << "skipped dumping classes due to pre-existing dump data" << std::endl;
        return;
    }

    jint count;
    static thread_local jclass *loaded_classes;

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

    dumped = true;

    std::cout << "dumped " << count << " classes" << std::endl;
}

jclass java::get_class(std::string name) {
    auto pos = class_map.find(name);
    if (pos == class_map.end()) {
        auto lookup = m_env->FindClass(name.c_str());
        if (lookup) {
            class_map.emplace(name, lookup);

            return lookup;
        } else {
            return nullptr;
        }
    } else {
        return pos->second;
    }
}

java* java::get() {
    static thread_local java instance;
    return &instance;
}

load_status java::load_jar(std::filesystem::path path, std::string agent_class) {
    if (!std::filesystem::exists(path)) {
        return load_status::FILE_NOT_FOUND;
    }

    auto URLClassLoader = get_class("java/net/URLClassLoader");
    auto File = get_class("java/io/File");
    auto URI = get_class("java/net/URI");
    auto URL = get_class("java/net/URL");

    auto File_init = m_env->GetMethodID(File, "<init>", "(Ljava/lang/String;)V");
    auto toURI = m_env->GetMethodID(File, "toURI", "()Ljava/net/URI;");
    auto toURL = m_env->GetMethodID(URI, "toURL", "()Ljava/net/URL;");

    auto URLClassLoader_init = m_env->GetMethodID(URLClassLoader, "<init>", "([Ljava/net/URL;)V");
    auto loadClass = m_env->GetMethodID(URLClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    std::string str = path.string();
    const char* ptr = str.c_str();
    auto java_path = m_env->NewStringUTF(ptr);
    auto file_instance = m_env->NewObject(File, File_init, java_path);

    auto uri_instance = m_env->CallObjectMethod(file_instance, toURI);
    auto url_instance = m_env->CallObjectMethod(uri_instance, toURL);

    auto url_array = m_env->NewObjectArray(1, URL, url_instance);

    auto loader = m_env->NewObject(URLClassLoader, URLClassLoader_init, url_array);

    auto agent_class_str = m_env->NewStringUTF(agent_class.c_str());
    auto agent_class_obj = m_env->CallObjectMethod(loader, loadClass, agent_class_str);

    auto onAgentLoadID = m_env->GetMethodID(reinterpret_cast<jclass>(agent_class_obj), "onAgentLoad", "()V");
    m_env->CallVoidMethod(agent_class_obj, onAgentLoadID);

    m_env->DeleteLocalRef(agent_class_obj);
    m_env->DeleteLocalRef(agent_class_str);
    m_env->DeleteLocalRef(loader);
    m_env->DeleteLocalRef(url_array);
    m_env->DeleteLocalRef(url_instance);
    m_env->DeleteLocalRef(uri_instance);
    m_env->DeleteLocalRef(file_instance);
    m_env->ReleaseStringUTFChars(java_path, ptr);
    m_env->DeleteLocalRef(java_path);

    return load_status::OK;
}
