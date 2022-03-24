PROJECT(LOCATION)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
#INCLUDE_DIRECTORIES(${openssl_BINARY_DIR}/ssl)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${QUARK_SOURCE_DIR})
INCLUDE_DIRECTORIES(${QUARK_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${ANS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${NETWORK_SOURCE_DIR})
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${HTTP_SOURCE_DIR})
INCLUDE_DIRECTORIES(${MQTT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${DEVICE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${JSON_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(location_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(location_static quark_static http_static logs_static md5_static json_static)

ADD_TARGET_LINK_LIBRARIES(location_static quark http logs md5 json rcutils)

# 3.Set properties
SET_MODULE_PROPERTIES(location_static PROPERTIES OUTPUT_NAME "location")


# 5.Create module install target
ADD_MODULE_INSTALL(location_install http_install logs_install md5_install)
