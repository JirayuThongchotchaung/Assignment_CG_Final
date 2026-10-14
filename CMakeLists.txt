cmake_minimum_required(VERSION 3.20)

# Find vcpkg before project() so its toolchain is loaded on the first configure.
# Priority:
#   1. A valid CMAKE_TOOLCHAIN_FILE supplied explicitly
#   2. VCPKG_ROOT, if the environment variable is set
#   3. The starter's recommended install location:
#        Windows: %USERPROFILE%/vcpkg
#        macOS/Linux: $HOME/vcpkg
#
# This also repairs a stale cached toolchain path (for example
# /scripts/buildsystems/vcpkg.cmake from an earlier configure where
# VCPKG_ROOT was empty).
set(_detect_vcpkg_toolchain FALSE)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR CMAKE_TOOLCHAIN_FILE STREQUAL "")
  set(_detect_vcpkg_toolchain TRUE)
elseif(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
  message(STATUS "Cached/explicit toolchain file does not exist: ${CMAKE_TOOLCHAIN_FILE}")
  set(_detect_vcpkg_toolchain TRUE)
endif()

if(_detect_vcpkg_toolchain)
  set(_vcpkg_toolchain_candidates)

  if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      _vcpkg_from_env
    )
    list(APPEND _vcpkg_toolchain_candidates "${_vcpkg_from_env}")
  endif()

  if(CMAKE_HOST_WIN32 AND DEFINED ENV{USERPROFILE})
    file(TO_CMAKE_PATH
      "$ENV{USERPROFILE}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      _vcpkg_from_home
    )
    list(APPEND _vcpkg_toolchain_candidates "${_vcpkg_from_home}")
  elseif(DEFINED ENV{HOME})
    file(TO_CMAKE_PATH
      "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      _vcpkg_from_home
    )
    list(APPEND _vcpkg_toolchain_candidates "${_vcpkg_from_home}")
  endif()

  unset(_found_vcpkg_toolchain)
  foreach(_candidate IN LISTS _vcpkg_toolchain_candidates)
    if(EXISTS "${_candidate}")
      set(_found_vcpkg_toolchain "${_candidate}")
      break()
    endif()
  endforeach()

  if(_found_vcpkg_toolchain)
    set(CMAKE_TOOLCHAIN_FILE "${_found_vcpkg_toolchain}"
        CACHE FILEPATH "vcpkg toolchain file" FORCE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
  else()
    message(FATAL_ERROR
      "Could not find the vcpkg toolchain file.\n"
      "Install vcpkg in the recommended location, or set VCPKG_ROOT to your vcpkg folder.\n"
      "Windows recommended location: %USERPROFILE%/vcpkg\n"
      "macOS/Linux recommended location: $HOME/vcpkg"
    )
  endif()
endif()

project(Assignment2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)

# Everything is in one file (submitted as Assignment2_<studentID>.cpp).
# Shaders/Models/Textures are loaded with relative paths from the .exe folder.
add_executable(Assignment2
  src/main.cpp
)

target_include_directories(Assignment2 PRIVATE
  ${Stb_INCLUDE_DIR}
)

target_link_libraries(Assignment2 PRIVATE
  OpenGL::GL
  glfw
  GLEW::GLEW
  glm::glm
)

if(APPLE)
  # OpenGL is deprecated by Apple but remains available (up to OpenGL 4.1).
  target_compile_definitions(Assignment2 PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Copy Shaders, Models and Textures next to the executable so relative paths
# such as "Shaders/shader.vert" and "Models/cart/model.obj" work from there.
add_custom_target(copy_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/Shaders
          $<TARGET_FILE_DIR:Assignment2>/Shaders)
add_dependencies(Assignment2 copy_shaders)

add_custom_target(copy_models ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/Models
          $<TARGET_FILE_DIR:Assignment2>/Models)
add_dependencies(Assignment2 copy_models)

add_custom_target(copy_textures ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/Textures
          $<TARGET_FILE_DIR:Assignment2>/Textures)
add_dependencies(Assignment2 copy_textures)
