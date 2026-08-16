# Changelog

All notable changes to this project are documented in this file.

## [Unreleased]

- The empty `Wait` that `_fixProcessesWithWait` appends to a suspending process now carries `PROPERTY_PROCESS_LOOP_TAIL`. That node is not a statement the source wrote: it records that the process loops back round and reapplies its static sensitivity, which is what the SystemC lowering emits at the tail of its `while (true)`. Every field on it is null, which made it identical to the node `vhdl2hif` produces for VHDL's `wait;` — and that means the opposite, suspend permanently. A backend reading the tree therefore had no way to tell them apart and necessarily got one of the two wrong: `hif2verilog` regenerated a VHDL process ending in `wait;` as a zero-delay `always begin ... end` ([hif-backend#46](https://github.com/hif-project/hif-backend/issues/46)). Naming the node is additive and changes nothing on its own — no existing consumer reads the property, and the node is otherwise unchanged — so `hif2sc` and the core manipulations that treat it as a suspension point are unaffected. (hif-backend#46)

## [1.2.0] - 2026-08-16

- Fixed `verilog2hif` aborting with hif-core's "Declaration not found" on a continuous assignment to an output port whose right-hand side refers to a declaration in the module body — so the design could not be translated at all. An assignment with a constant-looking right-hand side was folded into the target's declaration, and for an output port that declaration lives on the `Entity`, which cannot see `Contents` declarations; the reference was left with nothing to resolve to. Folding is now restricted to values that stay resolvable where they are moved. Reported for a user-defined `function` (#14), but a plain `localparam` failed identically — it was never about functions. Literals, module parameters and system functions were always reachable from the `Entity` and still fold as before. (#14)

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
