#include "java.hpp"
#include "jni.h"
#include "jvmti.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include "../lib/lib.hpp"

#include "embedded.cpp"


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

JNIEXPORT jint JNICALL retransform_class_c(JNIEnv* env, jclass owner, jclass j_class) {
    return java::get()->ti()->RetransformClasses(1, &j_class);
}

JNIEXPORT jint JNICALL retransform_class_s(JNIEnv* env, jclass owner, jstring j_class_name) {
    auto jvm = java::get();

    const char* class_name = env->GetStringUTFChars(j_class_name, nullptr);
    std::cout << "attempting to retransform " << class_name << std::endl;
    auto value = retransform_class_c(env, owner, jvm->get_class(class_name));
    env->ReleaseStringUTFChars(j_class_name, class_name);

    return value;
}

JNIEXPORT void JNICALL on_shutdown(JNIEnv* env, jclass owner) {
    lib::get()->uninit();
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

    auto class_loader = get_class("java.lang.ClassLoader");
    auto get_system_loader = m_env->GetStaticMethodID(class_loader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    auto system_loader = m_env->CallStaticObjectMethod(class_loader, get_system_loader);

    // TODO: will have to implement a sort of dependency-system so i can load the important classes first
    for (int i = 1; i < embedded::classes.size(); i++) {
        auto info = embedded::classes[i];

        auto clazz = define_class(
            info.name,
            system_loader,
            reinterpret_cast<jbyte *>(info.bytes.data()),
            info.bytes.size()
        );

        if (clazz == 0) {
            std::cerr << "Failed to define class " << info.name << std::endl;

            if (m_env->ExceptionCheck()) {
                m_env->ExceptionDescribe();
            }
        }
    }

    auto utility_info = embedded::classes[0];

    auto clazz = define_class(
        utility_info.name,
        system_loader,
        reinterpret_cast<jbyte *>(utility_info.bytes.data()),
        utility_info.bytes.size()
    );

    if (clazz == 0) {
        std::cerr << "Failed to define class " << utility_info.name << std::endl;
    }

    const JNINativeMethod methods[] = {
        { const_cast<char*>("redefineClass"), const_cast<char*>("(Ljava/lang/String;[B)I"), reinterpret_cast<void*>(&redefine_class_s) },
        { const_cast<char*>("redefineClass"), const_cast<char*>("(Ljava/lang/Class;[B)I"), reinterpret_cast<void*>(&redefine_class_c) },
        { const_cast<char*>("retransformClass"), const_cast<char*>("(Ljava/lang/String;)I"), reinterpret_cast<void*>(&retransform_class_s) },
        { const_cast<char*>("retransformClass"), const_cast<char*>("(Ljava/lang/Class;)I"), reinterpret_cast<void*>(&retransform_class_c) },
    };
    m_env->RegisterNatives(clazz, reinterpret_cast<const JNINativeMethod *>(&methods), 4);

    caps.can_retransform_any_class = 1;
    caps.can_retransform_classes = 1;
    caps.can_redefine_any_class = 1;
    caps.can_redefine_classes = 1;

    m_ti->AddCapabilities(&caps);

    callbacks.VMDeath = [](jvmtiEnv *jvmti_env, JNIEnv* jni_env) {
        lib::get()->uninit();
    };

    callbacks.ClassFileLoadHook = [](jvmtiEnv *jvmti_env, JNIEnv *jni_env, jclass class_being_redefined, jobject loader, const char *name, jobject protection_domain, jint class_data_len, const unsigned char *class_data, jint *new_class_data_len, unsigned char **new_class_data) {
        static jclass Utility;
        auto jvm = get();

        if (Utility == nullptr)
            Utility = jvm->get_class("cat.psychward.goober.Utility");

        if (Utility != nullptr) {
            auto data_array = jni_env->NewByteArray(class_data_len);
            jni_env->SetByteArrayRegion(data_array, 0, class_data_len, reinterpret_cast<const jbyte*>(class_data));
            jstring j_name = jni_env->NewStringUTF(name);

            static auto ClassLoadListener = jvm->get_class("cat.psychward.goober.ClassLoadListener");
            static auto onLoadMethod = jni_env->GetMethodID(ClassLoadListener, "onLoad", "(Ljava/lang/String;[B)[B");

            static auto List = jvm->get_class("java.util.List");
            static auto Iterator = jvm->get_class("java.util.Iterator");

            static auto listenersField = jni_env->GetStaticFieldID(Utility, "loadListeners", "Ljava/util/List;");
            static auto iteratorMethod = jni_env->GetMethodID(List, "iterator", "()Ljava/util/Iterator;");
            static auto hasNextMethod = jni_env->GetMethodID(Iterator, "hasNext", "()Z");
            static auto nextMethod = jni_env->GetMethodID(Iterator, "next", "()Ljava/lang/Object;");

            auto listeners = jni_env->GetStaticObjectField(Utility, listenersField);
            auto iter = jni_env->CallObjectMethod(listeners, iteratorMethod);

            while (jni_env->CallBooleanMethod(iter, hasNextMethod)) {
                auto listener = jni_env->CallObjectMethod(iter, nextMethod);

                auto value = jni_env->CallObjectMethod(listener, onLoadMethod, j_name, data_array);

                jni_env->DeleteLocalRef(listener);

                if (value != nullptr) {
                    jbyteArray outArray = static_cast<jbyteArray>(value);
                    jsize outLen = jni_env->GetArrayLength(outArray);

                    jbyte* outBytes = jni_env->GetByteArrayElements(outArray, nullptr);

                    unsigned char* buf;
                    jvmti_env->Allocate(outLen, &buf);
                    memcpy(buf, outBytes, outLen);

                    *new_class_data_len = outLen;
                    *new_class_data = buf;

                    jni_env->ReleaseByteArrayElements(outArray, outBytes, JNI_ABORT);
                    break;
                }

                jni_env->DeleteLocalRef(value);
            }

            jni_env->DeleteLocalRef(j_name);
            jni_env->DeleteLocalRef(data_array);
            jni_env->DeleteLocalRef(iter);
            jni_env->DeleteLocalRef(listeners);

        }
    };

    m_ti->SetEventCallbacks(&callbacks, sizeof(jvmtiEventCallbacks));
    m_ti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    m_ti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
}

java::~java() {
    if (m_ti) {
        m_ti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
        m_ti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_VM_DEATH, nullptr);
        m_ti->RelinquishCapabilities(&caps);
    }
}

