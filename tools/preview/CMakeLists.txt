cmake_minimum_required(VERSION 3.20)
project(desk_assistant_ui_preview C CXX ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(LVGL_DIR ${REPO_ROOT}/managed_components/lvgl__lvgl)
set(UI_DIR ${REPO_ROOT}/components/ui)

# Mirror the LVGL settings of sdkconfig.defaults, except memory: PNG encoding needs the host heap,
# so the LVGL pool is measured on the device instead.
set(LV_CONF_SKIP ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_compile_definitions(
    LV_USE_OS=LV_OS_NONE
    LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB
    LV_USE_THEME_DEFAULT=0
    LV_FONT_MONTSERRAT_14=0
    LV_FONT_MONTSERRAT_20=1
    LV_FONT_MONTSERRAT_24=1
    LV_FONT_MONTSERRAT_28=1
    LV_FONT_DEFAULT=&lv_font_montserrat_24
    LV_USE_LODEPNG=1)
add_subdirectory(${LVGL_DIR} lvgl)

file(GLOB UI_SOURCES CONFIGURE_DEPENDS ${UI_DIR}/src/*.c ${UI_DIR}/src/fonts/*.c)
add_executable(ui_preview preview.c ${UI_SOURCES} ${REPO_ROOT}/components/board/src/display_geometry.c)
target_include_directories(ui_preview PRIVATE ${UI_DIR}/src ${REPO_ROOT}/components/board/include ${LVGL_DIR}/src/libs/lodepng)
target_compile_options(ui_preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(ui_preview PRIVATE lvgl m)
