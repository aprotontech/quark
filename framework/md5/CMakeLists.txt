PROJECT(MD5)

# 1.Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(md5_static STATIC ${LIB_SRC_LIST})

# 3.Set properties
SET_MODULE_PROPERTIES(md5_static PROPERTIES OUTPUT_NAME "md5")

# 4.Install executable,libraries,include,doc,demo source code etc.
INSTALL_OUTPUT(TARGETS md5_static DESTINATION ${INSTALL_DIR}/lib/static)
INSTALL_OUTPUT(DIRECTORY include DESTINATION ${INSTALL_DIR})

# 5.Create module install target
ADD_MODULE_INSTALL(md5_install)