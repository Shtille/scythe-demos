project(Sandbox)

set(CMAKE_CXX_STANDARD 14)
set(SRC_DIRS
	src
)
set(include_directories
	${SCYTHE_PATH}/include
	${SCYTHE_PATH}/src
	${SCYTHE_THIRDPARTY_DIR}/bullet/src
	${SCYTHE_THIRDPARTY_DIR}/script/src
)
#set(defines )
set(libraries
	scythe
	bullet
	script
)

foreach(DIR ${SRC_DIRS})
	file(GLOB DIR_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp)
	set(SRC_FILES ${SRC_FILES} ${DIR_SOURCE})
endforeach(DIR)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${include_directories})
#target_compile_definitions(${PROJECT_NAME} PRIVATE ${defines})
target_link_libraries(${PROJECT_NAME} PRIVATE ${libraries})

install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${BINARY_PATH})