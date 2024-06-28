# What is this?
A custom libnb.so. Which will load a special patch library (found [here](https://github.com/qwerty12356-wart/test_nbpatch)) when initialized.

# Installation
1. Obtain a compiled libnb.so and the patcher library (see Building)
2. Copy libnb.so and the patcher library to system/lib (for 32 bit) and system/lib64 (for 64 bit)
3. edit ``ro.dalvik.vm.native.bridge`` in system/build.prop to point to libnb.so

# Building
1. Either export ANDROID_NDK_HOME or edit it in CMakeLists.txt to point to the folder containing the extracted Android NDK tools
2. ``cmake . -B build -D{ADD_MORE_OPTION_HERE}``
3.``cd build``, ``make``
Available build options:
COMPILE_ARCH
LOG_DEBUG
Enable it like this: ``cmake -B build -DCOMPILE_ARCH=1``
