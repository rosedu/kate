# Uses Q3ListView!! TODO: port!
add_definitions(-DQT3_SUPPORT)


########### next target ###############

set(katexmlcheckplugin_PART_SRCS
plugin_katexmledit.cpp
xml_parser_parts.cpp )

kde4_add_plugin(katexmleditplugin ${katexmlcheckplugin_PART_SRCS})

target_link_libraries(katexmleditplugin ${QT_QT3SUPPORT_LIBRARY} ${KDE4_KDEUI_LIBS} ${KDE4_KTEXTEDITOR_LIBS} kateinterfaces )

install(TARGETS katexmleditplugin  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES ui.rc  DESTINATION  ${DATA_INSTALL_DIR}/kate/plugins/katexmledit )
install( FILES katexmledit.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )



