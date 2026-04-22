cmake_minimum_required(VERSION 3.16)
project(officeviewer VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include(FetchContent)
FetchContent_Declare(SeerSdk
    GIT_REPOSITORY https://github.com/ccseer/Seer-sdk.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SeerSdk)

# Oracle Outside In Technology SDK
set(OIT_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk")
include_directories(${OIT_SDK_DIR}/common)

add_library(officeviewer SHARED
    src/officeviewer.cpp
    src/officeviewer.h
    src/ui_oitviewer.h
    ${seersdk_SOURCE_DIR}/seer/viewerbase.h
    ${seersdk_SOURCE_DIR}/seer/viewerhelper.h
    ${seersdk_SOURCE_DIR}/seer/viewoption.h
)

target_include_directories(officeviewer PRIVATE
    ${seersdk_SOURCE_DIR}
)

target_link_libraries(officeviewer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    user32
)

set_target_properties(officeviewer PROPERTIES
    OUTPUT_NAME "officeviewer"
    SUFFIX ".dll"
)

target_compile_definitions(officeviewer PRIVATE
    COMPANY_NAME="1218.io"
    PRODUCT_NAME="Seer"
)

# Test executable
add_executable(test_officeviewer WIN32 src/test.cpp)
target_include_directories(test_officeviewer PRIVATE
    ${seersdk_SOURCE_DIR}
)
target_link_libraries(test_officeviewer PRIVATE
    officeviewer
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
