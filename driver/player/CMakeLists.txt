PROJECT(PLAYER)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
#INCLUDE_DIRECTORIES(${openssl_BINARY_DIR}/ssl)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${HTTP_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(player_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(player_static http_static system_static logs_static)

ADD_TARGET_LINK_LIBRARIES(player_static http_static system logs rcutils)

# 3.Set properties
SET_MODULE_PROPERTIES(player_static PROPERTIES OUTPUT_NAME "player")


# 5.Create module install target
ADD_MODULE_INSTALL(player_install http_install logs_install md5_install)
