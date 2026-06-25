cmake_minimum_required(VERSION 4.0.2)
project(BackUpTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(AWSSDK REQUIRED COMPONENTS s3)

add_executable(BackUpTool
        include/CliApp.h
        src/CliApp.cpp
        src/main.cpp
        include/PrintPath.h
        src/PrintPath.cpp
        include/Config.h
        include/AwsSdkSession.h
        include/Archiver.h
        include/Result.h
        src/Archiver.cpp
        include/S3Storage.h
        src/S3Storage.cpp
        src/BackupService.cpp
        src/BackupService.cpp
        src/BackupService.cpp
        src/BackupService.cpp
        src/BackupService.cpp
        include/BackupService.h
        src/BackupService.cpp
        src/main.cpp
)

target_include_directories(BackUpTool PRIVATE include)
target_link_libraries(BackUpTool PRIVATE ${AWSSDK_LINK_LIBRARIES})
target_compile_options(BackUpTool PRIVATE -Wall -Wextra)