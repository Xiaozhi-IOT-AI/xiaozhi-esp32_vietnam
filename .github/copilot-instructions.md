# Copilot instructions (xiaozhi-esp32_vietnam)

## Big picture
- This is an ESP-IDF firmware app. The entrypoint is [main/main.cc](../main/main.cc) (`app_main`).
- `Application` (singleton) is the top-level orchestrator/state machine: device state, networking, protocol session, audio I/O, UI, OTA, MCP. See [main/application.cc](../main/application.cc).
- Hardware is abstracted behind `Board` and per-board implementations under [main/boards/](../main/boards/). Common board helpers live in `main/boards/common/` and are pulled in by CMake.

## Build / flash / monitor (ESP-IDF)
- Use ESP-IDF v5.4+ (project expectation in README). Build with `idf.py`.
- This repo relies on layered defaults via `SDKCONFIG_DEFAULTS` (root `sdkconfig.defaults*`). Example for ESP32-S3 + LCD 2.8 defaults:
  - `SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.defaults.lcd2_8" idf.py build`
  - `... idf.py -p /dev/cu.usbmodemXXXX flash`
  - `... idf.py -p /dev/cu.usbmodemXXXX monitor --no-reset --timestamps`
- Prefer the VS Code tasks in this workspace (Build/Flash/Monitor + “Stop monitor”) to avoid serial-port contention.

## Configuration & board selection
- Project Kconfig is in [main/Kconfig.projbuild](../main/Kconfig.projbuild) (menu: “Xiaozhi Assistant”).
  - `CONFIG_OTA_URL` sets the default OTA endpoint used by version check.
  - `BOARD_TYPE` selects the target hardware; many options are gated by `IDF_TARGET_*`.
  - “Flash Assets” controls whether default/custom assets are flashed; “Default Language” sets initial UI language.
- Board-specific compile-time switches (`CONFIG_BOARD_TYPE_*`) drive build-time selection in [main/CMakeLists.txt](../main/CMakeLists.txt) (e.g., `BOARD_TYPE`, builtin fonts, emoji collection).

## Protocols & data flow
- The device connects to a backend using either WebSocket or MQTT+UDP; protocol implementations live in [main/protocols/](../main/protocols/).
- “Hello” handshake and message types are described in [docs/websocket.md](../docs/websocket.md) (JSON frames) and implemented via `Protocol` callbacks.
- MCP is carried inside the base transport as JSON: `{ "type": "mcp", "session_id": "...", "payload": <JSON-RPC 2.0> }`.
  - Wrapper helper: `Protocol::SendMcpMessage` in [main/protocols/protocol.cc](../main/protocols/protocol.cc).
  - Server-side tool registry/handlers: [main/mcp_server.cc](../main/mcp_server.cc).
  - MCP tools: common tools are added first to leverage prompt caching; board-specific tools must be added from the board’s `InitializeTools` (don’t add custom tools into `AddCommonTools`).

## Persistent settings (NVS)
- Use `Settings` as the NVS wrapper ([main/settings.cc](../main/settings.cc)). It commits on destruction if `dirty_` and opened read/write.
- Pick a namespace per feature (e.g., `Settings("assets", true)`) and keep key names stable; don’t forget read-only vs read-write.

## OTA + assets (v2 partition table)
- v2 introduces an `assets` partition for network-loadable content (fonts/emoji/sounds/wakeword models). See [partitions/v2/README.md](../partitions/v2/README.md).
- `Application::CheckAssetsVersion()` reads `assets/download_url` from NVS and triggers `Assets::Download()` + `Assets::Apply()`; this is the supported way to update assets without reflashing.
- Firmware OTA/version check lives in `Ota`/`OtaServer` and is driven from `Application::CheckNewVersion()` in [main/application.cc](../main/application.cc).

## Patterns to follow when editing
- Add feature code under [main/features/](../main/features/) (e.g., [main/features/weather/](../main/features/weather/), [main/features/music/](../main/features/music/)), and wire it through `Application` only at integration points.
- UI updates go through the display abstraction (`Board::GetInstance().GetDisplay()`), not direct panel drivers.
- Be mindful of embedded constraints: avoid large stack allocations (there are explicit SRAM notes in `Application`), and prefer incremental/progress callbacks for long operations (e.g., assets download).
- Respect compile-time constraints: AEC modes are mutually exclusive (`CONFIG_USE_DEVICE_AEC` vs `CONFIG_USE_SERVER_AEC`) and enforced in `Application`.
