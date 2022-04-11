PROJECT(BASE64)

# 1.Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(base64_static STATIC ${LIB_SRC_LIST})

ADD_TARGET_LINK_LIBRARIES(base64_static)

# 3.Set propertie
SET_MODULE_PROPERTIES(base64_static PROPERTIES OUTPUT_NAME "base64")

# 4.Install executable,libraries,include,doc,demo source code etc.

# 5.Create module install target
ADD_MODULE_INSTALL(base64_install)
