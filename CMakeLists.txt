cmake_minimum_required(VERSION 3.21)
project(LifeAgain VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sadece AE (1.6.x) desteği açık, diğerleri kapalı - derlemeyi hızlandırır
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON  CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS      OFF CACHE BOOL "" FORCE)

# vcpkg ile gelen paketler
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# CommonLibSSE-NG alt projesi
add_subdirectory(extern/CommonLibSSE-NG)

# Kaynak dosyaları
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

# King Core is a soft runtime dependency. Its API header is needed at build
# time; allow callers to point at a sibling checkout or another installation.
set(KING_CORE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../King Core/include" CACHE PATH
    "Directory containing KingCore/KingCoreAPI.h")
if(NOT EXISTS "${KING_CORE_INCLUDE_DIR}/KingCore/KingCoreAPI.h")
    message(FATAL_ERROR
        "KingCoreAPI.h not found. Set KING_CORE_INCLUDE_DIR to the King Core include directory.")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    include           # SKSEMenuFramework.h
    src               # FollowerTracker.h, MenuManager.h, Settings.h
    "${KING_CORE_INCLUDE_DIR}"  # KingCoreAPI.h (King Core uyumluluğu)
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    fmt::fmt
)

# The OAR bridge is a separate, optional GPL-3.0 add-on. It is kept in its own
# target so its OAR API license does not change the LifeAgain plugin license.
option(BUILD_LIFEAGAIN_OAR_BRIDGE "Build the optional Open Animation Replacer bridge" OFF)
if(BUILD_LIFEAGAIN_OAR_BRIDGE)
    set(OAR_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/LifeAgainOARBridge")
    add_library(LifeAgainOARBridge SHARED
        "${OAR_BRIDGE_DIR}/src/main.cpp"
        "${OAR_BRIDGE_DIR}/include/OAR/OpenAnimationReplacer-ConditionTypes.cpp"
        "${OAR_BRIDGE_DIR}/include/OAR/OpenAnimationReplacerAPI-Conditions.cpp"
    )
    target_precompile_headers(LifeAgainOARBridge PRIVATE
        "${OAR_BRIDGE_DIR}/src/pch.h"
    )
    target_include_directories(LifeAgainOARBridge PRIVATE
        include
        "${OAR_BRIDGE_DIR}/include"
    )
    target_link_libraries(LifeAgainOARBridge PRIVATE
        CommonLibSSE::CommonLibSSE
        spdlog::spdlog
    )
    target_compile_definitions(LifeAgainOARBridge PRIVATE
        SKSE_SUPPORT_XBYAK=0
        UNICODE
        _UNICODE
        NOMINMAX
    )
    set_target_properties(LifeAgainOARBridge PROPERTIES
        PREFIX ""
        OUTPUT_NAME "LifeAgainOARBridge"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Skyrim AE / NG modu
    SKSE_SUPPORT_XBYAK=0
    UNICODE
    _UNICODE
    NOMINMAX
)

# DLL adı: LifeAgain.dll
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "LifeAgain"
)

# İsteğe bağlı: derleme sonrası DLL'i kopyala
# set(SKYRIM_PLUGINS_DIR "C:/Path/To/Skyrim/Data/SKSE/Plugins")
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${SKYRIM_PLUGINS_DIR}/LifeAgain.dll"
# )
