
set(kate_kttsd_PART_SRCS katekttsd.cpp )

kde4_add_plugin(kate_kttsd ${kate_kttsd_PART_SRCS})

target_link_libraries(kate_kttsd ${KDE4_KDECORE_LIBS} ${KDE4_KTEXTEDITOR_LIBS} ${KDE4_KPARTS_LIBS} kateinterfaces )


install(TARGETS kate_kttsd  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kate_kttsd.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES ui.rc DESTINATION  ${DATA_INSTALL_DIR}/kate/plugins/kate_kttsd)
