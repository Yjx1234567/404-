cmake_minimum_required(VERSION 3.10)
project(OJSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# MySQL
find_package(MySQL)
if(NOT MySQL_FOUND)
    find_library(MYSQL_LIBRARY mysqlclient PATHS /usr/lib/x86_64-linux-gnu /usr/lib)
    find_path(MYSQL_INCLUDE_DIR mysql/mysql.h PATHS /usr/include/mysql /usr/include)
    if(MYSQL_LIBRARY AND MYSQL_INCLUDE_DIR)
        set(MySQL_FOUND TRUE)
    endif()
endif()

# yaml-cpp
find_package(yaml-cpp REQUIRED)

# jsoncpp
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)
if(NOT JSONCPP_FOUND)
    find_library(JSONCPP_LIBRARY jsoncpp PATHS /usr/lib/x86_64-linux-gnu)
    find_path(JSONCPP_INCLUDE_DIR json/json.h PATHS /usr/include/jsoncpp)
    if(JSONCPP_LIBRARY AND JSONCPP_INCLUDE_DIR)
        set(JSONCPP_FOUND TRUE)
        set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR})
        set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
    endif()
endif()

# Botan (for bcrypt)
find_package(PkgConfig REQUIRED)
pkg_check_modules(BOTAN botan-2)
if(NOT BOTAN_FOUND)
    find_library(BOTAN_LIBRARY botan-2 PATHS /usr/lib/x86_64-linux-gnu)
    find_path(BOTAN_INCLUDE_DIR botan/botan.h PATHS /usr/include/botan-2)
    if(BOTAN_LIBRARY AND BOTAN_INCLUDE_DIR)
        set(BOTAN_FOUND TRUE)
        set(BOTAN_INCLUDE_DIRS ${BOTAN_INCLUDE_DIR})
        set(BOTAN_LIBRARIES ${BOTAN_LIBRARY})
    endif()
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/third_party)
if(MySQL_FOUND)
    include_directories(${MYSQL_INCLUDE_DIR})
endif()
if(JSONCPP_FOUND)
    include_directories(${JSONCPP_INCLUDE_DIRS})
endif()
if(BOTAN_FOUND)
    include_directories(${BOTAN_INCLUDE_DIRS})
endif()

# Source files
set(SOURCES
    src/main.cc
    src/server/server.cc
    src/server/router.cc
    src/handler/problem_handler.cc
    src/handler/submission_handler.cc
    src/handler/auth_handler.cc
    src/handler/admin_handler.cc
    src/handler/class_handler.cc
    src/service/problem_service.cc
    src/service/executor_service.cc
    src/service/auth_service.cc
    src/service/judge_queue.cc
    src/service/rate_limiter.cc
    src/model/problem.cc
    src/model/test_case.cc
    src/model/submission.cc
    src/model/user.cc
    src/model/session.cc
    src/db/connection_pool.cc
    src/utils/logger.cc
    src/utils/config.cc
    src/utils/bcrypt.cc
)

# Main server executable
add_executable(oj_server ${SOURCES})

target_link_libraries(oj_server
    Threads::Threads
    yaml-cpp
)

if(MySQL_FOUND)
    target_link_libraries(oj_server ${MYSQL_LIBRARY})
endif()

if(JSONCPP_FOUND)
    target_link_libraries(oj_server ${JSONCPP_LIBRARIES})
endif()

if(BOTAN_FOUND)
    target_link_libraries(oj_server ${BOTAN_LIBRARIES})
endif()

