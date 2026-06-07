#include "GeraNESApp/AndroidFileDialog.h"

#ifdef __ANDROID__

#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#include <nlohmann/json.hpp>

#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct PickerResult
    {
        bool completed = false;
        bool cancelled = false;
        std::string pathOrUri;
        std::string error;
    };

    std::mutex g_pickerMutex;
    std::condition_variable g_pickerCv;
    std::unordered_map<int, PickerResult> g_pickerResults;
    int g_nextPickerRequestId = 1;

    JNIEnv* currentEnv()
    {
        return reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    }

    jobject currentActivity()
    {
        return reinterpret_cast<jobject>(SDL_AndroidGetActivity());
    }

    std::string fromJString(JNIEnv* env, jstring value)
    {
        if(env == nullptr || value == nullptr) return {};

        const char* chars = env->GetStringUTFChars(value, nullptr);
        if(chars == nullptr) return {};

        std::string result(chars);
        env->ReleaseStringUTFChars(value, chars);
        return result;
    }

    int beginPickerRequest()
    {
        std::scoped_lock lock(g_pickerMutex);
        const int requestId = g_nextPickerRequestId++;
        g_pickerResults.emplace(requestId, PickerResult{});
        return requestId;
    }

    bool finishPickerRequest(int requestId, std::string& outValue, std::string* outError)
    {
        std::unique_lock lock(g_pickerMutex);
        g_pickerCv.wait(lock, [requestId]() {
            const auto it = g_pickerResults.find(requestId);
            return it != g_pickerResults.end() && it->second.completed;
        });

        const auto it = g_pickerResults.find(requestId);
        if(it == g_pickerResults.end()) {
            if(outError != nullptr) *outError = "Android picker request disappeared.";
            return false;
        }

        const PickerResult result = it->second;
        g_pickerResults.erase(it);
        lock.unlock();

        if(result.cancelled) {
            return false;
        }
        if(!result.error.empty()) {
            if(outError != nullptr) *outError = result.error;
            return false;
        }

        outValue = result.pathOrUri;
        return !outValue.empty();
    }

    void completePickerRequest(int requestId, const std::string& value, const std::string& error)
    {
        std::scoped_lock lock(g_pickerMutex);
        auto& result = g_pickerResults[requestId];
        result.completed = true;
        result.cancelled = value.empty() && error.empty();
        result.pathOrUri = value;
        result.error = error;
        g_pickerCv.notify_all();
    }

    bool parsePickedFilePayload(const std::string& payload, AndroidFileDialog::PickedFile& outFile)
    {
        try {
            const nlohmann::json data = nlohmann::json::parse(payload);
            if(!data.is_object()) {
                return false;
            }

            outFile.cachePath = data.value("cachePath", "");
            outFile.displayName = data.value("displayName", "");
            outFile.uri = data.value("uri", "");
            return !outFile.cachePath.empty();
        } catch(...) {
            return false;
        }
    }

    bool invokeOpenFilePicker(const std::vector<std::string>& mimeTypes, std::string& outPath, std::string* outError)
    {
        JNIEnv* env = currentEnv();
        jobject activity = currentActivity();
        if(env == nullptr || activity == nullptr) {
            if(outError != nullptr) *outError = "SDL Android activity is unavailable.";
            return false;
        }

        jclass activityClass = env->GetObjectClass(activity);
        if(activityClass == nullptr) {
            if(outError != nullptr) *outError = "Could not resolve Android activity class.";
            return false;
        }

        const jmethodID method = env->GetMethodID(activityClass, "geranesShowOpenDocumentPicker", "(I[Ljava/lang/String;)V");
        if(method == nullptr) {
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android activity does not implement geranesShowOpenDocumentPicker.";
            return false;
        }

        jclass stringClass = env->FindClass("java/lang/String");
        if(stringClass == nullptr) {
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Could not resolve java.lang.String.";
            return false;
        }

        jobjectArray mimeArray = env->NewObjectArray(static_cast<jsize>(mimeTypes.size()), stringClass, nullptr);
        if(mimeArray == nullptr) {
            env->DeleteLocalRef(stringClass);
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Could not allocate Android MIME type array.";
            return false;
        }
        for(jsize i = 0; i < static_cast<jsize>(mimeTypes.size()); ++i) {
            jstring mimeString = env->NewStringUTF(mimeTypes[i].c_str());
            env->SetObjectArrayElement(mimeArray, i, mimeString);
            env->DeleteLocalRef(mimeString);
        }

        const int requestId = beginPickerRequest();
        env->CallVoidMethod(activity, method, static_cast<jint>(requestId), mimeArray);
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(mimeArray);
            env->DeleteLocalRef(stringClass);
            env->DeleteLocalRef(activityClass);
            std::scoped_lock lock(g_pickerMutex);
            g_pickerResults.erase(requestId);
            if(outError != nullptr) *outError = "Android document picker threw an exception.";
            return false;
        }

        env->DeleteLocalRef(mimeArray);
        env->DeleteLocalRef(stringClass);
        env->DeleteLocalRef(activityClass);

        return finishPickerRequest(requestId, outPath, outError);
    }

    bool invokeOpenTreePicker(std::string& outPath, std::string* outError)
    {
        JNIEnv* env = currentEnv();
        jobject activity = currentActivity();
        if(env == nullptr || activity == nullptr) {
            if(outError != nullptr) *outError = "SDL Android activity is unavailable.";
            return false;
        }

        jclass activityClass = env->GetObjectClass(activity);
        if(activityClass == nullptr) {
            if(outError != nullptr) *outError = "Could not resolve Android activity class.";
            return false;
        }

        const jmethodID method = env->GetMethodID(activityClass, "geranesShowOpenTreePicker", "(I)V");
        if(method == nullptr) {
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android activity does not implement geranesShowOpenTreePicker.";
            return false;
        }

        const int requestId = beginPickerRequest();
        env->CallVoidMethod(activity, method, static_cast<jint>(requestId));
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(activityClass);
            std::scoped_lock lock(g_pickerMutex);
            g_pickerResults.erase(requestId);
            if(outError != nullptr) *outError = "Android folder picker threw an exception.";
            return false;
        }
        env->DeleteLocalRef(activityClass);

        return finishPickerRequest(requestId, outPath, outError);
    }

    bool invokeCreateDocumentPicker(const std::string& suggestedName,
                                    const std::string& mimeType,
                                    std::string& outUri,
                                    std::string* outError)
    {
        JNIEnv* env = currentEnv();
        jobject activity = currentActivity();
        if(env == nullptr || activity == nullptr) {
            if(outError != nullptr) *outError = "SDL Android activity is unavailable.";
            return false;
        }

        jclass activityClass = env->GetObjectClass(activity);
        if(activityClass == nullptr) {
            if(outError != nullptr) *outError = "Could not resolve Android activity class.";
            return false;
        }

        const jmethodID method = env->GetMethodID(activityClass, "geranesShowCreateDocumentPicker", "(ILjava/lang/String;Ljava/lang/String;)V");
        if(method == nullptr) {
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android activity does not implement geranesShowCreateDocumentPicker.";
            return false;
        }

        jstring mimeString = env->NewStringUTF(mimeType.c_str());
        jstring nameString = env->NewStringUTF(suggestedName.c_str());
        const int requestId = beginPickerRequest();
        env->CallVoidMethod(activity, method, static_cast<jint>(requestId), mimeString, nameString);
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(nameString);
            env->DeleteLocalRef(mimeString);
            env->DeleteLocalRef(activityClass);
            std::scoped_lock lock(g_pickerMutex);
            g_pickerResults.erase(requestId);
            if(outError != nullptr) *outError = "Android create-document picker threw an exception.";
            return false;
        }
        env->DeleteLocalRef(nameString);
        env->DeleteLocalRef(mimeString);
        env->DeleteLocalRef(activityClass);

        return finishPickerRequest(requestId, outUri, outError);
    }

    bool writeBytesToUri(const std::string& uri, const std::vector<uint8_t>& bytes, std::string* outError)
    {
        JNIEnv* env = currentEnv();
        jobject activity = currentActivity();
        if(env == nullptr || activity == nullptr) {
            if(outError != nullptr) *outError = "SDL Android activity is unavailable.";
            return false;
        }

        jclass activityClass = env->GetObjectClass(activity);
        if(activityClass == nullptr) {
            if(outError != nullptr) *outError = "Could not resolve Android activity class.";
            return false;
        }

        const jmethodID method = env->GetMethodID(activityClass, "geranesWriteBytesToUri", "(Ljava/lang/String;[B)Z");
        if(method == nullptr) {
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android activity does not implement geranesWriteBytesToUri.";
            return false;
        }

        jstring uriString = env->NewStringUTF(uri.c_str());
        jbyteArray byteArray = env->NewByteArray(static_cast<jsize>(bytes.size()));
        if(byteArray != nullptr && !bytes.empty()) {
            env->SetByteArrayRegion(byteArray, 0, static_cast<jsize>(bytes.size()),
                reinterpret_cast<const jbyte*>(bytes.data()));
        }

        const jboolean ok = env->CallBooleanMethod(activity, method, uriString, byteArray);
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(byteArray);
            env->DeleteLocalRef(uriString);
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android document writer threw an exception.";
            return false;
        }

        env->DeleteLocalRef(byteArray);
        env->DeleteLocalRef(uriString);
        env->DeleteLocalRef(activityClass);

        if(ok != JNI_TRUE && outError != nullptr) {
            *outError = "Android document write failed.";
        }
        return ok == JNI_TRUE;
    }
}

