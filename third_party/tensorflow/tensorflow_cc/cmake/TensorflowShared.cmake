cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
include(ExternalProject)

externalproject_add(tensorflow_shared
                    DEPENDS
                    tensorflow_base
                    TMP_DIR
                    "/tmp"
                    STAMP_DIR
                    "tensorflow-stamp"
                    SOURCE_DIR
                    "tensorflow"
                    BUILD_IN_SOURCE
                    1
                    DOWNLOAD_COMMAND
                    ""
                    UPDATE_COMMAND
                    ""
                    CONFIGURE_COMMAND
                    tensorflow/contrib/makefile/compile_linux_protobuf.sh # pat
                                                                          # ch
                                                                          # nsy
                                                                          # nc
                                                                          # to
                                                                          # use
                                                                          # g++
                                                                          # -7
                    COMMAND
                    sed
                    -i
                    "s/ g++/ g++-7/g"
                    tensorflow/contrib/makefile/compile_nsync.sh
                    COMMAND
                    tensorflow/contrib/makefile/compile_nsync.sh
                    COMMAND
                    "${CMAKE_CURRENT_BINARY_DIR}/build_tensorflow.sh"
                    COMMAND
                    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_links.sh"
                    . # For some reason, Bazel sometimes puts the headers into
                      # `bazel-genfiles/genfiles` and sometimes just to `bazel-
                      # genfiles`. So we just create and include both the
                      # directories.
                    COMMAND
                    mkdir
                    -p
                    bazel-genfiles/genfiles
                    COMMAND
                    touch
                    bazel-genfiles/_placeholder.h
                    COMMAND
                    touch
                    bazel-genfiles/genfiles/_placeholder.h
                    BUILD_COMMAND
                    ""
                    INSTALL_COMMAND
                    "")