# Add static content
set_target_properties(oj_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Google Test
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    find_package(GTest)
    if(NOT GTest_FOUND)
        find_library(GTEST_LIBRARY gtest PATHS /usr/lib/x86_64-linux-gnu)
        find_library(GTEST_MAIN_LIBRARY gtest_main PATHS /usr/lib/x86_64-linux-gnu)
        find_path(GTEST_INCLUDE_DIR gtest/gtest.h PATHS /usr/include)
        if(GTEST_LIBRARY AND GTEST_MAIN_LIBRARY AND GTEST_INCLUDE_DIR)
            set(GTest_FOUND TRUE)
        endif()
    endif()

    if(GTest_FOUND)
        enable_testing()
        include_directories(${GTEST_INCLUDE_DIR})

        # CMake 3.22 的 find_package(GTest) 可能走 CONFIG 模式（Ubuntu 22.04），
        # 此时只提供 GTest::gtest 目标、不设置 GTEST_LIBRARY 变量，两种情况都兼容
        if(TARGET GTest::gtest AND TARGET GTest::gtest_main)
            set(OJ_GTEST_LIBS GTest::gtest GTest::gtest_main)
        else()
            set(OJ_GTEST_LIBS ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
        endif()

        # Pure HTTP header/cookie policy tests (no database or live server).
        add_executable(http_policy_test tests/unit/http_policy_test.cc)
        target_link_libraries(http_policy_test ${OJ_GTEST_LIBS} Threads::Threads)
        add_test(NAME http_policy_test COMMAND http_policy_test)

        # Config test
        add_executable(config_test
            tests/unit/config_test.cc
            src/utils/config.cc
            src/utils/logger.cc
        )
        target_link_libraries(config_test ${OJ_GTEST_LIBS} Threads::Threads yaml-cpp)
        add_test(NAME config_test COMMAND config_test)

        # Logger test
        add_executable(logger_test
            tests/unit/logger_test.cc
            src/utils/logger.cc
        )
        target_link_libraries(logger_test ${OJ_GTEST_LIBS} Threads::Threads)
        add_test(NAME logger_test COMMAND logger_test)

        # Connection pool test
        add_executable(connection_pool_test
            tests/unit/connection_pool_test.cc
            src/db/connection_pool.cc
            src/utils/config.cc
            src/utils/logger.cc
        )
        target_link_libraries(connection_pool_test
            ${OJ_GTEST_LIBS}
            Threads::Threads
            yaml-cpp
        )
        if(MySQL_FOUND)
            target_link_libraries(connection_pool_test ${MYSQL_LIBRARY})
        endif()
        add_test(NAME connection_pool_test COMMAND connection_pool_test)

        # Problem test
        add_executable(problem_test
            tests/unit/problem_test.cc
            src/model/problem.cc
            src/model/test_case.cc
            src/db/connection_pool.cc
            src/utils/config.cc
            src/utils/logger.cc
        )
        target_link_libraries(problem_test
            ${OJ_GTEST_LIBS}
            Threads::Threads
            yaml-cpp
        )
        if(MySQL_FOUND)
            target_link_libraries(problem_test ${MYSQL_LIBRARY})
        endif()
        add_test(NAME problem_test COMMAND problem_test)

        # Executor test
        add_executable(executor_test
            tests/unit/executor_test.cc
            src/service/executor_service.cc
            src/model/problem.cc
            src/model/test_case.cc
            src/db/connection_pool.cc
            src/utils/config.cc
            src/utils/logger.cc
        )
        target_link_libraries(executor_test
            ${OJ_GTEST_LIBS}
            Threads::Threads
            yaml-cpp
        )
        if(MySQL_FOUND)
            target_link_libraries(executor_test ${MYSQL_LIBRARY})
        endif()
        add_test(NAME executor_test COMMAND executor_test)

        # Auth test
        add_executable(auth_test
            tests/unit/auth_test.cc
            src/utils/bcrypt.cc
            src/model/user.cc
            src/model/session.cc
            src/db/connection_pool.cc
            src/utils/config.cc
            src/utils/logger.cc
            src/service/auth_service.cc
        )
        target_link_libraries(auth_test
            ${OJ_GTEST_LIBS}
            Threads::Threads
            yaml-cpp
        )
        if(MySQL_FOUND)
            target_link_libraries(auth_test ${MYSQL_LIBRARY})
        endif()
        if(BOTAN_FOUND)
            target_link_libraries(auth_test ${BOTAN_LIBRARIES})
        endif()
        add_test(NAME auth_test COMMAND auth_test)
    endif()
endif()

# Create admin user tool
add_executable(create_admin_user
    tools/create_admin_user.cc
    src/utils/bcrypt.cc
    src/model/user.cc
    src/db/connection_pool.cc
    src/utils/config.cc
    src/utils/logger.cc
)
target_link_libraries(create_admin_user
    Threads::Threads
    yaml-cpp
)
if(MySQL_FOUND)
    target_link_libraries(create_admin_user ${MYSQL_LIBRARY})
endif()
if(BOTAN_FOUND)
    target_link_libraries(create_admin_user ${BOTAN_LIBRARIES})
endif()

# DB reset tool
add_executable(db_reset_tool
    tools/db_reset_tool.cc
    src/model/problem.cc
    src/model/test_case.cc
    src/model/user.cc
    src/db/connection_pool.cc
    src/utils/config.cc
    src/utils/logger.cc
)
target_link_libraries(db_reset_tool
    Threads::Threads
    yaml-cpp
)
if(MySQL_FOUND)
    target_link_libraries(db_reset_tool ${MYSQL_LIBRARY})
endif()
if(JSONCPP_FOUND)
    target_link_libraries(db_reset_tool ${JSONCPP_LIBRARIES})
endif()
