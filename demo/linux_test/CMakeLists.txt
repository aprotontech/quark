ADD_EXECUTABLE(linux_test main.c)

SET(TOOLCHAIN_NAME "$ENV{SYSTEM_NAME}")


LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

ADD_DEPENDENCIES(linux_test examples_static)

string(COMPARE EQUAL "$ENV{CROSS_COMPILE}" "" IS_PC)
IF(IS_PC)
ADD_DEPENDENCIES(linux_test examples_static quark_static)
TARGET_LINK_LIBRARIES(linux_test examples quark device ans network mqtt paho-mqtt3c http driver rcutils system crypt md5 base64 json logs m -lpthread -lssl)
ELSE()
TARGET_LINK_LIBRARIES(linux_test examples quark device m -pthread -lstdc++ -ldl)
ADD_DEPENDENCIES(linux_test examples quark -pthread -lstdc++ -ldl)
ENDIF(IS_PC)

INSTALL(TARGETS linux_test DESTINATION ${INSTALL_DIR}/bin)

# 5.Create module install target
ADD_MODULE_INSTALL(linux_test_install)
