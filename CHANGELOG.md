# Changelog

## 0.2.1

- Fixed the calibration request to invoke the editor's `TriggerCalibrate` port instead of the internal `Calibrate` method name.
- Check the receiver's advertised trigger metadata before sending a calibration request.
- Added regression scenarios for missing trigger metadata and an unsupported legacy trigger name.

## 0.2.0

- Added automatic OBS language selection and nine embedded translations.
- Added translated button labels, tooltips, accessibility text, status messages, and errors.
- Added regional locale fallback and an English default.
- Added comment-free C++ source, CMake builds, translation validation, and isolated integration tests.
- Added English and Korean documentation, a multilingual quick start, and a Windows build workflow.

## 0.1.0

- Initial Korean-only OBS Controls button for local RhyLive recalibration.
