# xrMPNet Session 1

Transport and handshake module for the custom multiplayer stack.

## Build options

- `XRMP_ENABLE_GNS=OFF` by default: builds the interface, codecs, handshake, and a stub `GNSTransport`.
- `XRMP_ENABLE_GNS=ON`: requires Valve GameNetworkingSockets headers and library discoverable by CMake.
- `XRMP_BUILD_TESTS=ON`: builds `xrMPNetHandshakeCodecTests`.
- `XRMP_BUILD_EXAMPLES=ON`: builds `xrMPNetTransportHandshakeExample`.

## OpenXRay-specific binding points

This module is currently engine-adjacent and does not depend on `xrCore`.
Future integration should bind:

- asset/script checksums from the canonical OpenXRay virtual filesystem manifest;
- auth token source from launcher/session service;
- server tick from the game scheduler;
- user messages from the replication and event channels.

## Minimal test

```text
cmake -S . -B build/xrmp -DXRMP_BUILD_TESTS=ON -DXRMP_ENABLE_GNS=OFF
cmake --build build/xrmp --target xrMPNetHandshakeCodecTests
build/xrmp/src/xrMPNet/xrMPNetHandshakeCodecTests
```

## GNS example

```text
cmake -S . -B build/xrmp-gns -DXRMP_ENABLE_GNS=ON -DXRMP_BUILD_EXAMPLES=ON
cmake --build build/xrmp-gns --target xrMPNetTransportHandshakeExample

xrMPNetTransportHandshakeExample server
xrMPNetTransportHandshakeExample 127.0.0.1
```
