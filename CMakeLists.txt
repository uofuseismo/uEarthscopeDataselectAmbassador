cmake_minimum_required(VERSION 3.28)
project(uEarthscopeDataselectAmbassador VERSION 0.0.1 LANGUAGES CXX)
enable_testing()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_CONTAINER "Build container for deployment" OFF)
option(ENABLE_SSL "Enable SSL connections for the webserver backend" ON) 
option(ENABLE_COMPRESSION "Enable compression for the webserver backend" ON) 
option(WITH_CONAN "Build using Conan" OFF)
option(BUILD_TESTS "Compile unit tests" ON) 
option(USE_CLANG_TIDY "Build using clang-tidy" OFF)
include(GenerateExportHeader)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
if (NOT ${BUILD_SHARED_LIBS})
   set(Boost_USE_STATIC_LIBS ON) 
endif()

if (${WITH_CONAN})
   message("Will use Conan OTLP exporters")
   find_package(opentelemetry-cpp CONFIG COMPONENTS exporter_otlp_grpc exporters_otlp_http REQUIRED)
else()
   find_package(opentelemetry-cpp CONFIG COMPONENTS exporters_otlp_http REQUIRED)
endif()

find_package(spdlog REQUIRED)
find_package(Boost REQUIRED headers program_options REQUIRED)
find_package(Threads REQUIRED)
#find_package(libmseed REQUIRED)
find_package(Crow REQUIRED)
if (${ENABLE_COMPRESSION})
   set(CROW_ENABLE_COMPRESSION ON) 
else()
   set(CROW_ENABLE_COMPRESSION OFF)
endif()

if (${ENABLE_SSL})
   if (NOT BUILD_SHARED_LIBS)
      message("Will use static SSL libraries")
      set(OPENSSL_USE_STATIC_LIBS TRUE)
   endif()
   find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
else()
   if (NOT BUILD_SHARED_LIBS)
      message("Will use static SSL libraries")
      set(OPENSSL_USE_STATIC_LIBS TRUE)
   endif()
   find_package(OpenSSL COMPONENTS SSL Crypto)
endif()
if (${OPENSSL_FOUND})
   message("Found SSL version ${OPENSSL_VERSION}")
   if (${Crow_FOUND})
      set(CROW_ENABLE_SSL ON) 
   endif()
endif()
if (${BUILD_TESTS})
   find_package(Catch2 3 REQUIRED)
endif()

if (${USE_CLANG_TIDY} AND PROJECT_IS_TOP_LEVEL)
   message("Looking for clang-tidy")
   find_program(CLANG_TIDY_EXECUTABLE NAMES "clang-tidy")
   if (CLANG_TIDY_EXECUTABLE)
      message("Using clang-tidy  ${CLANG_TIDY_EXECUTABLE}")
      set(CMAKE_C_CLANG_TIDY   ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
      set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
      set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
   else()
      message("clang-tidy not found")
   endif()
endif()

find_package(Git)
if (Git_FOUND)
   execute_process(
     COMMAND ${GIT_EXECUTABLE} --no-pager log -n 1 HEAD "--pretty=format:%h"
     OUTPUT_VARIABLE uEventAnalysisToolBackend_GITTAG
     RESULT_VARIABLE gitresult
     ERROR_QUIET
     OUTPUT_STRIP_TRAILING_WHITESPACE
   )   
   if (gitresult EQUAL 0)
      message(STATUS "Last Git commit: ${uEventAnalysisToolBackend_GITTAG}")
   endif()
else()
   set(uEventAnalysisToolBackend_GITTAG "") 
endif()
configure_file(${CMAKE_SOURCE_DIR}/src/version.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/version.hpp)

set(HEADER_FILES 
    include/${PROJECT_NAME}/curlGlobal.hpp
    include/${PROJECT_NAME}/earthscopeClient.hpp
    include/${PROJECT_NAME}/earthscopeClientOptions.hpp
    include/${PROJECT_NAME}/metricsSingleton.hpp)

set(LIBRARY_SRC
    src/curlGlobal.cpp
    src/earthscopeClient.cpp
    src/earthscopeClientOptions.cpp
    src/metricsSingleton.cpp
    src/version.cpp)

if (${BUILD_SHARED_LIBS})
   add_library(libuEarthscopeDataselectAmbassador SHARED ${LIBRARY_SRC})
else()
   add_library(libuEarthscopeDataselectAmbassador STATIC ${LIBRARY_SRC})
endif()
add_library(uEarthscopeDataselectAmbassador::libuEarthscopeDataselectAmbassador ALIAS libuEarthscopeDataselectAmbassador)
set_target_properties(libuEarthscopeDataselectAmbassador PROPERTIES
                      OUTPUT_NAME uEarthscopeDataselectAmbassador
                      CXX_STANDARD 23
                      CXX_STANDARD_REQUIRED YES
                      CXX_EXTENSIONS NO)
target_include_directories(libuEarthscopeDataselectAmbassador
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                           PUBLIC  $<INSTALL_INTERFACE:>
                          )
target_link_libraries(libuEarthscopeDataselectAmbassador
                      PUBLIC spdlog::spdlog_header_only
                      PRIVATE Boost::headers
                              Boost::program_options
                              Threads::Threads
                              CURL::libcurl)
target_sources(libuEarthscopeDataselectAmbassador
               PUBLIC FILE_SET
                      HEADERS
                      BASE_DIRS
                         include
                         ${CMAKE_CURRENT_BINARY_DIR}/include
                      FILES
                         ${HEADER_FILES}
                         ${WILL_BE_DELETED_HEADER_FILES}
                         ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/version.hpp
               )

add_executable(uEarthscopeDataselectAmbassador
               src/main.cpp)
set_target_properties(uEarthscopeDataselectAmbassador PROPERTIES
                      CXX_STANDARD 23
                      CXX_STANDARD_REQUIRED YES
                      CXX_EXTENSIONS NO)
target_include_directories(uEarthscopeDataselectAmbassador
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                          )
target_link_libraries(uEarthscopeDataselectAmbassador
                      PRIVATE uEarthscopeDataselectAmbassador::libuEarthscopeDataselectAmbassador
                              Crow::Crow
                              Boost::headers Boost::program_options)
if (${WITH_CONAN})
   target_link_libraries(uEarthscopeDataselectAmbassador
                         PRIVATE opentelemetry-cpp::opentelemetry-cpp
                                 opentelemetry-cpp::proto_grpc
                                 opentelemetry-cpp::exporter_otlp_grpc
                                 opentelemetry-cpp::exporter_otlp_grpc_client
                                 opentelemetry-cpp::exporter_otlp_grpc_metrics
                                 opentelemetry-cpp::exporter_otlp_http
                                 opentelemetry-cpp::exporter_otlp_http_client
                                 opentelemetry-cpp::exporter_otlp_http_metric
                                 opentelemetry-cpp::exporter_otlp_http_log
                                 opentelemetry-cpp::metrics
                                 opentelemetry-cpp::common
                                 opentelemetry-cpp::api)
   target_compile_definitions(uEarthscopeDataselectAmbassador PRIVATE WITH_OTLP_GRPC)
else()
   target_link_libraries(uEarthscopeDataselectAmbassador
                         PRIVATE opentelemetry-cpp::otlp_http_log_record_exporter
                                 opentelemetry-cpp::otlp_http_metric_exporter)
endif()



