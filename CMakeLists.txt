cmake_minimum_required(VERSION 3.10)
project(TGBotWithDB)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(Boost_USE_MULTITHREADED ON)

set(BOOST_ROOT "C:/Users/Illya/source/repos/vcpkg/boost_1_82_0/boost_1_82_0")

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS)

link_directories(C:/Users/Illya/source/repos/vcpkg/packages/tgbot-cpp_x64-windows/lib)

find_package(SQLiteCpp CONFIG REQUIRED)

include_directories(C:/Users/Illya/source/repos/vcpkg/packages/tgbot-cpp_x64-windows/include/tgbot ${OPENSSL_INCLUDE_DIR} ${Boost_INCLUDE_DIR})
include_directories(C:/Users/Illya/source/repos/vcpkg/packages/sqlitecpp_x64-windows/include/SQLiteCpp)

set(CURL_LIBRARY "C:/Users/Illya/source/repos/vcpkg/packages/curl_x64-windows/lib")
set(CURL_INCLUDE_DIR "C:/Users/Illya/source/repos/vcpkg/packages/curl_x64-windows/include/curl")
find_package(CURL)
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    add_definitions(-DHAVE_CURL)
endif()

include_directories(${SQLiteCpp_INCLUDE_DIRS})

set(SOURCES TGBotWithDB.cpp TGBotWithDB.h)

add_executable(TGBotWithDB C:/Users/Illya/source/repos/TGBotWithDB/TGBotWithDB.cpp)

target_link_libraries(TGBotWithDB PRIVATE TgBot SQLiteCpp ${SQLiteCpp_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})