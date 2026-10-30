cmake_minimum_required(VERSION 4.4.5)

project(dm C)

file(GLOB SRC_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.c")

add_executable(dm ${SRC_FILES})

target_compile_features(dm PRIVATE c_std_90)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(dm
                PRIVATE
                -g
                -O0
                -fsanitize=address,undefined
                -Wall
                -Wextra
                -Wundef
                -Werror=return-type
                -Wmaybe-uninitialized
                -pedantic
        )
target_link_libraries(dm PRIVATE m curl -fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(dm
                PRIVATE
                -O3
                -DNDEBUG
                -Wall
                -Wextra
                -Wundef
                -Werror=return-type
                -Wmaybe-uninitialized
                -pedantic
        )
target_link_libraries(dm PRIVATE m curl)
endif()

target_include_directories(dm PRIVATE "${PROJECT_SOURCE_DIR}/include")
