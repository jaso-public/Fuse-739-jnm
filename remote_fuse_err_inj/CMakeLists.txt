
add_library(remote_fuse_err_inj "remote_fuse.cc")

target_compile_options(remote_fuse_err_inj PUBLIC -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-unused-parameter)

target_link_libraries(remote_fuse_err_inj
        hw_grpc_proto
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(remote_fuse_err_inj PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
