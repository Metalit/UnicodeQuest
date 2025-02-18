#pragma once

#include <jni.h>

namespace Java {
    template <class T>
    struct TypeResolver {
        using JType = T;
        static constexpr auto JMethod = &JNIEnv::CallObjectMethodV;
        static constexpr auto JStaticMethod = &JNIEnv::CallStaticObjectMethodV;
        static constexpr auto JGetField = &JNIEnv::GetObjectField;
        static constexpr auto JGetStaticField = &JNIEnv::GetStaticObjectField;
        static constexpr auto JSetField = &JNIEnv::SetObjectField;
        static constexpr auto JSetStaticField = &JNIEnv::SetStaticObjectField;
    };

    template <>
    struct TypeResolver<void> {
        static constexpr auto JMethod = &JNIEnv::CallVoidMethodV;
        static constexpr auto JStaticMethod = &JNIEnv::CallStaticVoidMethodV;
    };

#define TYPE_RESOLUTION(type, jtype, jname) \
    template <> \
    struct TypeResolver<type> { \
        using JType = jtype; \
        static constexpr auto JMethod = &JNIEnv::Call##jname##MethodV; \
        static constexpr auto JStaticMethod = &JNIEnv::CallStatic##jname##MethodV; \
        static constexpr auto JGetField = &JNIEnv::Get##jname##Field; \
        static constexpr auto JGetStaticField = &JNIEnv::GetStatic##jname##Field; \
        static constexpr auto JSetField = &JNIEnv::Set##jname##Field; \
        static constexpr auto JSetStaticField = &JNIEnv::SetStatic##jname##Field; \
    }

    TYPE_RESOLUTION(bool, jboolean, Boolean);
    TYPE_RESOLUTION(uint8_t, jbyte, Byte);
    TYPE_RESOLUTION(char, jchar, Char);
    TYPE_RESOLUTION(short, jshort, Short);
    TYPE_RESOLUTION(int, jint, Int);
    TYPE_RESOLUTION(long, jlong, Long);
    TYPE_RESOLUTION(float, jfloat, Float);
    TYPE_RESOLUTION(double, jdouble, Double);

#undef TYPE_RESOLUTION
}
