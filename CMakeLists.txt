cmake_minimum_required(VERSION 3.16)
project(filesystem-full-text-search-indexer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic --std=c++20")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG -Wall -march=x86-64 --std=c++20")
endif ()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install prefix" FORCE)
endif ()

find_package(Threads REQUIRED)

set(SOURCES
        # Main
        app/main.cpp

        # CliParser
        app/CliParser/CliParser.cpp

        # Config
        app/Config/Config.cpp

        # Helper
        app/Helper/Helper.cpp

        # Index
        app/Index/Index.cpp
        app/Index/Index_Add.cpp
        app/Index/Index_Last_Updated.cpp
        app/Index/Index_Lock.cpp
        app/Index/Index_Merge.cpp
        app/Index/Index_Search.cpp
        app/Index/Index_Transaction.cpp
        app/Index/Index_Check.cpp
        #Disk IO
        app/Index/DiskIO/DiskIO.cpp
        app/Index/DiskIO/DiskIO_Public_Getters.cpp
        app/Index/DiskIO/DiskIO_Additional.cpp
        app/Index/DiskIO/DiskIO_Reversed.cpp
        app/Index/DiskIO/DiskIO_Words.cpp
        app/Index/DiskIO/DiskIO_WordsF.cpp
        app/Index/DiskIO/DiskIO_Paths.cpp
        app/Index/DiskIO/DiskIO_PathsCount.cpp
        app/Index/DiskIO/DiskIO_Move.cpp

        # Indexer
        app/Indexer/Indexer.cpp

        # LocalIndex
        app/LocalIndex/LocalIndex.cpp

        # Logging
        app/Logging/Logger.cpp

        # Search
        app/Search/Search.cpp

)

set(HEADERS
        # Main headers
        app/index_config.h

        # CliParser
        app/CliParser/cliparser.h

        # Config
        app/Config/config.h

        # Helper
        app/Helper/helper.h

        # Index
        app/Index/index.h
        app/Index/index_types.h

        # Indexer
        app/Indexer/indexer.h
        app/Indexer/indexer_types.h

        # LocalIndex
        app/LocalIndex/localindex.h
        app/LocalIndex/localindex_types.h

        # Logging
        app/Logging/logging.h

        # Search
        app/Search/search.h

        # External libraries
        app/lib/mio.hpp
)

# All sources minus main.cpp — reused by the test binary so we avoid
# linking two definitions of main().
set(LIB_SOURCES ${SOURCES})
list(REMOVE_ITEM LIB_SOURCES app/main.cpp)

set(HEADERS
        app/index_config.h
        app/CliParser/cliparser.h
        app/Config/config.h
        app/Helper/helper.h
        app/Index/index.h
        app/Index/index_types.h
        app/Indexer/indexer.h
        app/Indexer/indexer_types.h
        app/LocalIndex/localindex.h
        app/LocalIndex/localindex_types.h
        app/Logging/logging.h
        app/Search/search.h
        app/lib/mio.hpp
)

add_library(indexer-lib STATIC ${LIB_SOURCES} ${HEADERS})
target_include_directories(indexer-lib PUBLIC app)
target_link_libraries(indexer-lib PUBLIC Threads::Threads)

add_executable(filesystem-indexer app/main.cpp)
target_link_libraries(filesystem-indexer PRIVATE indexer-lib)

set_target_properties(filesystem-indexer PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH TRUE
)

target_include_directories(filesystem-indexer
        PRIVATE
        app
)

target_link_libraries(filesystem-indexer
        PRIVATE
        Threads::Threads
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(filesystem-indexer PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    target_link_options(filesystem-indexer PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )

    # Release build optimizations
    target_compile_options(filesystem-indexer PRIVATE
            $<$<CONFIG:Release>:-flto>
    )
    target_link_options(filesystem-indexer PRIVATE
            $<$<CONFIG:Release>:-flto>
            $<$<CONFIG:Release>:-s>  # Strip symbols
    )
endif ()


# ─────────────────────────────────────────────────────────────────
# Unit tests (Google Test)
# ─────────────────────────────────────────────────────────────────
option(BUILD_UNIT_TESTS "Build the unit-test binary" ON)

if (BUILD_UNIT_TESTS)
    # Try to find an installed GTest first; fall back to FetchContent.
    find_package(GTest QUIET)

    if (NOT GTest_FOUND)
        message(STATUS "GTest not found locally — fetching via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG v1.14.0
        )
        # Prevent GTest from overriding our compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif ()

    enable_testing()

    add_executable(unit-tests
            tests/unit/test_diskio.cpp
    )

    target_include_directories(unit-tests PRIVATE app)

    target_link_libraries(unit-tests
            PRIVATE
            indexer-lib
            GTest::gtest
            GTest::gtest_main
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(unit-tests PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
                $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )
        target_link_options(unit-tests PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif ()

    # Register with CTest so `ctest` works too
    include(GoogleTest)
    gtest_discover_tests(unit-tests)

    message(STATUS "Unit tests: enabled (build target: unit-tests)")
else ()
    message(STATUS "Unit tests: disabled (pass -DBUILD_UNIT_TESTS=ON to enable)")
endif ()


message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug features: AddressSanitizer, UBSan enabled")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release optimizations: LTO, native arch, stripped symbols")
endif ()

install(TARGETS filesystem-indexer
        RUNTIME DESTINATION bin
)

install(FILES README.md
        DESTINATION share/doc/filesystem-full-text-search-indexer
)

install(DIRECTORY docs/
        DESTINATION share/doc/filesystem-full-text-search-indexer/docs
        FILES_MATCHING PATTERN "*.md"
)

install(FILES config.txt.example
        DESTINATION share/doc/filesystem-full-text-search-indexer
        OPTIONAL
)