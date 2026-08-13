# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

## [1.1.0] - 2026-08-13

- Implemented the `~&` (reduction-NAND) operator.
- Fixed `splitLogicConesLoops` to pass explicit `TerminalPrefixOptions` (`recurseIntoMembers`/`recurseIntoSlices` = true) into `getTerminalPrefix`, fixing a declaration-resolution assert on constructs like `gray2bin`/`muxhot`/`shiftreg` that assign to different bit-selects of the same signal.
- Added an end-to-end regression for `$clog2` used in a port range, pairing hif-core's registration fix.
- Migrated the project to the `hif-project` GitHub organization; updated internal references accordingly.
- Replaced the README's ecosystem-navigation list with a link to the organization profile.

## [1.0.0] - 2026-08-12

Initial coordinated release of the HIF toolchain baseline (hif-core, hif-frontend, hif-backend, hif-muffin, all tagged v1.0.0).

- Fixed a heap corruption crash in `verilog2hif` on escaped Verilog identifiers.
- Added support for the 8 basic Verilog gate primitives (`and`/`nand`/`or`/`nor`/`xor`/`xnor`/`buf`/`not`).
- Fixed CI to actually build its `hif-core` dependency (previously failed at the configure step on every run) and consolidated it to a single Linux workflow.
