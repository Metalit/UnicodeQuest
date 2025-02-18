#include "java.hpp"

#include "jtypes.hpp"
#include "main.hpp"

JNIEnv* Java::GetEnv() {
    JNIEnv* env;

    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = NULL;
    args.group = NULL;

    modloader_jvm->AttachCurrentThread(&env, &args);

    return env;
}

jclass Java::GetClass(JNIEnv* env, FindClass clazz) {
    if (clazz.clazz)
        return clazz.clazz;
    if (clazz.instance)
        return env->GetObjectClass(clazz.instance);
    if (!clazz.name.empty())
        return env->FindClass(clazz.name.data());
    return nullptr;
}

jmethodID Java::GetMethodID(JNIEnv* env, FindClass clazz, FindMethodID method) {
    if (method.method)
        return method.method;
    jclass foundClass = GetClass(env, clazz);
    if (!foundClass || method.name.empty() || method.signature.empty())
        return nullptr;
    return env->GetMethodID(foundClass, method.name.data(), method.signature.data());
    // delete local ref of class?
}

jfieldID Java::GetFieldID(JNIEnv* env, FindClass clazz, FindFieldID field) {
    if (field.field)
        return field.field;
    jclass foundClass = GetClass(env, clazz);
    if (!foundClass || field.name.empty() || field.signature.empty())
        return nullptr;
    return env->GetFieldID(foundClass, field.name.data(), field.signature.data());
}

jobject Java::NewObject(JNIEnv* env, FindClass clazz, FindMethodID init, ...) {
    va_list va;
    va_start(va, init);
    auto foundClass = GetClass(env, clazz);
    auto foundMethod = GetMethodID(env, foundClass, init);
    if (!foundClass || !foundMethod)
        return nullptr;
    return env->NewObjectV(foundClass, foundMethod, va);
}

jobject Java::NewObject(JNIEnv* env, FindClass clazz, std::string_view init, ...) {
    va_list va;
    va_start(va, init);
    auto foundClass = GetClass(env, clazz);
    auto foundMethod = GetMethodID(env, {foundClass}, {"<init>", init});
    if (!foundClass || !foundMethod)
        return nullptr;
    return env->NewObjectV(foundClass, foundMethod, va);
}

template <class T>
T Java::RunMethod(JNIEnv* env, FindClass clazz, FindMethodID method, ...) {
    va_list va;
    va_start(va, method);
    if (clazz.instance) {
        auto foundMethod = GetMethodID(env, clazz, method);
        if constexpr (std::is_same_v<T, void>)
            std::invoke(TypeResolver<T>::JMethod, env, clazz.instance, foundMethod, va);
        else
            return std::invoke(TypeResolver<T>::JMethod, env, clazz.instance, foundMethod, va);
    } else {
        auto foundClass = GetClass(env, clazz);
        auto foundMethod = GetMethodID(env, foundClass, method);
        if constexpr (std::is_same_v<T, void>)
            std::invoke(TypeResolver<T>::JStaticMethod, env, foundClass, foundMethod, va);
        else
            return std::invoke(TypeResolver<T>::JStaticMethod, env, foundClass, foundMethod, va);
    }
}

template <class T>
T Java::GetField(JNIEnv* env, FindClass clazz, FindFieldID field) {
    if (clazz.instance) {
        auto foundField = GetFieldID(env, clazz, field);
        return std::invoke(TypeResolver<T>::JGetField, env, clazz.instance, foundField);
    } else {
        auto foundClass = GetClass(env, clazz);
        auto foundField = GetFieldID(env, foundClass, field);
        return std::invoke(TypeResolver<T>::JGetStaticField, env, foundClass, foundField);
    }
}

template <class T>
void Java::SetField(JNIEnv* env, FindClass clazz, FindFieldID field, T value) {
    if (clazz.instance) {
        auto foundField = GetFieldID(env, clazz, field);
        std::invoke(TypeResolver<T>::JSetField, env, clazz.instance, foundField, value);
    } else {
        auto foundClass = GetClass(env, clazz);
        auto foundField = GetFieldID(env, foundClass, field);
        std::invoke(TypeResolver<T>::JSetStaticField, env, foundClass, foundField, value);
    }
}

jclass Java::LoadClass(JNIEnv* env, std::string_view dexBytes) {
    auto dexBuffer = env->NewDirectByteBuffer((void*) dexBytes.data(), dexBytes.length());

    // not sure if necessary to run this on the UnityPlayer class
    auto baseClassLoader = RunMethod<jobject>(env, {"com/unity3d/player/UnityPlayer"}, {"getClassLoader", "()Ljava/lang/ClassLoader;"});

    auto classLoader = NewObject(
        env, {"dalvik/system/InMemoryDexClassLoader"}, "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V", dexBuffer, baseClassLoader
    );

    auto loadedClass = RunMethod<jobject>(
        env, classLoader, {"loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"}, env->NewStringUTF("com.metalit.hollywood.MainClass")
    );

    return (jclass) loadedClass;
}

#define SPECIALIZATION(type) \
    template type Java::RunMethod(JNIEnv* env, FindClass clazz, FindMethodID method, ...); \
    template type Java::GetField(JNIEnv* env, FindClass clazz, FindFieldID field); \
    template void Java::SetField(JNIEnv* env, FindClass clazz, FindFieldID field, type value)

SPECIALIZATION(jobject);
SPECIALIZATION(bool);
SPECIALIZATION(uint8_t);
SPECIALIZATION(char);
SPECIALIZATION(short);
SPECIALIZATION(int);
SPECIALIZATION(long);
SPECIALIZATION(float);
SPECIALIZATION(double);

template void Java::RunMethod(JNIEnv* env, FindClass clazz, FindMethodID method, ...);
