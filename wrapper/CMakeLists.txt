cmake_minimum_required(VERSION 3.25)

project(wrapper)
include(ExternalProject)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

set(ANDROID_NDK_PATH "$ENV{HOME}/Android/Sdk/ndk/29.0.14206865")
set(TOOLCHAIN "${ANDROID_NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64")

set(CMAKE_C_COMPILER "${TOOLCHAIN}/bin/x86_64-linux-android22-clang")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN}/bin/x86_64-linux-android22-clang++")
set(C_COMPILER "${TOOLCHAIN}/bin/clang")

set(CMAKE_C_FLAGS "-Wall -O3")
set(CMAKE_CXX_FLAGS "-Wall -O3")

set(CMDLINE_SOURCE cmdline.c)
# set(HANDLE_SOURCE main.cpp)
set(MAIN_SOURCE main.c)
set(WRAPPER_SOURCE wrapper.c)

add_library(cmdline_object OBJECT ${CMDLINE_SOURCE})
# add_library(handle_object OBJECT ${HANDLE_SOURCE})

add_executable(amwrapper main.c subhook/subhook.c subhookwrapper.cpp $<TARGET_OBJECTS:cmdline_object>)

target_include_directories(amwrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(amwrapper PROPERTIES
    COMPILE_DEFINITIONS "MyRelease;HAVE_CONFIG_H"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
)

find_library(ANDROIDAPPMUSIC_LIB androidappmusic PATHS ${CMAKE_SOURCE_DIR}/../bin/rootfs/system/lib64 REQUIRED)
find_library(STORESERVICESCORE_LIB storeservicescore PATHS ${CMAKE_SOURCE_DIR}/../bin/rootfs/system/lib64 REQUIRED)
find_library(MEDIAPLATFORM_LIB mediaplatform PATHS ${CMAKE_SOURCE_DIR}/../bin/rootfs/system/lib64 REQUIRED)
find_library(CXX_SHARED_LIB c++_shared PATHS ${CMAKE_SOURCE_DIR}/../bin/rootfs/system/lib64 REQUIRED)


# Link libraries
target_link_libraries(amwrapper
    ${CXX_SHARED_LIB}
    ${ANDROIDAPPMUSIC_LIB}
    ${STORESERVICESCORE_LIB}
    ${MEDIAPLATFORM_LIB}
)


link_directories(${CMAKE_SOURCE_DIR}/../bin/rootfs/system/lib64)

#ExternalProject_Add(
#    wrapper
#    PREFIX ${CMAKE_BINARY_DIR}
#    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
#    CONFIGURE_COMMAND ""
#    BUILD_COMMAND ${C_COMPILER} -O3 -Wall -o wrapper ${WRAPPER_SOURCE}
#    BUILD_IN_SOURCE 1
#    INSTALL_COMMAND ""
#    DEPENDS amwrapper
#)