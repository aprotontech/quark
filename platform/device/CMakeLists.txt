PROJECT(DEVICE)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${HTTP_SOURCE_DIR})
INCLUDE_DIRECTORIES(${MD5_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${BASE64_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR}/)
INCLUDE_DIRECTORIES(${JSON_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(device_shared SHARED ${LIB_SRC_LIST})
ADD_OUTPUT_LIBRARY(device_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(device_shared http_shared logs_shared md5_shared json_shared)
ADD_MODULE_DEPENDENCIES(device_static http_static logs_static md5_static json_static)

ADD_TARGET_LINK_LIBRARIES(device_shared http logs md5 json rcutils)
ADD_TARGET_LINK_LIBRARIES(device_static http logs md5 json rcutils)

# 3.Set properties
SET_MODULE_PROPERTIES(device_shared PROPERTIES OUTPUT_NAME "device")
SET_MODULE_PROPERTIES(device_static PROPERTIES OUTPUT_NAME "device")

# 4.Install executable,libraries,include,doc,demo source code etc.
#INSTALL(TARGETS http_shared DESTINATION ${INSTALL_DIR}/lib/shared)
#INSTALL(TARGETS http_static DESTINATION ${INSTALL_DIR}/lib/static)
#INSTALL(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(device_install http_install logs_install md5_install)
