cmake_minimum_required(VERSION 3.21)
project(polybolos_lattice_cpp_contested_bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(contested_core STATIC src/contested/track_engine.cpp)
target_include_directories(contested_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(MSVC)
  target_compile_options(contested_core PRIVATE /W4 /permissive-)
endif()

if(WIN32)
  add_library(anduril_lattice_rest STATIC
    vendor/anduril_lattice_rest/src/client.cpp
  )
  target_include_directories(anduril_lattice_rest PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/anduril_lattice_rest/include"
  )
  target_link_libraries(anduril_lattice_rest PUBLIC winhttp)
  if(MSVC)
    target_compile_options(anduril_lattice_rest PRIVATE /W4 /permissive-)
    target_compile_definitions(anduril_lattice_rest PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  add_executable(contested_bridge apps/contested_bridge.cpp)
  target_link_libraries(contested_bridge PRIVATE contested_core anduril_lattice_rest)
  add_executable(lattice_probe apps/lattice_probe.cpp)
  target_link_libraries(lattice_probe PRIVATE anduril_lattice_rest)
  add_executable(auth_resume_probe apps/auth_resume_probe.cpp)
  target_link_libraries(auth_resume_probe PRIVATE anduril_lattice_rest)
  if(MSVC)
    target_compile_options(contested_bridge PRIVATE /W4 /permissive-)
    target_compile_definitions(contested_bridge PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(lattice_probe PRIVATE /W4 /permissive-)
    target_compile_definitions(lattice_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(auth_resume_probe PRIVATE /W4 /permissive-)
    target_compile_definitions(auth_resume_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
else()
  message(STATUS "WinHTTP Lattice client skipped (Windows only). contested_core still builds.")
endif()
