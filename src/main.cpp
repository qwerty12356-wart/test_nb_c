/*
 * Copyright (C) 2015-2017 The Android-x86 Open Source Project
 *
 * by Chih-Wei Huang <cwhuang@linux.org.tw>
 *
 * Licensed under the GNU General Public License Version 2 or later.
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.gnu.org/licenses/gpl.html
 *
 */


#include <dlfcn.h>

#include "nativebridge.h"
#include <android/log.h>



#define LIBRARY_ADDRESS_BY_HANDLE(dlhandle) ((0 == dlhandle) ? 0 : (void*)*(size_t const*)(dlhandle))

//Adjust this number if you wish to use other patches
#ifndef PATCHTOUSE
#define PATCHTOUSE 0
#endif

#ifndef USE_NATIVEBRIDGE
#define USE_NATIVEBRIDGE "libhoudini.so"
#endif


#define USED_NATIVEBRIDGE "/" USE_NATIVEBRIDGE





typedef int (*patchmainfn)(void* , unsigned short);
typedef bool (*odmPatchfn)(const android::NativeBridgeRuntimeCallbacks*,const char *,const char *);


namespace android {
static void *native_handle = nullptr;
static void *patcher_handle = nullptr;
static NativeBridgeCallbacks *callbacks = nullptr;

static bool is_native_bridge_enabled()
{
    return true;
}

static NativeBridgeCallbacks *get_callbacks()
{
    if (!callbacks) {
        const char *libnb = "/system/lib"
        #ifdef __LP64__
                "64"
        #endif
                USED_NATIVEBRIDGE;

        if (!native_handle) {
            native_handle = dlopen(libnb, RTLD_LAZY);
            if (!native_handle){
                __android_log_print(ANDROID_LOG_ERROR,"libnb_custom" ,"Unable to open %s: %s", libnb, dlerror());
                return nullptr;
            }
        }
        callbacks = reinterpret_cast<NativeBridgeCallbacks *>(dlsym(native_handle, "NativeBridgeItf"));
        __android_log_print(ANDROID_LOG_INFO,"libnb_custom" , libnb, callbacks ? callbacks->version : 0);
    }
    return callbacks;
}
// NativeBridgeCallbacks implementations
static bool native_bridge2_initialize(const NativeBridgeRuntimeCallbacks *art_cbs,
                                      const char *app_code_cache_dir,
                                      const char *isa)
{   
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_initialize %s %s", app_code_cache_dir, isa);
    #endif
    if (is_native_bridge_enabled()) {
        if (NativeBridgeCallbacks *cb = get_callbacks()) {
            if (!patcher_handle){
                const char* libnbpatcher = "/system/lib"
                #ifdef __LP64__
                                    "64"
                #endif
                "/libnbpatcher.so";
                patcher_handle = dlopen(libnbpatcher, RTLD_LAZY);
                if (!patcher_handle){
                    __android_log_print(ANDROID_LOG_ERROR, "libnb_custom", "libnbpatcher not found");
                    goto next;
                }
                else {
                    patchmainfn patch_main = reinterpret_cast<patchmainfn>(dlsym(patcher_handle, "patch_main"));

                    if (!patch_main){
                        __android_log_print(ANDROID_LOG_ERROR, "libnb_custom", "Failed to call patch_main");
                        goto next;
                    }
                    Dl_info dlinf{};
                    dladdr(cb, &dlinf);
                
                    patch_main(dlinf.dli_fbase, PATCHTOUSE);
                }
                
            }
            next:
            if (patcher_handle){
                odmPatchfn odmPatch = reinterpret_cast<odmPatchfn>(dlsym(patcher_handle, "onDemandPatch"));
                odmPatch(art_cbs, app_code_cache_dir, isa);
            }
            return cb->initialize(art_cbs, app_code_cache_dir, isa);
        }
        __android_log_print(ANDROID_LOG_WARN,"libnb_custom", "Native bridge is enabled but callbacks not found");
    } else {
       __android_log_print(ANDROID_LOG_WARN,"libnb_custom" ,"Native bridge is disabled");
    }
    return false;
}

static void *native_bridge2_loadLibrary(const char *libpath, int flag)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_loadLibrary %s", libpath);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->loadLibrary(libpath, flag) : nullptr;
}

static void *native_bridge2_getTrampoline(void *handle, const char *name,
                                          const char* shorty, uint32_t len)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_getTrampoline %s", name);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->getTrampoline(handle, name, shorty, len) : nullptr;
}

static bool native_bridge2_isSupported(const char *libpath)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_isSupported %s", libpath);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->isSupported(libpath) : false;
}

static const struct NativeBridgeRuntimeValues *native_bridge2_getAppEnv(const char *abi)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_getAppEnv %s", abi);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->getAppEnv(abi) : nullptr;
}

static bool native_bridge2_isCompatibleWith(uint32_t version)
{
    static uint32_t my_version = 0;
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_isCompatibleWith %u", version);
    #endif
    if (my_version == 0 && is_native_bridge_enabled()) {
        if (NativeBridgeCallbacks *cb = get_callbacks()) {
            my_version = cb->version;
        }
    }
    // We have to claim a valid version before loading the real callbacks,
    // otherwise native bridge will be disabled entirely
    return version <= (my_version ? my_version : 3);
}

static NativeBridgeSignalHandlerFn native_bridge2_getSignalHandler(int signal)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge2_getSignalHandler %d", signal);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->getSignalHandler(signal) : nullptr;
}

static int native_bridge3_unloadLibrary(void *handle)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_unloadLibrary %p", handle);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->unloadLibrary(handle) : -1;
}

