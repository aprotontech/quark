PROJECT(HTTP)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
#INCLUDE_DIRECTORIES(${openssl_BINARY_DIR}/ssl)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(http_shared SHARED ${LIB_SRC_LIST})
ADD_OUTPUT_LIBRARY(http_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(http_shared system_shared logs_shared)
ADD_MODULE_DEPENDENCIES(http_static system_static logs_static)

ADD_TARGET_LINK_LIBRARIES(http_shared system logs rcutils)
ADD_TARGET_LINK_LIBRARIES(http_static system logs rcutils)

# 3.Set properties
SET_MODULE_PROPERTIES(http_shared PROPERTIES OUTPUT_NAME "http")
SET_MODULE_PROPERTIES(http_static PROPERTIES OUTPUT_NAME "http")

# 4.Install executable,libraries,include,doc,demo source code etc.
#INSTALL(TARGETS http_shared DESTINATION ${INSTALL_DIR}/lib/shared)
#INSTALL(TARGETS http_static DESTINATION ${INSTALL_DIR}/lib/static)
#INSTALL(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(http_install logs_install md5_install)
