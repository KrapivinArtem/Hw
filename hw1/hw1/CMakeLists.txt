cmake_minimum_required(VERSION 3.16)
project(hw1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Для GCC/Clang делаем Release-оптимизацию -O2 (как /O2 в MSVC), а не -O3 по умолчанию.
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Если конфигурация не задана (одноконфигурационные генераторы: Make, Ninja) — Debug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Заголовочная библиотека: mySort.hpp и Point.hpp лежат в include/
add_library(hw1_sort INTERFACE)
target_include_directories(hw1_sort INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# tests/  — корректность. Собирать в Debug, с AddressSanitizer.
# ---------------------------------------------------------------------------
add_executable(hw1_tests tests/test_sort.cpp)
target_link_libraries(hw1_tests PRIVATE hw1_sort)

# ---------------------------------------------------------------------------
# bench/  — производительность. Собирать в Release, БЕЗ санитайзеров.
# ---------------------------------------------------------------------------
add_executable(hw1_bench bench/bench_sort.cpp)
target_link_libraries(hw1_bench PRIVATE hw1_sort)

if(MSVC)
  # /utf-8 — исходники в UTF-8 (в комментариях русский текст); /W4 — много предупреждений.
  target_compile_options(hw1_tests PRIVATE /utf-8 /W4)
  target_compile_options(hw1_bench PRIVATE /utf-8 /W4)

  # AddressSanitizer в MSVC несовместим с /RTC1 и с /ZI (Edit and Continue),
  # которые CMake включает в Debug по умолчанию, — заменяем на совместимые.
  string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  target_compile_options(hw1_tests PRIVATE /fsanitize=address /Zi)
  target_link_options(hw1_tests PRIVATE /INCREMENTAL:NO)
else()
  target_compile_options(hw1_tests PRIVATE -Wall -Wextra -Wpedantic
                                          -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(hw1_tests PRIVATE -fsanitize=address,undefined)
  target_compile_options(hw1_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Предупреждение, если бенчмарк собирают не в Release.
add_custom_command(TARGET hw1_bench PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo
          "NOTE: hw1_bench must be built in Release mode, Debug timings are meaningless"
  VERBATIM)

# ctest: запускает тесты корректности.
enable_testing()
add_test(NAME hw1_tests COMMAND hw1_tests)
