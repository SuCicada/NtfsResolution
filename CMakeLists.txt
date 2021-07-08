cmake_minimum_required(VERSION 3.16)
project(NtfsResolution)

set(CMAKE_CXX_STANDARD 14)

include_directories(myConsoleApplication)

add_executable(NtfsResolution
        myConsoleApplication/fat32.h
        myConsoleApplication/myConsoleApplication.cpp
        myConsoleApplication/myConsoleApplication.h
        myConsoleApplication/ntfs.h
        myConsoleApplication/stdafx.cpp
        myConsoleApplication/stdafx.h
        myConsoleApplication/targetver.h)
