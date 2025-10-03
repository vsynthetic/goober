#include "java.hpp"
#include "jni.h"
#include "jvmti.h"
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include "../lib/lib.hpp"


JNIEXPORT jint JNICALL redefine_class_c(JNIEnv* env, jclass owner, jclass j_class, jbyteArray new_bytes) {
    auto length = env->GetArrayLength(new_bytes);
    std::vector<jbyte> buffer(length);
    env->GetByteArrayRegion(new_bytes, 0, length, buffer.data());

    auto definition = jvmtiClassDefinition {
        .klass = j_class,
        .class_byte_count = length,
        .class_bytes = reinterpret_cast<const unsigned char*>(buffer.data())
    };

    return java::get()->ti()->RedefineClasses(1, &definition);
}

JNIEXPORT jint JNICALL redefine_class_s(JNIEnv* env, jclass owner, jstring j_class_name, jbyteArray new_bytes) {
    auto jvm = java::get();

    const char* class_name = env->GetStringUTFChars(j_class_name, nullptr);
    auto value = redefine_class_c(env, owner, jvm->get_class(class_name), new_bytes);
    env->ReleaseStringUTFChars(j_class_name, class_name);

    return value;
}

java::java() : dumped(false), caps({}), callbacks({}) {
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

    callbacks.VMDeath = [](jvmtiEnv *jvmti_env, JNIEnv* jni_env) {
        lib::get()->uninit();
    };

    m_ti->SetEventCallbacks(&callbacks, sizeof(jvmtiEventCallbacks));
    m_ti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
}

java::~java() {
    if (m_ti) {
        m_ti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_VM_DEATH, nullptr);
        m_ti->RelinquishCapabilities(&caps);
    }

    if (m_jvm)
        m_jvm->DetachCurrentThread();
}

JavaVM* java::jvm() {
    return m_jvm;
}

JNIEnv* java::env() {
    return m_env;
}

jvmtiEnv* java::ti() {
    return m_ti;
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

    // TODO: this should probably be more than one class so we can build a sort of stdlib
    // which will then also be packaged into a .jar file for use as a library
    auto clazz = m_env->DefineClass(
        embedded::cat_psychward_goober_Utility_name,
        system_loader,
        reinterpret_cast<jbyte *>(embedded::cat_psychward_goober_Utility),
        embedded::cat_psychward_goober_Utility_size
    );

    auto load_agent = m_env->GetStaticMethodID(clazz, "loadAgent", "(Ljava/lang/String;Ljava/lang/String;)V");
    auto utility = m_env->NewObject(clazz, m_env->GetMethodID(get_class("java.lang.Object"), "<init>", "()V"));
    auto path_str = path.string();

    const JNINativeMethod methods[] = {
        { const_cast<char*>("redefineClass"), const_cast<char*>("(Ljava/lang/String;[B)I"), reinterpret_cast<void*>(&redefine_class_s) },
        { const_cast<char*>("redefineClass"), const_cast<char*>("(Ljava/lang/Class;[B)I"), reinterpret_cast<void*>(&redefine_class_c) },
    };
    m_env->RegisterNatives(clazz, reinterpret_cast<const JNINativeMethod *>(&methods), 2);

    auto path_j_str = m_env->NewStringUTF(path_str.c_str());
    auto agent_class_j_str = m_env->NewStringUTF(agent_class.c_str());

    m_env->CallVoidMethod(utility, load_agent, path_j_str, agent_class_j_str);

    if (m_env->ExceptionCheck()) {
        m_env->ExceptionDescribe();
        return load_status::EXCEPTION_CAUGHT;
    }

    return load_status::OK;
}

std::ostream& operator<<(std::ostream& stream, load_status status) {

    switch (status) {
    case load_status::OK:
        stream << "OK";
        break;
    case load_status::EXCEPTION_CAUGHT:
        stream << "Exception caught";
        break;
    }

    return stream;
}
