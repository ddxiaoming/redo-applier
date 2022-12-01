# 设置交叉编译器
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(tools /usr/local/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu)
set(CMAKE_C_COMPILER ${tools}/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER ${tools}/bin/aarch64-none-linux-gnu-as)
set(CMAKE_AR ${tools}/bin/aarch64-none-linux-gnu-ar)

