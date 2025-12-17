vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/CommonLibVR
    REF ng
    SHA512 2a3ac6a21103b5ae4546ab3c572692420e85c4c67f92df62b40654fa9f50d5e74cb276c22eec7c6e5fc53c71181549b1c883d4525ba5a1daf3e415231382809a
    HEAD_REF ng
)

# Patch CMakeLists.txt to remove hardcoded OpenVR paths (vcpkg's OpenVR will be used automatically)
file(READ "${SOURCE_PATH}/CMakeLists.txt" _cmakelists)
string(REPLACE 
    "set(OPENVR_INCLUDE_DIR \${CMAKE_CURRENT_SOURCE_DIR}/extern/openvr/headers)\nset(OPENVR_LIB_DIR \${CMAKE_CURRENT_SOURCE_DIR}/extern/openvr/lib/win64)"
    "# OpenVR paths removed - using vcpkg's OpenVR"
    _cmakelists "${_cmakelists}")
string(REPLACE 
    "\"\$<BUILD_INTERFACE:\${OPENVR_INCLUDE_DIR}>\""
    ""
    _cmakelists "${_cmakelists}")
string(REPLACE
    "\"\$<\$<BOOL:\${ENABLE_SKYRIM_VR}>:\${OPENVR_LIB_DIR}/openvr_api.lib>\""
    ""
    _cmakelists "${_cmakelists}")
file(WRITE "${SOURCE_PATH}/CMakeLists.txt" "${_cmakelists}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DSKSE_SUPPORT_XBYAK=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME CommonLibSSE CONFIG_PATH lib/cmake/CommonLibSSE)

# Copy the helper CMake file that defines add_commonlibsse_plugin
file(COPY "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")

# Add missing find_dependency calls to the config file
file(READ "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSEConfig.cmake" _contents)
string(REPLACE "find_dependency(spdlog CONFIG)" 
    "find_dependency(spdlog CONFIG)\nfind_dependency(directxtk CONFIG)\nfind_dependency(xbyak CONFIG)" 
    _contents "${_contents}")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSEConfig.cmake" "${_contents}")

vcpkg_copy_pdbs()

file(GLOB HEADER_FILES "${SOURCE_PATH}/include/*")
file(COPY ${HEADER_FILES} DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
