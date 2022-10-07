ADD_EXECUTABLE(linux_test main.c)

SET(TOOLCHAIN_NAME "$ENV{SYSTEM_NAME}")


LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

ADD_DEPENDENCIES(linux_test examples_static quark_static)
TARGET_LINK_LIBRARIES(linux_test examples_static quark_static device ans network mqtt http driver rcutils system crypt md5 base64 json logs m -lpthread -lssl)

INSTALL(TARGETS linux_test DESTINATION ${INSTALL_DIR}/bin)

# 5.Create module install target
ADD_MODULE_INSTALL(linux_test_install)
