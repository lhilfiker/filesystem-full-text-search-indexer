cmake_minimum_required(VERSION 3.16)
project(filesystem-full-text-search-indexer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG -Wall -march=x86-64")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install prefix" FORCE)
endif()

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

        # Indexer
        app/Indexer/Indexer.cpp

        # LocalIndex
        app/LocalIndex/LocalIndex.cpp

        # Logging
        app/Logging/Logger.cpp

        # Search
        app/Search/Search.cpp
        app/Index/Index_Check.cpp
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

add_executable(filesystem-indexer ${SOURCES} ${HEADERS})

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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
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
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug features: AddressSanitizer, UBSan enabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release optimizations: LTO, native arch, stripped symbols")
endif()

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