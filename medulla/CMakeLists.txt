if (NOT WIN32)
	find_package(Qt4 COMPONENTS QtCore QtDBus QtXml REQUIRED )
endif (NOT WIN32)

if (QT4_FOUND AND QT_QTDBUS_FOUND)
	set(QT_USE_QTDBUS ON)
	set(QT_USE_QTXML ON)
	set(QT_DONT_USE_QTGUI ON)
	include(${QT_USE_FILE})
	
	set(medulla_SRCS
		medulla.cpp
		${SOCKETCAN_PLUGIN}
	)
	
	qt4_automoc(${medulla_SRCS})
	
	include_directories (${CMAKE_BINARY_DIR}/medulla)
	
	add_executable(asebamedulla ${medulla_SRCS})
	
	target_link_libraries(asebamedulla asebacompiler asebacore ${QT_LIBRARIES} ${EXTRA_LIBS})

	install(TARGETS asebamedulla RUNTIME
		DESTINATION bin
		LIBRARY DESTINATION lib
	)
endif (QT4_FOUND AND QT_QTDBUS_FOUND)
