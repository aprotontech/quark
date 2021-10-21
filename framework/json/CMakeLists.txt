PROJECT(JSON)

# 1.Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(json_shared SHARED ${LIB_SRC_LIST})
ADD_OUTPUT_LIBRARY(json_static STATIC ${LIB_SRC_LIST})

# 3.Set properties
SET_MODULE_PROPERTIES(json_shared PROPERTIES OUTPUT_NAME "json")
SET_MODULE_PROPERTIES(json_static PROPERTIES OUTPUT_NAME "json")

# 4.Install executable,libraries,include,doc,demo source code etc.
INSTALL_OUTPUT(TARGETS json_shared DESTINATION ${INSTALL_DIR}/lib/shared)
INSTALL_OUTPUT(TARGETS json_static DESTINATION ${INSTALL_DIR}/lib/static)
INSTALL_OUTPUT(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(json_install)