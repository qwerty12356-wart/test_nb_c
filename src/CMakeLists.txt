add_library(nb SHARED main.cpp)
target_link_libraries(nb log)
#target_include_directories(libnb PRIVATE "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include")