cmake_minimum_required(VERSION 3.17)

set(EXAMPLEVIEWJS "ExampleViewJS")
PROJECT(${EXAMPLEVIEWJS})

# -----------------------------------------------------------------------------
# Library versions for Artifactory sourced binaries
# -----------------------------------------------------------------------------
set(HDILib_VERSION 1.2.6)

if(NOT USE_ARTIFACTORY_LIBS)
    set(USE_ARTIFACTORY_LIBS TRUE CACHE BOOL "Use the prebuilt libraries from artifactory")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DWIN32 /EHsc /MP /permissive- /Zc:__cplusplus")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
endif(MSVC)


# Check if the directory to the ManiVault installation has been provided
if(NOT DEFINED MV_INSTALL_DIR)
    set(MV_INSTALL_DIR "" CACHE PATH "Directory where ManiVault is installed")
    message(FATAL_ERROR "Please set MV_INSTALL_DIR to the directory where ManiVault is installed")
endif()
file(TO_CMAKE_PATH ${MV_INSTALL_DIR} MV_INSTALL_DIR)

find_package(Qt6 COMPONENTS Widgets WebEngineWidgets OpenGL OpenGLWidgets REQUIRED)

# Define the plugin sources
set(PLUGIN
    src/ExampleViewJSPlugin.h
    src/ExampleViewJSPlugin.cpp
    src/ChartWidget.h
    src/ChartWidget.cpp
    src/ExampleViewJSPlugin.json
	src/ScatterView.h
    src/ScatterView.cpp
	src/TableWidget.h
    src/TableWidget.cpp
)

set(Compute
    src/Compute/fastcluster.cpp
    src/Compute/fastcluster.h
    src/Compute/fastcluster_dm.h
    src/Compute/fastcluster_R_dm.h
    src/Compute/DataMatrix.cpp
    src/Compute/DataMatrix.h
    src/Compute/DataStore.cpp
    src/Compute/DataStore.h
    src/Compute/DataTransformations.cpp
    src/Compute/DataTransformations.h
    src/Compute/EnrichmentAnalysis.cpp
    src/Compute/EnrichmentAnalysis.h
	src/Compute/CorrFilter.cpp
    src/Compute/CorrFilter.h
	src/Compute/DataSubset.cpp
    src/Compute/DataSubset.h
)

set(Actions
    src/Actions/SettingsAction.cpp
    src/Actions/SettingsAction.h
	src/Actions/CorrelationModeAction.cpp
    src/Actions/CorrelationModeAction.h
	src/Actions/PointPlotAction.cpp
    src/Actions/PointPlotAction.h
	src/Actions/PositionAction.cpp
    src/Actions/PositionAction.h
	src/Actions/SingleCellModeAction.cpp
    src/Actions/SingleCellModeAction.h
	src/Actions/ClusteringAction.cpp
    src/Actions/ClusteringAction.h
	src/Actions/DimensionSelectionAction.cpp
    src/Actions/DimensionSelectionAction.h
	src/Actions/GenePickerAction.cpp
	src/Actions/GenePickerAction.h
)


set(WEB
    res/example_chart/bar_chart.html
    res/example_chart/bar_chart.tools.js
    res/js_libs/d3.v3.min.js
    res/js_libs/qwebchannel.tools.js
)

set(AUX
    res/example_chart.qrc
)

qt6_add_resources(RESOURCE_FILES res/example_chart.qrc)


source_group(Plugin FILES ${PLUGIN})
source_group(Compute FILES ${Compute})
source_group(Actions FILES ${Actions})
source_group(Web FILES ${WEB})
source_group(Aux FILES ${AUX})


# Create dynamic library for the plugin
add_library(${EXAMPLEVIEWJS} SHARED ${PLUGIN} ${Compute} ${Actions} ${AUX} ${WEB} ${RESOURCE_FILES})
set_target_properties(${EXAMPLEVIEWJS} PROPERTIES CXX_STANDARD 17)


set(LIBRARY_INSTALL_DIR ${PROJECT_BINARY_DIR})
include(InstallArtifactoryPackage)
if (USE_ARTIFACTORY_LIBS AND NOT JS_ARTIFACTORY_LIBS_INSTALLED) 
    message(STATUS "Installing artifactory packages to: ${LIBRARY_INSTALL_DIR}")
    # Both HDILib and flann are available prebuilt in the lkeb-artifactory as combined Debug/Release packages
    # lz4 is also available in the lkb-artifactory in separate Debug and |Release packages
    install_artifactory_package(HDILib ${HDILib_VERSION} biovault TRUE) 


    message(STATUS "module path ${CMAKE_MODULE_PATH}")
    message(STATUS "Call find_package HDILib search ${CMAKE_SOURCE_DIR}/HDILib/lib/cmake/HDILib")
    message(STATUS "Run find_package HDILib")

    set(JS_ARTIFACTORY_LIBS_INSTALLED TRUE CACHE BOOL "Use the prebuilt libraries from artifactory" FORCE)
