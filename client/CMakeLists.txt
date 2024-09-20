cmake_minimum_required(VERSION 3.5)

project(client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Weffc++ -Wstrict-aliasing -pedantic")

add_executable(client main.cpp)

include(GNUInstallDirs)
install(TARGETS client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