namespace AndroidFileDialog
{
    bool pickFileToCache(const std::vector<std::string>& mimeTypes, std::string& outPath, std::string* outError)
    {
        PickedFile pickedFile;
        if(!pickFileToCacheWithMetadata(mimeTypes, pickedFile, outError)) {
            return false;
        }
        outPath = pickedFile.cachePath;
        return true;
    }

    bool pickFileToCacheWithMetadata(const std::vector<std::string>& mimeTypes, PickedFile& outFile, std::string* outError)
    {
        const std::vector<std::string> effectiveTypes = mimeTypes.empty() ? std::vector<std::string>{"*/*"} : mimeTypes;
        std::string payload;
        if(!invokeOpenFilePicker(effectiveTypes, payload, outError)) {
            return false;
        }

        if(!parsePickedFilePayload(payload, outFile)) {
            if(outError != nullptr) {
                *outError = "Android picker returned an invalid document payload.";
            }
            return false;
        }
        return true;
    }

    bool copyDocumentUriToCache(const std::string& uri, PickedFile& outFile, std::string* outError)
    {
        JNIEnv* env = currentEnv();
        jobject activity = currentActivity();
        if(env == nullptr || activity == nullptr) {
            if(outError != nullptr) *outError = "SDL Android activity is unavailable.";
            return false;
        }

        jclass activityClass = env->GetObjectClass(activity);
        if(activityClass == nullptr) {
            if(outError != nullptr) *outError = "Could not resolve Android activity class.";
            return false;
        }

        const jmethodID method = env->GetMethodID(activityClass, "geranesCopyDocumentUriToCache", "(Ljava/lang/String;)Ljava/lang/String;");
        if(method == nullptr) {
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android activity does not implement geranesCopyDocumentUriToCache.";
            return false;
        }

        jstring uriString = env->NewStringUTF(uri.c_str());
        jstring payloadString = static_cast<jstring>(env->CallObjectMethod(activity, method, uriString));
        if(env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(uriString);
            env->DeleteLocalRef(activityClass);
            if(outError != nullptr) *outError = "Android document cache copy threw an exception.";
            return false;
        }

        const std::string payload = fromJString(env, payloadString);
        env->DeleteLocalRef(payloadString);
        env->DeleteLocalRef(uriString);
        env->DeleteLocalRef(activityClass);

        if(payload.empty()) {
            if(outError != nullptr) *outError = "Android document cache copy returned no data.";
            return false;
        }

        if(!parsePickedFilePayload(payload, outFile)) {
            if(outError != nullptr) *outError = "Android document cache copy returned invalid data.";
            return false;
        }

        return true;
    }

