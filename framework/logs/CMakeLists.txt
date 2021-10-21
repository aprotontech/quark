PROJECT(LOGS)

SET(TOOLCHAIN_NAME "$ENV{SYSTEM_NAME}")

# 1.Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(logs_shared SHARED ${LIB_SRC_LIST})
ADD_OUTPUT_LIBRARY(logs_static STATIC ${LIB_SRC_LIST})

IF(${TOOLCHAIN_NAME} MATCHES ANDROID)
    ADD_TARGET_LINK_LIBRARIES(logs_shared log)
    ADD_TARGET_LINK_LIBRARIES(logs_static log)
ENDIF(${TOOLCHAIN_NAME} MATCHES ANDROID)

# 3.Set properties
SET_MODULE_PROPERTIES(logs_shared PROPERTIES OUTPUT_NAME "logs")
SET_MODULE_PROPERTIES(logs_static PROPERTIES OUTPUT_NAME "logs")

# 4.Install executable,libraries,include,doc,demo source code etc.
#INSTALL(TARGETS logs_shared DESTINATION ${INSTALL_DIR}/lib/shared)
#INSTALL(TARGETS logs_static DESTINATION ${INSTALL_DIR}/lib/static)
#INSTALL(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(logs_install)
