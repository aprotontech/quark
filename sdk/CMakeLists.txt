PROJECT(QUARK)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${DEVICE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${PLAYER_SOURCE_DIR})
INCLUDE_DIRECTORIES(${NETWORK_SOURCE_DIR})
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${ANS_SOURCE_DIR}/include)
#INCLUDE_DIRECTORIES(${ANS_SOURCE_DIR})
INCLUDE_DIRECTORIES(${MQTT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${DRIVER_SOURCE_DIR})
INCLUDE_DIRECTORIES(${HTTP_SOURCE_DIR})
INCLUDE_DIRECTORIES(${MD5_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${BASE64_SOURCE_DIR}/include)
#INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR}/)
INCLUDE_DIRECTORIES(${JSON_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(quark_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(quark_static driver_static player_static mqtt_static device_static ans_static network_static logs_static md5_static json_static rcutils_static system_static)

ADD_TARGET_LINK_LIBRARIES(quark_static driver_static ans_static mqtt_static device_static network_static player_static logs_static md5_static json_static rcutils_static system_static)

# 3.Set properties
SET_MODULE_PROPERTIES(quark_static PROPERTIES OUTPUT_NAME "quark")

# 5.Create module install target
ADD_MODULE_INSTALL(quark_install logs_install md5_install system_install)
