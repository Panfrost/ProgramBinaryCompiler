project(Compiler)

set (SRCS Main.cpp)
set (LIBS EGL GLESv2 GLESv3)

add_executable(compiler ${SRCS})
target_link_libraries(compiler ${LIBS})

