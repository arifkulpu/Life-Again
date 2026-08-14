cmake_minimum_required(VERSION 3.21)
project(LifeAgain VERSION 1.0.0 LANGUAGES CXX)

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

target_include_directories(${PROJECT_NAME} PRIVATE
    include           # SKSEMenuFramework.h
    src               # FollowerTracker.h, MenuManager.h, Settings.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    fmt::fmt
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Skyrim AE / NG modu
    SKSE_SUPPORT_XBYAK=0
    UNICODE
    _UNICODE
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
