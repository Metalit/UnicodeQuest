#pragma once

#include <jni.h>

#include <string_view>

namespace Java {
    struct FindClass {
        jclass clazz = nullptr;
        jobject instance = nullptr;
        std::string_view name = "";

        FindClass(jclass clazz) : clazz(clazz) {}
        FindClass(jobject instance) : instance(instance) {}
        FindClass(std::string_view name) : name(name) {}
    };

    struct FindMethodID {
        jmethodID method = nullptr;
        std::string_view name = "";
        std::string_view signature = "";

        FindMethodID(jmethodID method) : method(method) {}
        FindMethodID(std::string_view name, std::string_view signature) : name(name), signature(signature) {}
    };

    struct FindFieldID {
        jfieldID field = nullptr;
        std::string_view name = "";
        std::string_view signature = "";

        FindFieldID(jfieldID field) : field(field) {}
        FindFieldID(std::string_view name, std::string_view signature) : name(name), signature(signature) {}
    };

    jclass GetClass(JNIEnv* env, FindClass clazz);
    jmethodID GetMethodID(JNIEnv* env, FindClass clazz, FindMethodID method);
    jfieldID GetFieldID(JNIEnv* env, FindClass clazz, FindFieldID field);
}
