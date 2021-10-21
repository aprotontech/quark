PROJECT(SYSTEM)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(system_shared SHARED ${LIB_SRC_LIST})
ADD_OUTPUT_LIBRARY(system_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(system_shared logs_shared)
ADD_MODULE_DEPENDENCIES(system_static logs_static)

ADD_TARGET_LINK_LIBRARIES(system_shared logs)
ADD_TARGET_LINK_LIBRARIES(system_static logs)

# 3.Set properties
SET_MODULE_PROPERTIES(system_shared PROPERTIES OUTPUT_NAME "system")
SET_MODULE_PROPERTIES(system_static PROPERTIES OUTPUT_NAME "system")

# 4.Install executable,libraries,include,doc,demo source code etc.
#INSTALL(TARGETS http_shared DESTINATION ${INSTALL_DIR}/lib/shared)
#INSTALL(TARGETS http_static DESTINATION ${INSTALL_DIR}/lib/static)
#INSTALL(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(system_install logs_install)
