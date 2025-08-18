set(SRCLIST_APP src/main_PJRD.cpp src/xAppPJRD.h src/xAppPJRD.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${SRCLIST_APP})
source_group(App FILES ${SRCLIST_APP})

