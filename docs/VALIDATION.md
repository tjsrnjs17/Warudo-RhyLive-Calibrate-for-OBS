# Validation

Validated on 2026-10-29 for Windows x64, using MSVC 19.44.35229 and Qt 6.11.1.

The public CMake project configured and built successfully. Both CTest groups passed:

| Test group | Coverage |
| --- | --- |
| locales-and-placement | 15 locale configurations, all 9 translation tables, native button text and accessibility text, insertion directly below Settings in a fixture layout, and duplicate attachment prevention |
| local-api-integration | 15 isolated scenarios using temporary loopback servers, synthetic authentication, and the production control code |

Integration scenarios cover successful calibration acknowledgements, object-form asset lists, inactive receiver filtering, fragmented WebSocket messages and ping/pong, HTTP authentication failures, missing or multiple receivers, missing trigger metadata, an unsupported legacy trigger name, malformed scene JSON, invalid WebSocket handshakes, trigger errors, unconfirmed acknowledgements, timeouts, and unavailable servers.

The x64 DLL's OBS module exports and dependencies were inspected. All nine locale files contain the same 32 nonempty message keys. Source, build scripts, and tests contain no code comments.

Version 0.2.1 fixes a real failure recorded by Warudo: `Trigger port Calibrate does not exist`. Inspection of the installed Warudo receiver implementation identified the exposed editor trigger as `TriggerCalibrate`. The plugin now requires that port to be present in the serialized receiver metadata before issuing the request.

The tests do not open or control a user's OBS or Warudo. Actual installation, a live Warudo calibration, a broadcast session, other OBS versions, and the remote GitHub Actions runner have not been validated by these tests.
