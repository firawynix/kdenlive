# Project Structure

**Root:** `C:/Users/Hugo/kdenlive`

```text
kdenlive/
├── src/                 application code
│   ├── timeline2/       timeline model, controller, and QML
│   ├── dialogs/         dialogs and tool widgets
│   ├── onlineresources/ existing HTTP integration pattern
│   ├── project/         project lifecycle
│   └── render/          rendering workflows
├── tests/               Catch-based model and integration tests
├── appiumtests/         UI automation
├── data/                effects, profiles, icons, and resources
├── dev-docs/            architecture, build, and coding notes
└── packaging/           platform packages
```

## Where the AI Feature Lives

- Domain/parser/provider code: `src/aieditor/`.
- Timeline execution: `src/aieditor/`, calling public `timeline2/model` APIs.
- Dock UI: planned under `src/aieditor/` and registered by `MainWindow`.
- Unit tests: `tests/aieditorplannertest.cpp` and later executor tests.

