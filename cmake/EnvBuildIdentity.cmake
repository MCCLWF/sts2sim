# Portable snapshots bind native callback symbols and numerical behavior to
# exactly these relative sources and this compiler/platform configuration.
file(GLOB_RECURSE STS2_IDENTITY_SOURCES CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  core/*.h core/*.cpp content/*.h content/*.cpp engine/*.h engine/*.cpp
  game/*.h game/*.cpp game/*.def env/*.h env/*.cpp)
list(APPEND STS2_IDENTITY_SOURCES CMakeLists.txt cmake/EnvBuildIdentity.cmake cmake/env_build_identity.h.in data/content.json)
list(SORT STS2_IDENTITY_SOURCES)
set(STS2_IDENTITY_INPUT "portable-codec=1\n")
foreach(STS2_IDENTITY_PATH IN LISTS STS2_IDENTITY_SOURCES)
  file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${STS2_IDENTITY_PATH}" STS2_IDENTITY_FILE_SHA)
  string(APPEND STS2_IDENTITY_INPUT "${STS2_IDENTITY_PATH}:${STS2_IDENTITY_FILE_SHA}\n")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/${STS2_IDENTITY_PATH}")
endforeach()
string(TOUPPER "${CMAKE_BUILD_TYPE}" STS2_IDENTITY_BUILD_TYPE)
string(APPEND STS2_IDENTITY_INPUT
  "${CMAKE_CXX_COMPILER_ID}:${CMAKE_CXX_COMPILER_VERSION}:${CMAKE_CXX_COMPILER_TARGET}\n"
  "${CMAKE_SYSTEM_NAME}:${CMAKE_SYSTEM_PROCESSOR}:${CMAKE_SIZEOF_VOID_P}\n"
  "${CMAKE_CXX_STANDARD}:${CMAKE_CXX_EXTENSIONS}:${CMAKE_BUILD_TYPE}\n"
  "${CMAKE_CXX_FLAGS}:${CMAKE_CXX_FLAGS_${STS2_IDENTITY_BUILD_TYPE}}\n"
  "${CMAKE_STATIC_LINKER_FLAGS}:${CMAKE_SHARED_LINKER_FLAGS}\n")
string(SHA256 STS2SIM_NATIVE_BUILD_SHA256 "${STS2_IDENTITY_INPUT}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/env_build_identity.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/env_build_identity.h" @ONLY)
target_include_directories(sts2env PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