endif()

if(USE_ARTIFACTORY_LIBS AND JS_ARTIFACTORY_LIBS_INSTALLED)
    # If the HDILib_VERSION is not a x.y.z version number 
    # omit the version argument from find package
    find_package(HDILib COMPONENTS hdidimensionalityreduction hdiutils hdidata PATHS "${LIBRARY_INSTALL_DIR}/HDILib" CONFIG REQUIRED)

    #HDILib include dir
    target_include_directories(${EXAMPLEVIEWJS} PRIVATE "${HDILib_INCLUDE_DIR}")

    # HDILib linking
    target_link_libraries(${EXAMPLEVIEWJS} PRIVATE ${HDILib_LINK_LIBS})
endif()

# Include mv headers, including system data plugins
target_include_directories(${EXAMPLEVIEWJS} PRIVATE "${MV_INSTALL_DIR}/$<CONFIGURATION>/include/")
target_include_directories(${EXAMPLEVIEWJS} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/Eigen/include)

# Request C++17
#target_compile_features(${EXAMPLEVIEWJS} PRIVATE cxx_std_17)

# Link to Qt libraries
target_link_libraries(${EXAMPLEVIEWJS} PRIVATE Qt6::Widgets)
target_link_libraries(${EXAMPLEVIEWJS} PRIVATE Qt6::WebEngineWidgets)
target_link_libraries(${EXAMPLEVIEWJS} PRIVATE Qt6::OpenGL)
target_link_libraries(${EXAMPLEVIEWJS} PRIVATE Qt6::OpenGLWidgets)

# Link to mv and data plugins
# The link path in this repo assumes that the ManiVault core was built locally
# in contrast to having been installed with an installer. In the latter case you'll have
# to adapt the MV_LINK_PATH and PLUGIN_LINK_PATH to your install folder
set(MV_LINK_PATH "${MV_INSTALL_DIR}/$<CONFIGURATION>/lib")
set(PLUGIN_LINK_PATH "${MV_INSTALL_DIR}/$<CONFIGURATION>/$<IF:$<CXX_COMPILER_ID:MSVC>,lib,Plugins>")
set(MV_LINK_SUFFIX $<IF:$<CXX_COMPILER_ID:MSVC>,${CMAKE_LINK_LIBRARY_SUFFIX},${CMAKE_SHARED_LIBRARY_SUFFIX}>)

set(MV_LINK_LIBRARY "${MV_LINK_PATH}/${CMAKE_SHARED_LIBRARY_PREFIX}MV_Public${MV_LINK_SUFFIX}")
set(POINTDATA_LINK_LIBRARY "${PLUGIN_LINK_PATH}/${CMAKE_SHARED_LIBRARY_PREFIX}PointData${MV_LINK_SUFFIX}") 
set(CLUSTERDATA_LINK_LIBRARY "${PLUGIN_LINK_PATH}/${CMAKE_SHARED_LIBRARY_PREFIX}ClusterData${MV_LINK_SUFFIX}") 

target_link_libraries(${EXAMPLEVIEWJS} PRIVATE "${MV_LINK_LIBRARY}")
target_link_libraries(${EXAMPLEVIEWJS} PRIVATE "${POINTDATA_LINK_LIBRARY}")
target_link_libraries(${EXAMPLEVIEWJS} PRIVATE "${CLUSTERDATA_LINK_LIBRARY}")

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${EXAMPLEVIEWJS} PRIVATE OpenMP::OpenMP_CXX)
endif()

# Install the shared plugin libary to the "Plugins" folder in the ManiVault install directory
install(TARGETS ${EXAMPLEVIEWJS}
    RUNTIME DESTINATION Plugins COMPONENT PLUGINS # Windows .dll
    LIBRARY DESTINATION Plugins COMPONENT PLUGINS # Linux/Mac .so
)

add_custom_command(TARGET ${EXAMPLEVIEWJS} POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        --install ${CMAKE_CURRENT_BINARY_DIR}
        --config $<CONFIGURATION>
        --prefix ${MV_INSTALL_DIR}/$<CONFIGURATION>
)

# Automatically set the debug environment (command + working directory) for MSVC
if(MSVC)
    set_property(TARGET ${EXAMPLEVIEWJS} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<IF:$<CONFIG:DEBUG>,${MV_INSTALL_DIR}/debug,${MV_INSTALL_DIR}/release>)
    set_property(TARGET ${EXAMPLEVIEWJS} PROPERTY VS_DEBUGGER_COMMAND $<IF:$<CONFIG:DEBUG>,"${MV_INSTALL_DIR}/debug/ManiVault Studio.exe","${MV_INSTALL_DIR}/release/ManiVault Studio.exe">)
endif()