jclass java::define_class(const char* name, jobject class_loader, jbyte* buffer, jsize size) {
    auto clazz = m_env->DefineClass(name, class_loader, buffer, size);

    if (clazz != nullptr) {
        static jclass klass = m_env->FindClass("java/lang/Class");
        static jmethodID getName = m_env->GetMethodID(klass, "getName", "()Ljava/lang/String;");

        auto name = static_cast<jstring>(m_env->CallObjectMethod(clazz, getName));
		const char* className = m_env->GetStringUTFChars(name, nullptr);

		class_map.emplace(std::string(className), clazz);

		m_env->ReleaseStringUTFChars(name, className);
    }

    return clazz;
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
    // static thread_local jclass *loaded_classes = nullptr;
    jclass *loaded_classes = nullptr;

    // if (loaded_classes != nullptr) {
    //     m_ti->Deallocate(reinterpret_cast<unsigned char *>(loaded_classes));
    //     loaded_classes = nullptr;
    // }

    // memory leaks but i don't care enough atm
    if (m_ti->GetLoadedClasses(&count, &loaded_classes) != JVMTI_ERROR_NONE) {
        std::cerr << "Failed to dump loaded java classes." << std::endl;
        return;
    }

    const jclass klass = m_env->FindClass("java/lang/Class");
	const jmethodID getName = m_env->GetMethodID(klass, "getName", "()Ljava/lang/String;");

	if (getName == nullptr) {
	    return;
	}

    for (int i = 0; i < count; i++) {
        auto ref = reinterpret_cast<jclass>(m_env->NewGlobalRef(loaded_classes[i]));

        const auto name = static_cast<jstring>(m_env->CallObjectMethod(ref, getName));
		const char* className = m_env->GetStringUTFChars(name, nullptr);

		class_map.emplace(std::string(className), ref);

		m_env->ReleaseStringUTFChars(name, className);
    }

    m_ti->Deallocate(reinterpret_cast<unsigned char*>(loaded_classes));
}

void java::cache(std::string name, jclass clazz) {
    if (!class_map.contains(name))
        class_map.emplace(name, clazz);
}

jclass java::get_class(std::string name) {
    auto pos = class_map.find(name);
    if (pos == class_map.end()) {
        // dump();

        // auto pos2 = class_map.find(name);
        // if (pos2 == class_map.end()) {
        //     return nullptr;
        // } else {
        //     return pos2->second;
        // }
        return nullptr;
    } else {
        return pos->second;
    }
}

java* java::get() {
    // no more thread_local i guess gg
    static java instance;
    return &instance;
}

load_status java::load_jar(std::filesystem::path path, std::string agent_class) {
    auto clazz = get_class("cat.psychward.goober.Utility");

    if (clazz == 0) {
        return load_status::CLASS_NOT_LOADED;
    }

    auto load_agent = m_env->GetStaticMethodID(clazz, "loadAgent", "(Ljava/lang/String;Ljava/lang/String;)V");
    auto path_str = path.string();

    auto path_j_str = m_env->NewStringUTF(path_str.c_str());
    auto agent_class_j_str = m_env->NewStringUTF(agent_class.c_str());

    m_env->CallStaticVoidMethod(clazz, load_agent, path_j_str, agent_class_j_str);

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
    case load_status::CLASS_NOT_LOADED:
        stream << "Class not loaded";
        break;
    }

    return stream;
}
