PROJECT(EXAMPLES)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${QUARK_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${JSON_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${DEVICE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${HTTP_SOURCE_DIR})
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)

LINK_DIRECTORIES(${LIBRARY_OUTPUT_PATH})

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(examples_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(examples_static quark_static system_static logs_static)

ADD_TARGET_LINK_LIBRARIES(examples_static quark system logs)

# 3.Set properties
SET_MODULE_PROPERTIES(examples_static PROPERTIES OUTPUT_NAME "examples")

# 4.Install executable,libraries,include,doc,demo source code etc.
#INSTALL(TARGETS http_static DESTINATION ${INSTALL_DIR}/lib/static)
#INSTALL(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(examples_install)
