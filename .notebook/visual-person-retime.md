# Visual person-aware retime
> Local NanoDet sampling creates safe, exact-duration retime plans

Entry: `src/aieditor/assistantdock.cpp:AssistantDock::generatePlan()`

Flow:
- `src/aieditor/assistantdock.cpp:AssistantDock::generatePlan()` routes person-aware requests locally; no provider/API key
- `src/aieditor/localvisionanalyzer.cpp:LocalVisionAnalyzer::start()` serializes the scene, samples by hardware budget, caches seven days
- Visual cache keys must use `AiSessionStore::timelineFingerprint()` so the random `QTemporaryDir` path is normalized; raw scene hashes restart identical analysis on every request
- `src/aieditor/visionhelper/main.cpp:main()` decodes the MLT timeline and runs NanoDet in an isolated process
- `src/aieditor/personretimeplanner.cpp:PersonRetimePlanner::build()` pads person ranges, complements them, splits at clip boundaries, preflights, and distributes the exact target duration

Safety boundary:
- `src/aieditor/muterangeexecutor.cpp:MuteRangeExecutor::preflight()` rejects ranges crossing an internal clip boundary
- `src/aieditor/retimerangeexecutor.cpp:RetimeRangeExecutor::preflight()` has the same continuous-clip rule
- `src/aieditor/editplanexecutor.cpp:EditPlanExecutor::compatiblePlan()` skips unsafe operations; all skipped yields the dock error

Model provisioning: `src/aieditor/localvisionanalyzer.cpp:LocalVisionAnalyzer::prepare()` downloads a pinned OpenCV Zoo NanoDet model, verifies SHA-256, and chooses INT8/FP32 plus sampling frequency from detected hardware.

Packaging: `packaging/windows/build-installer.ps1` injects `firawynix-kdenlive-ai-vision.exe` and rejects payloads missing required OpenCV runtimes.

Target-duration constraint: normal-speed duration must be below requested final duration; distribute remaining duration proportionally across fast ranges. `PersonRetimePlanResult::minimumDurationFrames` exposes the shortest safe result so the dock can suggest a valid target without rerunning detection.

Verified 2026-09-15: planner/configuration tests pass; H.264 sample with a known person produces `person_frames:[0]` using CPU backend.

Updated: 2026-09-15
