# Contributing to Stream Video ESP32

Thank you for your interest in contributing. This project is in **developer preview**; we welcome issues and pull requests.

## How to contribute

1. **Build and run:** Follow the [main README](README.md) and [minimal example README](examples/minimal/README.md). You need ESP-IDF v5.4+ and an ESP32-S3 or ESP32-P4 board.
2. **Code style:** Match the existing C style in the repo (indentation, naming, comments). We use ESP-IDF logging (`ESP_LOGI`, `ESP_LOGE`, etc.).
3. **Tests:** There is no automated test suite yet. Please test on real hardware where relevant and mention your board and IDF version in PRs.
4. **Docs:** If you change behavior or add options, update the relevant README or file under `docs/`.

## Opening issues

- **Bugs:** Include ESP-IDF version, target chip (e.g. ESP32-S3), steps to reproduce, and logs if applicable.
- **Features:** Describe the use case and how you’d expect it to work.

## Pull requests

- Keep PRs focused. If you’re fixing several things, consider splitting into separate PRs.
- In the PR description, note what you tested (e.g. “Built and ran minimal example on ESP32-S3, joined call and published video”).
- If you change user-facing config or docs, update the README and/or `docs/` as needed.

## Discussion

For questions or design discussion, open a GitHub Discussion or an issue labeled as a question.
