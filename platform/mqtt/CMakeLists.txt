PROJECT(MQTT)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
#INCLUDE_DIRECTORIES(${openssl_BINARY_DIR}/ssl)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${paho_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${JSON_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(mqtt_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(mqtt_static system_static logs_static json_static)

ADD_TARGET_LINK_LIBRARIES(mqtt_static paho-mqtt3c system rcutils logs json)

# 3.Set properties
SET_MODULE_PROPERTIES(mqtt_static PROPERTIES OUTPUT_NAME "mqtt")

# 4.Install executable,libraries,include,doc,demo source code etc.
#INSTALL(TARGETS mqtt_static DESTINATION ${INSTALL_DIR}/lib/static)
#INSTALL(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(mqtt_install logs_install md5_install)
