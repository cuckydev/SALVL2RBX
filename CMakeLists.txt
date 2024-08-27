project(SALVL2RBX LANGUAGES C CXX)

add_library(SALVL2RBX INTERFACE)
target_include_directories(SALVL2RBX INTERFACE "SALVL2RBX")
target_include_directories(SALVL2RBX INTERFACE "lib")
target_include_directories(SALVL2RBX INTERFACE "sadx-mod-loader/SADXModLoader/include")
target_link_libraries(SALVL2RBX INTERFACE wininet Ws2_32)

add_executable(SA1LVL2RBX
	"SALVL2RBX/SALVL2RBX.cpp"
	"SALVL2RBX/SALVL2RBX.h"
	"SA1LVL2RBX/SA1LVL2RBX.cpp"
	"sadx-mod-loader/libmodutils/LandTableInfo.cpp"
)
target_link_libraries(SA1LVL2RBX PUBLIC SALVL2RBX)
target_include_directories(SA1LVL2RBX PUBLIC "sadx-mod-loader/libmodutils")
target_include_directories(SA1LVL2RBX PUBLIC "sadx-mod-loader/SADXModLoader/include")

add_executable(SA2LVL2RBX
	"SALVL2RBX/SALVL2RBX.cpp"
	"SALVL2RBX/SALVL2RBX.h"
	"SA2LVL2RBX/SA2LVL2RBX.cpp"
	"sa2-mod-loader/libmodutils/LandTableInfo.cpp"
)
target_link_libraries(SA2LVL2RBX PUBLIC SALVL2RBX)
target_include_directories(SA2LVL2RBX PUBLIC "sa2-mod-loader/libmodutils")
target_include_directories(SA2LVL2RBX PUBLIC "sa2-mod-loader/SA2ModLoader/include")
target_compile_definitions(SA2LVL2RBX PUBLIC SALVL_DOUBLESIDED)
