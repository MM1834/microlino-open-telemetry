# Contributing to Microlino Open Telemetry

Thank you for your interest in contributing to Microlino Open Telemetry (MOT).

MOT is designed as an open, modular telemetry platform. Contributions are welcome in firmware, documentation, dashboard development, hardware testing, CAN decoding and integrations.

## Project Principles

Please keep these principles in mind:

- One telemetry model is the single source of truth.
- CAN decoders only decode vehicle data into the telemetry model.
- Outputs such as MQTT, JSON API, dashboard and ABRP do not decode CAN data.
- Vehicle-specific logic belongs inside decoders.
- MQTT and HTTP APIs should follow the documented telemetry structure.
- Every commit should keep the project buildable.

## Types of Contributions

Useful contributions include:

- New CAN signals or decoder improvements
- Hardware wiring documentation
- MQTT or JSON API improvements
- Dashboard improvements
- Testing on different Microlino generations
- Documentation corrections
- Firmware portability improvements

## Adding a CAN Signal

When adding a new decoded signal:

1. Document the raw CAN frame in `docs/can/`.
2. Add the decoded field to the telemetry model.
3. Publish it via MQTT only through the telemetry output layer.
4. Add it to the JSON API if it is user-facing.
5. Mark the signal as confirmed or experimental.

## Confirmed vs Experimental Data

Many vehicle signals are discovered by observation.

Please mark values clearly:

- `confirmed` - verified against the vehicle display, measurements or repeated tests
- `candidate` - likely correct but not fully verified
- `experimental` - useful for analysis but not yet reliable

## Pull Requests

Pull requests should include:

- A short description of the change
- Hardware used for testing
- Microlino model/generation if relevant
- Screenshots or logs if helpful
- Updated documentation when adding new behavior

## Coding Style

Firmware code should favor clarity over cleverness.

- Keep modules small.
- Avoid duplicating telemetry state.
- Do not mix decoding and publishing logic.
- Prefer explicit names over abbreviations.

## License

By contributing, you agree that your contribution will be licensed under the MIT License.
