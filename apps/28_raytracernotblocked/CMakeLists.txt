add_subdirectory(structs)

site_name(host)
if (host STREQUAL "naps62-mint")
include_directories(/home/naps62/.papi/include)
app(raytracernotblocked main.cpp Config.cpp EXTLIBS /home/naps62/.papi/lib/libpapi.a /usr/local/lib/libCGAL.so)
else()
include_directories(/workspace/pcosta/local/include)
app(raytracernotblocked main.cpp Config.cpp EXTLIBS /workspace/pcosta/local/lib/libpapi.a)
endif()

target_link_libraries(raytracernotblocked STRUCTSNOTBLOCKED)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frounding-math")
  #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -frounding-math")
endif()
