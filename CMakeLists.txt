cmake_minimum_required(VERSION 3.21)

project(c-octo VERSION 1.2 LANGUAGES C)

if(NOT WIN32)
  set(INSTALLDIR bin)
endif()

add_compile_definitions(VERSION="${PROJECT_VERSION}")

if(UNIX AND NOT APPLE)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_STANDARD_REQUIRED)

  add_compile_options(-lm)
endif()

if(MSVC)
  add_compile_options(/Wall) #MSVC uses /Wall as the highest warning level, includes /W4 and more. 
  #add_compile_options(/Wall /WX) # MSVC reports multiple errors that presumably other platforms don't, /WX is the equivalent to -Werror but means builds cannot complete under MSVC with an equivalent to the provided Makefile
else()
  add_compile_options(-Wall -Werror -Wextra)
endif()

if(APPLE)
  add_compile_options(-Wpedantic)
elseif(NOT MSVC)
  add_compile_options(-Wno-format-truncation)
endif()

add_executable(octo-cli src/octo_cli.c)

find_package(SDL2)

if(SDL2_FOUND)
  add_executable(octo-run src/octo_run.c)
  target_link_libraries(octo-run PRIVATE SDL2::SDL2 SDL2::SDL2main)
  add_executable(octo-de src/octo_de.c)
  target_link_libraries(octo-de PRIVATE SDL2::SDL2 SDL2::SDL2main)
  install(TARGETS octo-cli octo-run octo-de DESTINATION ${INSTALLDIR})
else()
  message("SDL2 could not be found, only Octo-cli will be built.")
  install(TARGETS octo-cli DESTINATION ${INSTALLDIR})
endif()