static const char *native_bridge3_getError()
{
    #ifdef LOG_DEBUG
   __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_getError");
   #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->getError() : "unknown";
}

static bool native_bridge3_isPathSupported(const char *path)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_isPathSupported %s", path);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb && cb->isPathSupported(path);
}

static bool native_bridge3_initAnonymousNamespace(const char *public_ns_sonames,
                                                  const char *anon_ns_library_path)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_initAnonymousNamespace %s, %s", public_ns_sonames, anon_ns_library_path);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb && cb->initAnonymousNamespace(public_ns_sonames, anon_ns_library_path);
}

static native_bridge_namespace_t *
native_bridge3_createNamespace(const char *name,
                               const char *ld_library_path,
                               const char *default_library_path,
                               uint64_t type,
                               const char *permitted_when_isolated_path,
                               native_bridge_namespace_t *parent_ns)
{
    #ifdef LOG_DEBUG
   __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_createNamespace %s, %s, %s, %s", name, ld_library_path, default_library_path, permitted_when_isolated_path);
   #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->createNamespace(name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_ns) : nullptr;
}

static bool native_bridge3_linkNamespaces(native_bridge_namespace_t *from,
                                          native_bridge_namespace_t *to,
                                          const char *shared_libs_soname)
{
    #ifdef LOG_DEBUG
   __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_linkNamespaces %s", shared_libs_soname);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb && cb->linkNamespaces(from, to, shared_libs_soname);
}

static void *native_bridge3_loadLibraryExt(const char *libpath,
                                           int flag,
                                           native_bridge_namespace_t *ns)
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge3_loadLibraryExt %s, %d, %p", libpath, flag, ns);
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    void *result = cb ? cb->loadLibraryExt(libpath, flag, ns) : nullptr;
//  void *result = cb ? cb->loadLibrary(libpath, flag) : nullptr;
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"native_bridge3_loadLibraryExt: %p", result);
    #endif
    return result;
}

static native_bridge_namespace_t *native_bridge4_getVendorNamespace()
{
    #ifdef LOG_DEBUG
    __android_log_print(ANDROID_LOG_DEBUG,"libnb_custom" ,"enter native_bridge4_getVendorNamespace");
    #endif
    NativeBridgeCallbacks *cb = get_callbacks();
    return cb ? cb->getVendorNamespace() : nullptr;
}

static native_bridge_namespace_t* native_bridge5_getExportedNamespace(const char* name){
    #ifdef LOG_DEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "libnb_custom", "enter native_bridge5_getExportedNamespace");
    #endif
    NativeBridgeCallbacks* cb = get_callbacks();
    return cb ? cb->getExportedNamespace(name) : nullptr;
}

static void native_bridge6_preZygoteFork(){
    #ifdef LOG_DEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "libnb_custom", "enter native_bridge6_preZygoteFork");
    #endif
    NativeBridgeCallbacks* cb =  get_callbacks();
    cb->preZygoteFork();
}

static void* native_bridge6_getTrampolineWithJNICallType(void* handle,const char* name,const char* shorty,uint32_t len,enum JNICallType jni_call_type){
    #ifdef LOG_DEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "libnb_custom", "enter native_bridge6_getTrampolineWithJNICallType");
    #endif
    NativeBridgeCallbacks* cb = get_callbacks();
    return cb ? cb->getTrampolineWithJNICallType(handle, name, shorty, len, jni_call_type) : nullptr;
}

static void* native_bridge6_getTrampolineForFunctionPointer(const void* method,const char* shorty,uint32_t len,enum JNICallType jni_call_type){
     #ifdef LOG_DEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "libnb_custom", "enter native_bridge6_getTrampolineForFunctionPointer");
    #endif
    NativeBridgeCallbacks* cb = get_callbacks();
    return cb ? cb->getTrampolineForFunctionPointer(method, shorty, len, jni_call_type) : nullptr;
}


static void __attribute__ ((destructor)) on_dlclose()
{
    if (native_handle) {
        dlclose(native_handle);
        native_handle = nullptr;
    }
    if (patcher_handle){
        dlclose(patcher_handle);
        patcher_handle = nullptr;
    }
}

extern "C" {

NativeBridgeCallbacks NativeBridgeItf = {
    // v1
    .version = 5,
    .initialize = native_bridge2_initialize,
    .loadLibrary = native_bridge2_loadLibrary,
    .getTrampoline = native_bridge2_getTrampoline,
    .isSupported = native_bridge2_isSupported,
    .getAppEnv = native_bridge2_getAppEnv,
    // v2
    .isCompatibleWith = native_bridge2_isCompatibleWith,
    .getSignalHandler = native_bridge2_getSignalHandler,
    // v3
    .unloadLibrary = native_bridge3_unloadLibrary,
    .getError = native_bridge3_getError,
    .isPathSupported = native_bridge3_isPathSupported,
    .initAnonymousNamespace = native_bridge3_initAnonymousNamespace,
    .createNamespace = native_bridge3_createNamespace,
    .linkNamespaces = native_bridge3_linkNamespaces,
    .loadLibraryExt = native_bridge3_loadLibraryExt,
    // v4
    .getVendorNamespace = native_bridge4_getVendorNamespace,
    // v5
    .getExportedNamespace = native_bridge5_getExportedNamespace,
    // v6?
    .preZygoteFork = native_bridge6_preZygoteFork,
    .getTrampolineWithJNICallType = native_bridge6_getTrampolineWithJNICallType,
    .getTrampolineForFunctionPointer = native_bridge6_getTrampolineForFunctionPointer,

};

} // extern "C"
} // namespace android