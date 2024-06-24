cmake_minimum_required(VERSION 3.17)

set(GENESURFER "GeneSurfer")
PROJECT(${GENESURFER})

# -----------------------------------------------------------------------------
# Library versions for Artifactory sourced binaries
# -----------------------------------------------------------------------------

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
    src/GeneSurferPlugin.h
    src/GeneSurferPlugin.cpp
    src/ChartWidget.h
    src/ChartWidget.cpp
    src/GeneSurferPlugin.json
	src/ScatterView.h
    src/ScatterView.cpp
	src/TableWidget.h
    src/TableWidget.cpp
)

set(Compute
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
	src/Actions/EnrichmentAction.cpp
	src/Actions/EnrichmentAction.h
	src/Actions/SectionAction.cpp
	src/Actions/SectionAction.h
)


set(WEB
    res/chart/bar_chart.html
    res/chart/bar_chart.tools.js
    res/js_libs/qwebchannel.tools.js
)

set(AUX
    res/genesurfer_resources.qrc
)

qt6_add_resources(RESOURCE_FILES res/genesurfer_resources.qrc)


source_group(Plugin FILES ${PLUGIN})
source_group(Compute FILES ${Compute})
source_group(Actions FILES ${Actions})
source_group(Web FILES ${WEB})
source_group(Aux FILES ${AUX})


# Create dynamic library for the plugin
add_library(${GENESURFER} SHARED ${PLUGIN} ${Compute} ${Actions} ${AUX} ${WEB} ${RESOURCE_FILES})
set_target_properties(${GENESURFER} PROPERTIES CXX_STANDARD 17)

# Include the fastcluster source file
target_sources(GeneSurfer PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/hclust/fastcluster.cpp)

# Include mv headers, including system data plugins
target_include_directories(${GENESURFER} PRIVATE "${MV_INSTALL_DIR}/$<CONFIGURATION>/include/")
target_include_directories(${GENESURFER} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/Eigen/include)
target_include_directories(${GENESURFER} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/hclust)

# Request C++17
#target_compile_features(${GENESURFER} PRIVATE cxx_std_17)

# Link to Qt libraries
target_link_libraries(${GENESURFER} PRIVATE Qt6::Widgets)
target_link_libraries(${GENESURFER} PRIVATE Qt6::WebEngineWidgets)
target_link_libraries(${GENESURFER} PRIVATE Qt6::OpenGL)
target_link_libraries(${GENESURFER} PRIVATE Qt6::OpenGLWidgets)

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

target_link_libraries(${GENESURFER} PRIVATE "${MV_LINK_LIBRARY}")
target_link_libraries(${GENESURFER} PRIVATE "${POINTDATA_LINK_LIBRARY}")
target_link_libraries(${GENESURFER} PRIVATE "${CLUSTERDATA_LINK_LIBRARY}")

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${GENESURFER} PRIVATE OpenMP::OpenMP_CXX)
endif()

# Install the shared plugin libary to the "Plugins" folder in the ManiVault install directory
install(TARGETS ${GENESURFER}
    RUNTIME DESTINATION Plugins COMPONENT PLUGINS # Windows .dll
    LIBRARY DESTINATION Plugins COMPONENT PLUGINS # Linux/Mac .so
)

add_custom_command(TARGET ${GENESURFER} POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        --install ${CMAKE_CURRENT_BINARY_DIR}
        --config $<CONFIGURATION>
        --prefix ${MV_INSTALL_DIR}/$<CONFIGURATION>
)

# Automatically set the debug environment (command + working directory) for MSVC
if(MSVC)
    set_property(TARGET ${GENESURFER} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<IF:$<CONFIG:DEBUG>,${MV_INSTALL_DIR}/debug,${MV_INSTALL_DIR}/release>)
    set_property(TARGET ${GENESURFER} PROPERTY VS_DEBUGGER_COMMAND $<IF:$<CONFIG:DEBUG>,"${MV_INSTALL_DIR}/debug/ManiVault Studio.exe","${MV_INSTALL_DIR}/release/ManiVault Studio.exe">)
endif()
