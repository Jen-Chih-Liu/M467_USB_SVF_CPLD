# CMakeList.txt : CMake project for M460, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("M460")
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
# Add source to this project's executable.
add_executable (M460 "M460.c" "cJSON.h" "cJSON.c" "mcu_isp.c" "cpld_svf.c" "usb_cmd.c"  "config.h" "cpld_svf.c")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET M460 PROPERTY CXX_STANDARD 20)
endif()

# ==============================================================================
# Libusb Configuration (Cross-Platform)
# ==============================================================================

if(WIN32)
    # --------------------------------------------------------------------------
    # Windows: Manual Configuration
    # --------------------------------------------------------------------------
    
    # 1. Set libusb root directory (relative to CMakeLists.txt location)
    # Ensure you have created the 'libusb' folder and placed the downloaded files inside.
    set(LIBUSB_ROOT_DIR "${CMAKE_SOURCE_DIR}/libusb")

    # 2. Set include path
    if(EXISTS "${LIBUSB_ROOT_DIR}/include")
        target_include_directories(M460 PRIVATE "${LIBUSB_ROOT_DIR}/include")
    else()
        message(FATAL_ERROR "Could not find libusb/include folder. Please ensure libusb is extracted into the 'libusb' folder in the project directory.")
    endif()

    # 3. Set specific paths for Lib and DLL
    # Note: Assuming 64-bit (MS64) and using VS2015-VS2022 compatible libraries.
    # If your folder name is different (e.g., VS2015), please modify the path below.
    set(LIB_PATH_SUFFIX "VS2019/MS64") 
    
    set(LIBUSB_LIB_FILE "${LIBUSB_ROOT_DIR}/${LIB_PATH_SUFFIX}/dll/libusb-1.0.lib")
    set(LIBUSB_DLL_FILE "${LIBUSB_ROOT_DIR}/${LIB_PATH_SUFFIX}/dll/libusb-1.0.dll")

    # 4. Link Library (.lib)
    if(EXISTS "${LIBUSB_LIB_FILE}")
        target_link_libraries(M460 PRIVATE "${LIBUSB_LIB_FILE}")
    else()
        message(FATAL_ERROR "Could not find .lib file: ${LIBUSB_LIB_FILE}. Please check path settings.")
    endif()

    # 5. [Crucial] Automatically copy DLL to exe output directory after build
    # This solves the "libusb-1.0.dll not found" error during runtime.
    add_custom_command(TARGET M460 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBUSB_DLL_FILE}"
        $<TARGET_FILE_DIR:M460>
        COMMENT "Copying libusb-1.0.dll to output directory..."
    )

else()
    # --------------------------------------------------------------------------
    # Linux: Automatic search using pkg-config
    # --------------------------------------------------------------------------
    find_package(PkgConfig REQUIRED)
    
    # Search for libusb-1.0
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

    # Add Include paths
    target_include_directories(M460 PRIVATE ${LIBUSB_INCLUDE_DIRS})
    
    # Link Libraries
    target_link_libraries(M460 PRIVATE ${LIBUSB_LIBRARIES})

endif()