    bool pickFolderToCache(std::string& outPath, std::string* outError)
    {
        return invokeOpenTreePicker(outPath, outError);
    }

    bool saveBytesWithDocumentPicker(const std::string& suggestedName,
                                     const std::string& mimeType,
                                     const std::vector<uint8_t>& bytes,
                                     std::string* outError)
    {
        std::string uri;
        if(!invokeCreateDocumentPicker(suggestedName, mimeType, uri, outError)) {
            return false;
        }
        return writeBytesToUri(uri, bytes, outError);
    }
}

extern "C"
{
    JNIEXPORT void JNICALL Java_org_libsdl_app_GeraNESActivity_nativeOnOpenPickerResult(
        JNIEnv* env, jclass, jint requestId, jstring localPath, jstring error)
    {
        completePickerRequest(static_cast<int>(requestId), fromJString(env, localPath), fromJString(env, error));
    }

    JNIEXPORT void JNICALL Java_org_libsdl_app_GeraNESActivity_nativeOnCreateDocumentResult(
        JNIEnv* env, jclass, jint requestId, jstring uri, jstring error)
    {
        completePickerRequest(static_cast<int>(requestId), fromJString(env, uri), fromJString(env, error));
    }
}

#else

namespace AndroidFileDialog
{
    bool pickFileToCache(const std::vector<std::string>&, std::string&, std::string*)
    {
        return false;
    }

    bool pickFileToCacheWithMetadata(const std::vector<std::string>&, PickedFile&, std::string*)
    {
        return false;
    }

    bool copyDocumentUriToCache(const std::string&, PickedFile&, std::string*)
    {
        return false;
    }

    bool pickFolderToCache(std::string&, std::string*)
    {
        return false;
    }

    bool saveBytesWithDocumentPicker(const std::string&, const std::string&, const std::vector<uint8_t>&, std::string*)
    {
        return false;
    }
}

#endif
