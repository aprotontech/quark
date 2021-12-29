PROJECT(DRIVER)

# Add include and source
AUX_SOURCE_DIRECTORY(. LIB_SRC_LIST)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 1.Add include of libraries
INCLUDE_DIRECTORIES(${LOGS_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${SYSTEM_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${RCUTILS_SOURCE_DIR}/include)

# 2.Add library,dependencies,link libraries
ADD_OUTPUT_LIBRARY(driver_static STATIC ${LIB_SRC_LIST})

ADD_MODULE_DEPENDENCIES(driver_static system_static rcutils_static logs_static)

ADD_TARGET_LINK_LIBRARIES(driver_static system rcutils logs)

# 3.Set properties
SET_MODULE_PROPERTIES(driver_static PROPERTIES OUTPUT_NAME "driver")


# 5.Create module install target
ADD_MODULE_INSTALL(driver_install)
