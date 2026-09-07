cmake_minimum_required(VERSION 3.21...4.0)
project(BackUpTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(AWSSDK REQUIRED COMPONENTS s3)

add_executable(BackUpTool
        src/main.cpp
        src/CliApp.cpp
        src/BackupService.cpp
        src/Archiver.cpp
        src/S3Storage.cpp
        src/PrintPath.cpp

        include/CliApp.h
        include/BackupService.h
        include/Archiver.h
        include/S3Storage.h
        include/AwsSdkSession.h
        include/Config.h
        include/PrintPath.h
        include/Result.h
)

target_compile_features(BackUpTool PRIVATE cxx_std_17)

target_include_directories(BackUpTool PRIVATE include)

target_link_libraries(BackUpTool PRIVATE ${AWSSDK_LINK_LIBRARIES})

target_compile_options(BackUpTool PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)