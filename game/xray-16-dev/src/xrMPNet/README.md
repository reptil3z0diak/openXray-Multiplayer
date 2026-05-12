# xrMPNet Sessions 1-3

Transport, handshake, replication-core, and snapshot pipeline module for the custom multiplayer stack.

## Build options

- `XRMP_ENABLE_GNS=OFF` by default: builds the interface, codecs, handshake, and a stub `GNSTransport`.
- `XRMP_ENABLE_GNS=ON`: requires Valve GameNetworkingSockets headers and library discoverable by CMake.
- `XRMP_WITH_OPENXRAY=OFF` by default: builds the generic replication layer without direct engine headers.
- `XRMP_WITH_OPENXRAY=ON`: adds the optional `CSE_Abstract` and `NET_Packet` adapter layer. This path expects the normal OpenXRay engine build context and compile definitions, not a header-only standalone build.
- `XRMP_BUILD_TESTS=ON`: builds `xrMPNetHandshakeCodecTests`.
- `XRMP_BUILD_TESTS=ON`: also builds `xrMPNetReplicationLayerTests` and `xrMPNetSnapshotSystemTests`.
- `XRMP_BUILD_EXAMPLES=ON`: builds `xrMPNetTransportHandshakeExample`.

## OpenXRay-specific binding points

This module is currently engine-adjacent and does not depend on `xrCore`.
Future integration should bind:

- asset/script checksums from the canonical OpenXRay virtual filesystem manifest;
- auth token source from launcher/session service;
- server tick from the game scheduler;
- user messages from the replication and event channels.
- health/animation/inventory extraction hooks from concrete `CSE_Abstract` subclasses in `xrGame`.
- final gameplay-specific input schema and prediction reconciliation on top of the generic `InputBuffer`.

## Minimal test

```text
cmake -S . -B build/xrmp -DXRMP_BUILD_TESTS=ON -DXRMP_ENABLE_GNS=OFF
cmake --build build/xrmp --target xrMPNetHandshakeCodecTests
build/xrmp/src/xrMPNet/xrMPNetHandshakeCodecTests
build/xrmp/src/xrMPNet/xrMPNetReplicationLayerTests
build/xrmp/src/xrMPNet/xrMPNetSnapshotSystemTests
```

## GNS example

```text
cmake -S . -B build/xrmp-gns -DXRMP_ENABLE_GNS=ON -DXRMP_BUILD_EXAMPLES=ON
cmake --build build/xrmp-gns --target xrMPNetTransportHandshakeExample

xrMPNetTransportHandshakeExample server
xrMPNetTransportHandshakeExample 127.0.0.1
```
