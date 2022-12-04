cmake_minimum_required(VERSION 3.10)

project(cpp-search-server CXX)
set(CMAKE_CXX_STANDARD 17)

set(SEARCH_SERVER_FILES
concurrent_map.h
document.cpp
document.h
log_duration.cpp
log_duration.h
main.cpp
paginator.h
process_queries.cpp
process_queries.h
read_input_functions.cpp
read_input_functions.h
remove_duplicates.cpp
remove_duplicates.h
request_queue.cpp
request_queue.h
search_server.cpp
search_server.h
string_processing.cpp
string_processing.h
test_example_functions.cpp
test_example_functions.h
)

add_executable(search-server ${SEARCH_SERVER_FILES})
