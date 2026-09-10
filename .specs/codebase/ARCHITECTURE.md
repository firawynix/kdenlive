# Architecture

**Pattern:** Modular desktop monolith with model/view/controller timeline layers over MLT.

## High-Level Structure

- `MainWindow` composes dock widgets and global actions.
- QML implements high-interaction timeline and monitor surfaces.
- `TimelineController` bridges UI actions to timeline models.
- `TimelineModel` owns tracks, clips, compositions, selection, grouping, and undoable mutations.
- MLT producers, filters, transitions, and consumers perform media processing and rendering.

## Relevant Patterns

### Timeline transaction composition

`src/timeline2/model/timelinemodel.cpp:TimelineModel::requestClipTimeWarp()` accepts shared undo/redo lambdas. Complex operations compose lambdas and push one user-visible undo command through Core.

### Range editing

`src/timeline2/model/timelinefunctions.cpp:TimelineFunctions::liftZone()` cuts boundary clips, enumerates items in a range, and composes mutations. `removeSpace()` and `requestInsertSpace()` provide existing ripple-move patterns.

### Dock integration

`src/mainwindow.cpp:MainWindow::addDock()` registers widget-based tools such as Time Remapping, Markers, and Online Resources.

### Network integration

`src/onlineresources/providermodel.cpp:ProviderModel::slotStartSearch()` uses `QNetworkAccessManager`, explicit replies, Qt signals, and actionable HTTP errors.

## AI Edit Flow

Prompt → provider client → untrusted JSON → strict parser → preview model → timeline executor → composed undo/redo → MLT-backed timeline.

