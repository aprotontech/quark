PROJECT(SYSTEM)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(system_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(system_static logs_static)

ADD_TARGET_LINK_LIBRARIES(system_static logs)

# 3.Set properties
SET_MODULE_PROPERTIES(system_static PROPERTIES OUTPUT_NAME "system")


# 5.Create module install target
ADD_MODULE_INSTALL(system_install logs_install)
