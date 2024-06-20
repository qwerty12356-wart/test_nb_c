add_library(libnb SHARED main.cpp)

target_include_directories(libnb PRIVATE "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include")