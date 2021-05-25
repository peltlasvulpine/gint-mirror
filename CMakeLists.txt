# Build system for the gint unikernel

cmake_minimum_required(VERSION 3.15)
project(Gint VERSION 2.4.0 LANGUAGES C ASM)

include(GitVersionNumber)
include(Fxconv)

option(GINT_USER_VRAM "Put all VRAMs into the user stack (fx-CG 50 only)")
option(GINT_STATIC_GRAY "Use static memory instead of malloc for gray buffers (fx-9860G only)")
option(GINT_KMALLOC_DEBUG "Enable debug functions for kmalloc")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${FXSDK_COMPILER_INSTALL}" CACHE PATH "..." FORCE)
  set(LIBDIR ".")
  set(INCDIR "include")
else()
  set(LIBDIR "lib")
  set(INCDIR "include")
endif()

set(CMAKE_INSTALL_MESSAGE LAZY)

# Generate <gint/config.h> with commit hash, version name and options
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  git_version_number(SHORT_HASH "GINT_GIT_HASH" TAG_RELATIVE "GINT_GIT_VERSION")
else()
  set(GINT_GIT_HASH "")
  set(GINT_GIT_VERSION "${PROJECT_VERSION}")
endif()
configure_file(include/gint/config.h.in include/gint/config.h)

set(SOURCES_COMMON
  src/cpg/cpg.c
  src/cpu/atomic.c
  src/cpu/cpu.c
  src/cpu/registers.s
  src/cpu/sleep.c
  src/dma/dma.c
  src/dma/inth.s
  src/dma/memcpy.c
  src/dma/memset.c
  src/intc/intc.c
  src/intc/inth.s
  src/kernel/exch.c
  src/kernel/exch.s
  src/kernel/hardware.c
  src/kernel/inth.S
  src/kernel/kernel.c
  src/kernel/osmenu.c
  src/kernel/start.c
  src/kernel/syscalls.S
  src/kernel/tlbh.S
  src/kernel/world.c
  src/keysc/getkey.c
  src/keysc/iokbd.c
  src/keysc/keycodes.c
  src/keysc/keydev.c
  src/keysc/keysc.c
  src/kmalloc/arena_gint.c
  src/kmalloc/arena_osheap.c
  src/kmalloc/kmalloc.c
  src/kprint/kprint.c
  src/kprint/kformat_fp.c
  src/mmu/mmu.c
  src/render/dhline.c
  src/render/dimage.c
  src/render/dline.c
  src/render/dprint.c
  src/render/drect_border.c
  src/render/dtext.c
  src/render/dvline.c
  src/render/topti.c
  src/rtc/rtc.c
  src/rtc/rtc_ticks.c
  src/spu/spu.c
  src/std/aprint.c
  src/std/malloc.c
  src/std/print.c
  src/tmu/inth-etmu.s
  src/tmu/inth-tmu.s
  src/tmu/sleep.c
  src/tmu/tmu.c
  src/usb/classes/ff-bulk.c
  src/usb/configure.c
  src/usb/pipes.c
  src/usb/setup.c
  src/usb/string.c
  src/usb/usb.c
  src/3rdparty/tinymt32/rand.c
  src/3rdparty/tinymt32/tinymt32.c
  src/3rdparty/grisu2b_59_56/grisu2b_59_56.c
)
set(SOURCES_FX
  src/gray/engine.c
  src/gray/gclear.c
  src/gray/gint_gline.c
  src/gray/gpixel.c
  src/gray/grect.c
  src/gray/gsubimage.c
  src/gray/gtext.c
  src/render-fx/bopti-asm-gray-scsp.s
  src/render-fx/bopti-asm-gray.s
  src/render-fx/bopti-asm-mono-scsp.s
  src/render-fx/bopti-asm.s
  src/render-fx/bopti.c
  src/render-fx/dclear.c
  src/render-fx/dpixel.c
  src/render-fx/drect.c
  src/render-fx/dsubimage.c
  src/render-fx/dupdate.c
  src/render-fx/gint_dline.c
  src/render-fx/masks.c
  src/render-fx/topti-asm.s
  src/render-fx/topti.c
  src/t6k11/t6k11.c
  src/usb/classes/ff-bulk-gray.c
)
set(SOURCES_CG
  src/r61524/r61524.c
  src/render-cg/bopti-asm.s
  src/render-cg/bopti.c
  src/render-cg/dclear.c
  src/render-cg/dpixel.c
  src/render-cg/drect.c
  src/render-cg/dsubimage.c
  src/render-cg/dupdate.c
  src/render-cg/dvram.c
  src/render-cg/gint_dline.c
  src/render-cg/topti-asm.s
  src/render-cg/topti.c
)

set(ASSETS_FX src/font5x7.png)
set(ASSETS_CG src/font8x9.png)
fxconv_declare_assets(${ASSETS_FX} ${ASSETS_CG})

include_directories(
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_BINARY_DIR}/include"
  "${FXSDK_COMPILER_INSTALL}/include/openlibm")
add_compile_options(-Wall -Wextra -std=c11 -Os -fstrict-volatile-bitfields -mtas)

# Silence extended warnings on Grisu2b code
set_source_files_properties(src/3rdparty/grisu2b_59_56/grisu2b_59_56.c PROPERTIES
  COMPILE_OPTIONS "-Wno-all;-Wno-extra")

if("${FXSDK_PLATFORM_LONG}" STREQUAL fx9860G)
  add_compile_definitions(FX9860G)
  add_library(gint-fx STATIC ${SOURCES_COMMON} ${SOURCES_FX} ${ASSETS_FX})
  set(NAME "gint-fx")
  set(LINKER_SCRIPT "fx9860g.ld")
endif()

if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50)
  add_compile_definitions(FXCG50)
  add_library(gint-cg STATIC ${SOURCES_COMMON} ${SOURCES_CG} ${ASSETS_CG})
  set(NAME "gint-cg")
  set(LINKER_SCRIPT "fxcg50.ld")
endif()

set_target_properties("${NAME}" PROPERTIES OUTPUT_NAME "${NAME}")

# Library file
install(TARGETS "${NAME}" DESTINATION ${LIBDIR})
# Linker script
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT}"
  DESTINATION ${LIBDIR})
# Headers
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION ${INCDIR}
  FILES_MATCHING PATTERN "*.h")
# Auto-generated config header
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/gint/config.h"
  DESTINATION ${INCDIR}/gint)
# CMake module to find gint
install(FILES cmake/FindGint.cmake DESTINATION "${FXSDK_CMAKE_MODULE_PATH}")
