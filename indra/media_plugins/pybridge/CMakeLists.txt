# -*- cmake -*-
project(media_plugin_pybridge)

include(00-Common)
include(LLCommon)
include(LLImage)
include(LLWindow)
include(Linking)
include(PluginAPI)

if(NOT ADDRESS_SIZE EQUAL 32)
  if(WINDOWS)
  else()
    add_definitions(-fPIC)
  endif()
endif()

set(media_plugin_pybridge_SOURCE_FILES
    media_plugin_pybridge.cpp
    )

add_library(media_plugin_pybridge SHARED ${media_plugin_pybridge_SOURCE_FILES})

target_link_libraries(media_plugin_pybridge media_plugin_base llwebrtc curl)

if(DARWIN)
  set_target_properties(
    media_plugin_pybridge
    PROPERTIES
    PREFIX ""
    BUILD_WITH_INSTALL_RPATH 1
    INSTALL_NAME_DIR "@executable_path"
    LINK_FLAGS "-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/../base/media_plugin_base.exp"
  )
endif()
