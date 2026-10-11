cmake_minimum_required(VERSION 3.20)
project(stalthos_domain_scenarios LANGUAGES CXX)
add_executable(stalthos_domain_scenarios scenarios.cpp)
target_compile_features(stalthos_domain_scenarios PRIVATE cxx_std_17)

add_executable(stalthos_domain_persistence persistence.cpp)
target_compile_features(stalthos_domain_persistence PRIVATE cxx_std_17)

if(MSVC)
  target_compile_options(stalthos_domain_scenarios PRIVATE /W4 /WX /permissive-)
  target_compile_options(stalthos_domain_persistence PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(stalthos_domain_scenarios PRIVATE -Wall -Wextra -Werror -pedantic)
  target_compile_options(stalthos_domain_persistence PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME stalthos_domain_scenarios COMMAND stalthos_domain_scenarios)
add_test(NAME stalthos_domain_persistence COMMAND stalthos_domain_persistence)
