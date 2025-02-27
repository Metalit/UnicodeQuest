#pragma once

#include "jutils.hpp"

namespace Java {
    struct JNIFrame {
        JNIFrame(JNIEnv* env, int size) : env(env) { env->PushLocalFrame(size); }
        ~JNIFrame() { pop(); }

        void pop() {
            if (env)
                env->PopLocalFrame(nullptr);
            env = nullptr;
        }

       private:
        JNIEnv* env;
    };

    JNIEnv* GetEnv();

    jobject NewObject(JNIEnv* env, FindClass clazz, FindMethodID init, ...);
    jobject NewObject(JNIEnv* env, FindClass clazz, std::string init, ...);

    template <class T = void>
    T RunMethod(JNIEnv* env, FindClass clazz, FindMethodID method, ...);

    template <class T>
    T GetField(JNIEnv* env, FindClass clazz, FindFieldID field);

    template <class T>
    void SetField(JNIEnv* env, FindClass clazz, FindFieldID field, T value);

    jclass LoadClass(JNIEnv* env, std::string_view dexBytes);

    std::string ConvertString(JNIEnv* env, jstring string);

    std::string GetClassName(JNIEnv* env, jclass clazz);

#define SPECIALIZATION(type) \
    extern template type RunMethod(JNIEnv* env, FindClass clazz, FindMethodID method, ...); \
    extern template type GetField(JNIEnv* env, FindClass clazz, FindFieldID field); \
    extern template void SetField(JNIEnv* env, FindClass clazz, FindFieldID field, type value);

    SPECIALIZATION(jobject);
    SPECIALIZATION(bool);
    SPECIALIZATION(uint8_t);
    SPECIALIZATION(char);
    SPECIALIZATION(short);
    SPECIALIZATION(int);
    SPECIALIZATION(long);
    SPECIALIZATION(float);
    SPECIALIZATION(double);

    extern template void RunMethod(JNIEnv* env, FindClass clazz, FindMethodID method, ...);

#undef SPECIALIZATION
}
