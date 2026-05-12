option(XRMP_ENABLE_GNS "Build xrMPNet with Valve GameNetworkingSockets transport" OFF)
option(XRMP_BUILD_EXAMPLES "Build xrMPNet examples" OFF)
option(XRMP_BUILD_TESTS "Build xrMPNet minimal tests" OFF)

add_library(xrMPNet STATIC)

target_sources(xrMPNet PRIVATE
    ByteStream.h
    GNSTransport.cpp
    GNSTransport.h
    Handshake.cpp
    Handshake.h
    ITransport.h
    NetMessageCodec.cpp
    NetMessageCodec.h
    NetTypes.h
)

target_include_directories(xrMPNet
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(xrMPNet PUBLIC cxx_std_17)
target_compile_definitions(xrMPNet PUBLIC XRMP_ENABLE_GNS=$<BOOL:${XRMP_ENABLE_GNS}>)

if (XRMP_ENABLE_GNS)
    find_path(GAMENETWORKINGSOCKETS_INCLUDE_DIR steam/steamnetworkingsockets.h)
    find_library(GAMENETWORKINGSOCKETS_LIBRARY GameNetworkingSockets)

    if (NOT GAMENETWORKINGSOCKETS_INCLUDE_DIR OR NOT GAMENETWORKINGSOCKETS_LIBRARY)
        message(FATAL_ERROR "XRMP_ENABLE_GNS=ON requires GameNetworkingSockets headers and library")
    endif()

    target_include_directories(xrMPNet PUBLIC "${GAMENETWORKINGSOCKETS_INCLUDE_DIR}")
    target_link_libraries(xrMPNet PUBLIC "${GAMENETWORKINGSOCKETS_LIBRARY}")
endif()

if (XRMP_BUILD_EXAMPLES)
    add_executable(xrMPNetTransportHandshakeExample examples/TransportHandshakeExample.cpp)
    target_link_libraries(xrMPNetTransportHandshakeExample PRIVATE xrMPNet)
endif()

if (XRMP_BUILD_TESTS)
    add_executable(xrMPNetHandshakeCodecTests tests/HandshakeCodecTests.cpp)
    target_link_libraries(xrMPNetHandshakeCodecTests PRIVATE xrMPNet)
endif()
