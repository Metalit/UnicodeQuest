#pragma once

#include <jni.h>

#include <string>

namespace Java {
    struct FindClass {
        jclass clazz = nullptr;
        jobject instance = nullptr;
        std::string name = "";

        FindClass(jclass clazz) : clazz(clazz) {}
        FindClass(jobject instance) : instance(instance) {}
        FindClass(jclass clazz, jobject instance) : clazz(clazz), instance(instance) {}
        FindClass(std::string name) : name(name) {}
    };

    struct FindMethodID {
        jmethodID method = nullptr;
        std::string name = "";
        std::string signature = "";

        FindMethodID(jmethodID method) : method(method) {}
        FindMethodID(std::string name, std::string signature) : name(name), signature(signature) {}
    };

    struct FindFieldID {
        jfieldID field = nullptr;
        std::string name = "";
        std::string signature = "";

        FindFieldID(jfieldID field) : field(field) {}
        FindFieldID(std::string name, std::string signature) : name(name), signature(signature) {}
    };

    jclass GetClass(JNIEnv* env, FindClass clazz);
    jmethodID GetMethodID(JNIEnv* env, FindClass clazz, FindMethodID method);
    jfieldID GetFieldID(JNIEnv* env, FindClass clazz, FindFieldID field);
}
