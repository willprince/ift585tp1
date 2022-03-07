cmake_minimum_required(VERSION 3.12.0)

project(Simulateur)

################################################################################
# Sub-projects
################################################################################
set(PROJECT_NAME Simulateur)

################################################################################
# Source groups
################################################################################
set(Headers
    "Computer/Computer.h"
    "Computer/Driver/Layer/DataType.h"
    "Computer/Driver/Layer/LinkLayer.h"
    "Computer/Driver/Layer/NetworkLayer.h"
    "Computer/Driver/Layer/LinkLayerLow.h"
    "Computer/Driver/NetworkDriver.h"
    "Computer/Hardware/NetworkInterfaceCard.h"
    "DataStructures/CircularQueue.h"
    "DataStructures/DataBuffer.h"
    "DataStructures/MACAddress.h"
    "DataStructures/Utils.h"
    "General/Configuration.h"
    "General/Logger.h"
    "General/Timer.h"
    "Transmission/Cable.h"
    "Transmission/Interferences.h"
    "Transmission/Transmission.h"
)
source_group("Fichiers header" FILES ${Headers})

set(Sources
    "Computer/Computer.cpp"
    "Computer/Driver/Layer/LinkLayer.cpp"
    "Computer/Driver/Layer/NetworkLayer.cpp"
    "Computer/Driver/Layer/LinkLayerLow.cpp"
    "Computer/Driver/NetworkDriver.cpp"
    "Computer/Hardware/NetworkInterfaceCard.cpp"
    "DataStructures/CircularQueue.cpp"
    "DataStructures/DataBuffer.cpp"
    "DataStructures/MACAddress.cpp"
    "General/Configuration.cpp"
    "General/Timer.cpp"
    "main.cpp"
    "Transmission/Cable.cpp"
    "Transmission/Interferences.cpp"
    "Transmission/Transmission.cpp"
)
source_group("Fichiers sources" FILES ${Sources})

set(ALL_FILES
    ${Headers}
    ${Sources}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY   "Output/")
