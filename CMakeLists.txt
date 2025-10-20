cmake_minimum_required(VERSION 3.22)

set(GENESURFER "GeneSurfer")
PROJECT(${GENESURFER} LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake options
# -----------------------------------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DWIN32 /EHsc /MP /permissive- /Zc:__cplusplus")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

find_package(Qt6 COMPONENTS Widgets WebEngineWidgets OpenGL OpenGLWidgets REQUIRED)

find_package(ManiVault COMPONENTS Core PointData ClusterData CONFIG QUIET)

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    message(STATUS "Found OpenMP")
endif()


# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
set(PLUGIN
    src/GeneSurferPlugin.h
    src/GeneSurferPlugin.cpp
    src/ChartWidget.h
    src/ChartWidget.cpp
	src/ScatterView.h
    src/ScatterView.cpp
	src/TableWidget.h
    src/TableWidget.cpp
	PluginInfo.json
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
	src/Actions/EnrichmentAction.cpp
	src/Actions/EnrichmentAction.h
	src/Actions/SectionAction.cpp
	src/Actions/SectionAction.h
)


set(WEB
    res/chart/bar_chart.html
    res/chart/bar_chart.tools.js
	res/chart/d3.v4.js
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

# -----------------------------------------------------------------------------
# CMake Target
# -----------------------------------------------------------------------------
add_library(${GENESURFER} SHARED ${PLUGIN} ${Compute} ${Actions} ${AUX} ${WEB} ${RESOURCE_FILES})

# -----------------------------------------------------------------------------
# Target include directories
# -----------------------------------------------------------------------------
target_include_directories(${GENESURFER} PRIVATE "${ManiVault_INCLUDE_DIR}")

# Include the fastcluster source file
target_sources(GeneSurfer PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/hclust/fastcluster.cpp)

target_include_directories(${GENESURFER} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/Eigen/include)
target_include_directories(${GENESURFER} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/hclust)

# -----------------------------------------------------------------------------
# Target properties
# -----------------------------------------------------------------------------
target_compile_features(${GENESURFER} PRIVATE cxx_std_20)

if(MV_UNITY_BUILD)
    set_target_properties(${GENESURFER} PROPERTIES UNITY_BUILD ON)
endif()

# -----------------------------------------------------------------------------
# Target library linking
# -----------------------------------------------------------------------------
target_link_libraries(${GENESURFER} PRIVATE Qt6::Widgets)
target_link_libraries(${GENESURFER} PRIVATE Qt6::WebEngineWidgets)
target_link_libraries(${GENESURFER} PRIVATE Qt6::OpenGL)
target_link_libraries(${GENESURFER} PRIVATE Qt6::OpenGLWidgets)

target_link_libraries(${GENESURFER} PRIVATE ManiVault::Core)
target_link_libraries(${GENESURFER} PRIVATE ManiVault::PointData)
target_link_libraries(${GENESURFER} PRIVATE ManiVault::ClusterData)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${GENESURFER} PRIVATE OpenMP::OpenMP_CXX)
endif()

# -----------------------------------------------------------------------------
# Target installation
# -----------------------------------------------------------------------------
install(TARGETS ${GENESURFER}
    RUNTIME DESTINATION Plugins COMPONENT PLUGINS # Windows .dll
    LIBRARY DESTINATION Plugins COMPONENT PLUGINS # Linux/Mac .so
)

add_custom_command(TARGET ${GENESURFER} POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
        --install ${CMAKE_CURRENT_BINARY_DIR}
        --config $<CONFIGURATION>
        --prefix ${ManiVault_INSTALL_DIR}/$<CONFIGURATION>
)

mv_handle_plugin_config(${GENESURFER})


# -----------------------------------------------------------------------------
# Miscellaneous
# -----------------------------------------------------------------------------
# Automatically set the debug environment (command + working directory) for MSVC
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set_property(TARGET ${GENESURFER} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<IF:$<CONFIG:DEBUG>,${ManiVault_INSTALL_DIR}/Debug,$<IF:$<CONFIG:RELWITHDEBINFO>,${ManiVault_INSTALL_DIR}/RelWithDebInfo,${ManiVault_INSTALL_DIR}/Release>>)
    set_property(TARGET ${GENESURFER} PROPERTY VS_DEBUGGER_COMMAND $<IF:$<CONFIG:DEBUG>,"${ManiVault_INSTALL_DIR}/Debug/ManiVault Studio.exe",$<IF:$<CONFIG:RELWITHDEBINFO>,"${ManiVault_INSTALL_DIR}/RelWithDebInfo/ManiVault Studio.exe","${ManiVault_INSTALL_DIR}/Release/ManiVault Studio.exe">>)
endif()
