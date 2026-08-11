cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(RazorLeak VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(RazorLeak VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(RAZORLEAK_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(RAZORLEAK_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(RAZORLEAK_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(RAZORLEAK_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")
set(RAZORLEAK_CREATE_AU OFF)
if(APPLE)
    set(RAZORLEAK_CREATE_AU ON)
endif()

add_library(razorleak_engine STATIC
    source/razorleak/RazorLeakEngine.cpp)
target_include_directories(razorleak_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(razorleak_engine PUBLIC cxx_std_20)
set_target_properties(razorleak_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(razorleak_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RAZORLEAK_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_library(razorleak_engine_standalone_tests STATIC
        source/razorleak/RazorLeakEngine.cpp
        source/razorleak/RazorLeakAuditionBridge.cpp)
    target_include_directories(razorleak_engine_standalone_tests PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include")
    target_compile_features(razorleak_engine_standalone_tests PUBLIC cxx_std_20)
    target_compile_definitions(razorleak_engine_standalone_tests PUBLIC
        YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
    set_target_properties(razorleak_engine_standalone_tests PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(razorleak_engine_standalone_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(razorleak_engine_tests
        tests/RazorLeakEngineTests.cpp)
    target_link_libraries(razorleak_engine_tests PRIVATE razorleak_engine_standalone_tests)
    target_compile_features(razorleak_engine_tests PRIVATE cxx_std_20)
    target_compile_definitions(razorleak_engine_tests PRIVATE
        RAZORLEAK_HEADLESS_BRIDGE_TEST=1
        YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(razorleak_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(razorleak_engine_tests)
    add_test(NAME razorleak_engine_tests COMMAND razorleak_engine_tests)

    add_executable(razorleak_hosted_compile_tests
        tests/RazorLeakHostedCompileTests.cpp)
    target_link_libraries(razorleak_hosted_compile_tests PRIVATE razorleak_engine)
    target_compile_features(razorleak_hosted_compile_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(razorleak_hosted_compile_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(razorleak_hosted_compile_tests)
    add_test(NAME razorleak_hosted_compile_tests COMMAND razorleak_hosted_compile_tests)
    add_test(NAME razorleak_hosted_no_audition_symbols
        COMMAND "${CMAKE_COMMAND}"
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DSOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}
            -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P "${CMAKE_CURRENT_LIST_DIR}/cmake/AssertNoAuditionSymbols.cmake")
endif()

if(RAZORLEAK_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${RAZORLEAK_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${RAZORLEAK_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME razorleak
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "RazorLeak"
        TARGET_APP_ID "jp.ehl.razorleak"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.razorleak"
        PLUGIN_NAME "RazorLeak"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Short bounded time-sheared leak delay glitch smear"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/RazorLeak"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${RAZORLEAK_CREATE_AU}
        PLUGIN_AU_SUBTYPE "RzLk"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(razorleak_shared INTERFACE
        source/razorleak/ParameterGridEditor.cpp
        source/razorleak/ParameterGridEditor.h
        source/razorleak/ProductState.h
        source/razorleak/plugins/RazorLeakPlugin.cpp
        source/razorleak/plugins/RazorLeakPlugin.h)
    target_link_libraries(razorleak_shared INTERFACE razorleak_engine)
    target_include_directories(razorleak_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    target_sources(razorleak_standalone_plugin PRIVATE
        source/razorleak/RazorLeakAuditionBridge.cpp)

    if(APPLE AND RAZORLEAK_ADHOC_SIGN)
        find_program(RAZORLEAK_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET razorleak_vst3_plugin POST_BUILD
            COMMAND "${RAZORLEAK_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:razorleak_vst3_plugin>"
            COMMENT "Ad-hoc signing RazorLeak VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET razorleak_standalone_plugin POST_BUILD
            COMMAND "${RAZORLEAK_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:razorleak_standalone_plugin>"
            COMMENT "Ad-hoc signing RazorLeak standalone app for local launch"
            VERBATIM)
    endif()
endif()
