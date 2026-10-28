cmake_minimum_required(VERSION 3.22)
project(warudo-calibrate VERSION 0.2.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This plugin supports Windows x64 only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

file(GLOB locale_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/locale/*.json")
set(locale_header "${CMAKE_CURRENT_BINARY_DIR}/generated/generated_locales.hpp")
add_custom_command(
    OUTPUT "${locale_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_locales.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/locale" "${locale_header}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_locales.py" ${locale_files}
    VERBATIM
)
add_custom_target(generate-locales DEPENDS "${locale_header}")

function(configure_plugin_target target)
    add_dependencies(${target} generate-locales)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX QT_NO_DEBUG)
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network kernel32 user32)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /permissive- /Zc:__cplusplus /W4)
    endif()
endfunction()

add_library(warudo-calibrate MODULE src/warudo-calibrate.cpp src/localization.hpp "${locale_header}")
configure_plugin_target(warudo-calibrate)
set_target_properties(warudo-calibrate PROPERTIES PREFIX "")
install(TARGETS warudo-calibrate LIBRARY DESTINATION obs-plugins/64bit RUNTIME DESTINATION obs-plugins/64bit)

include(CTest)
if(BUILD_TESTING)
    add_executable(warudo-calibrate-test tests/control_test.cpp "${locale_header}")
    configure_plugin_target(warudo-calibrate-test)
    add_test(NAME locales-and-placement COMMAND warudo-calibrate-test localization)
    add_test(NAME local-api-integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.py"
                "$<TARGET_FILE:warudo-calibrate-test>")
    set_tests_properties(locales-and-placement local-api-integration
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=minimal" TIMEOUT 90)
endif()
