# Search Meta-Engine Invariants Review Document

## Scope and Terminology

This document is a code-grounded, normative reference for invariants across search `Engine`, stop objects, restart meta-search (`RBS`), portfolio meta-search (`PBS`), and restart-based incomplete solver behavior (including FlatZinc LNS/on-restart).

Assumptions/defaults for this review:
- Scope includes core search, driver policy interaction, and FlatZinc restart/LNS behavior.
- This document defines behavioral invariants, not new runtime behavior.
- All claims are backed by direct source citations.

Terminology used below:
- `engine`: any `Gecode::Search::Engine` implementation (`next/statistics/stopped/constrain/reset/nogoods`). Evidence: `gecode/search.hh:921`, `gecode/search/engine.cpp:39`.
- `meta engine`: engine orchestrating other engines (`RBS`/`PBS`). Evidence: `gecode/search.hh:1156`, `gecode/search.hh:1241`.
- `master` and `slave`: meta callbacks on `Space` configured by `MetaInfo`. Evidence: `gecode/kernel/core.hpp:2066`, `gecode/kernel/core.hpp:2091`.
- `asset`: one slave engine in a portfolio. Evidence: `gecode/kernel/core.hpp:1657`, `gecode/kernel/core.hpp:1692`.
- `complete` vs `incomplete`: restart slave search completeness as returned by `slave(mi)` (`true` complete, `false` incomplete). Evidence: `gecode/kernel/core.hpp:2100`.

Interfaces explicitly covered:
- `Gecode::Search::Engine`: `gecode/search.hh:921`.
- `Gecode::Search::Stop`, `NodeStop`, `FailStop`, `TimeStop`, `RestartStop`: `gecode/search.hh:800`, `gecode/search.hh:832`, `gecode/search.hh:855`, `gecode/search.hh:874`, `gecode/search.hh:897`.
- `Gecode::Search::Options` (`assets`, `slice`, `stop`, `cutoff`, `threads`): `gecode/search.hh:748`, `gecode/search.hh:759`, `gecode/search.hh:762`, `gecode/search.hh:766`, `gecode/search.hh:768`, `gecode/search.hh:752`.
- `Gecode::MetaInfo` (`RESTART`/`PORTFOLIO`, `RR_*`, accessors): `gecode/kernel/core.hpp:1615`, `gecode/kernel/core.hpp:1619`, `gecode/kernel/core.hpp:1625`, `gecode/kernel/core.hpp:3116`.
- `Space::master` / `Space::slave`: `gecode/kernel/core.hpp:2089`, `gecode/kernel/core.hpp:2116`.

## Object Graph and Control Flow

### RBS control graph

`RBS` wraps one inner search engine plus restart control state:
- Fields: inner engine `e`, `master`, `last`, cutoff `co`, restart stop controller `stop`, completeness/restart flags. Evidence: `gecode/search/seq/rbs.hh:72`.
- Constructor sets initial per-round fail limit from cutoff via `stop->limit(stat, (*co)())`. Evidence: `gecode/search/seq/rbs.hpp:65`, `gecode/search/seq/rbs.hpp:71`.
- `next()` alternates between:
1. pre-step restart handling after a found solution (`restart` flag path),
2. delegating to inner `e->next()`,
3. forcing true restart on incomplete completion or engine fail cutoff. Evidence: `gecode/search/seq/rbs.cpp:55`.

### PBS control graph

`PBS` creates one engine per asset and wraps each asset's stop object:
- Per-slave stop wrapper is always inserted (`Seq::pbsstop`/`Par::pbsstop`). Evidence: `gecode/search/pbs.hpp:111`, `gecode/search/pbs.hpp:172`.
- `master->master(0)` is always called once before creating the portfolio engines. Evidence: `gecode/search/pbs.hpp:247`, `gecode/search/pbs.hpp:295`.
- Each asset gets a `slave(i)` callback before engine build. Evidence: `gecode/search/pbs.hpp:114`, `gecode/search/pbs.hpp:142`, `gecode/search/pbs.hpp:175`, `gecode/search/pbs.hpp:206`.
- SEB path supports heterogeneous builders and reconfigures each builder with wrapped stop + `clone=false`. Evidence: `gecode/search/pbs.hpp:137`, `gecode/search/pbs.hpp:138`, `gecode/search/pbs.hpp:139`, `gecode/search/pbs.hpp:201`, `gecode/search/pbs.hpp:202`, `gecode/search/pbs.hpp:203`.

## Invariant Catalog (normative)

### INV-STOP-1: Global stop thresholds are strict `>`
Search-global stop objects trigger only when the statistic is strictly above the configured limit:
- node: `s.node > l`
- fail: `s.fail > l`
- restart: `s.restart > l`
- time: elapsed `> l`
Evidence: `gecode/search/stop.cpp:65`, `gecode/search/stop.cpp:75`, `gecode/search/stop.cpp:85`, `gecode/search/stop.cpp:94`.

### INV-STOP-2: Sequential PBS slice threshold is `>=` and distinct from external stop
Sequential portfolio stop wrappers distinguish two stop classes:
- external/meta stop object hit: set `ssi->done=false`
- slice fail budget hit (`s.fail >= ssi->l`): set `ssi->done=true`
Evidence: `gecode/search/seq/pbs.cpp:39`, `gecode/search/seq/pbs.cpp:40`, `gecode/search/seq/pbs.cpp:44`.

### INV-WORKER-1: Worker stop is latched until `start()` clears it
`Worker::stop` performs `_stopped |= ...`; once true, it stays true for the active round until `start()` resets it to false.
Evidence: `gecode/search/worker.hh:74`, `gecode/search/worker.hh:79`, `gecode/search/worker.hh:82`.

### INV-RBS-1: Restart stop object splits engine fail-cutoff from meta-stop
`Seq::RestartStop::stop` first checks per-round fail cutoff (`s.fail > l`), marks `e_stopped=true`, increments meta restart count, and stops current round. Otherwise, it evaluates meta stop object on accumulated stats (`m_stat + s`) and sets `e_stopped=false`.
Evidence: `gecode/search/seq/rbs.cpp:40`, `gecode/search/seq/rbs.cpp:42`, `gecode/search/seq/rbs.cpp:44`, `gecode/search/seq/rbs.cpp:48`, `gecode/search/seq/rbs.hpp:42`, `gecode/search/seq/rbs.hh:53`.

### INV-RBS-2: RBS restart state machine
RBS state transitions are deterministic:
- solution found from inner engine: set `restart=true`, clone `last`, return solution.
- on next call with `restart=true`: execute `master(mi{RR_SOL})`; if master asks restart, clone/prepare slave and reset inner engine.
- true restart also occurs when either:
  - search is incomplete and inner engine finished without stop (`!complete && !e->stopped()`), or
  - inner engine stopped due engine fail-cutoff (`e->stopped() && stop->enginestopped()`).
- restart reason is `RR_CMPL` for completion path and `RR_LIM` for limit path.
Evidence: `gecode/search/seq/rbs.cpp:57`, `gecode/search/seq/rbs.cpp:63`, `gecode/search/seq/rbs.cpp:72`, `gecode/search/seq/rbs.cpp:83`, `gecode/search/seq/rbs.cpp:86`, `gecode/search/seq/rbs.cpp:90`, `gecode/search/seq/rbs.cpp:99`, `gecode/search/seq/rbs.cpp:108`.

### INV-RBS-3: `sslr` semantics are restart-local
`sslr` counts solutions since last restart; increment on restart-processing of a found solution; reset to 0 when a restart is materialized.
Evidence: `gecode/search/seq/rbs.cpp:59`, `gecode/search/seq/rbs.cpp:78`, `gecode/search/seq/rbs.cpp:96`.

### INV-RBS-4: RBS statistics composition
`RBS::statistics()` is always `metastatistics + inner statistics`.
Evidence: `gecode/search/seq/rbs.cpp:119`.

### INV-RBS-5: RBS stopped-reporting proxies inner engine
`RBS::stopped()` returns `e->stopped()` (not a separate meta stop flag), explicitly to avoid missing restart opportunities in parallel inner engines.
Evidence: `gecode/search/seq/rbs.cpp:141`, `gecode/search/seq/rbs.cpp:149`.

### INV-RBS-6: Initial restart phase uses `RR_INIT`
RBS constructor path sends `MetaInfo(0, RR_INIT, ...)` into the initial slave before creating the wrapped engine.
Evidence: `gecode/search/rbs.hpp:100`, `gecode/search/rbs.hpp:101`, `gecode/kernel/core.hpp:1629`.

### INV-PBS-1: Portfolio build always wraps stop per asset
Each asset receives a `PortfolioStop` wrapper, not the raw stop object.
Evidence: `gecode/search/pbs.hpp:111`, `gecode/search/pbs.hpp:137`, `gecode/search/pbs.hpp:172`, `gecode/search/pbs.hpp:201`.

### INV-PBS-2: Portfolio build always calls master once, slave per asset
`master->master(0)` is invoked once before assets are built; each asset executes `slave(asset_id)`.
Evidence: `gecode/search/pbs.hpp:247`, `gecode/search/pbs.hpp:114`, `gecode/search/pbs.hpp:142`.

### INV-PBS-3: Sequential PBS slicing is round-robin, slice-bounded
Sequential PBS behavior:
- if slave stopped and `ssi.done=true`, current slice exhausted for that slave; rotate.
- if slave stopped and `ssi.done=false`, treat as external/meta stop (`slave_stop=true`, return nullptr).
- increment slice threshold only after all remaining slaves have exhausted the current slice.
Evidence: `gecode/search/seq/pbs.hpp:111`, `gecode/search/seq/pbs.hpp:112`, `gecode/search/seq/pbs.hpp:115`, `gecode/search/seq/pbs.hpp:127`, `gecode/search/seq/pbs.hpp:130`.

### INV-PBS-4: Sequential PBS lifecycle compacts completed slaves
A non-stopped, exhausted slave is finalized immediately, its statistics are added, and it is removed by swap-with-last compaction. If only one slave remains, slice stopping is disabled (`ULONG_MAX`).
Evidence: `gecode/search/seq/pbs.hpp:119`, `gecode/search/seq/pbs.hpp:120`, `gecode/search/seq/pbs.hpp:121`, `gecode/search/seq/pbs.hpp:122`, `gecode/search/seq/pbs.hpp:123`, `gecode/search/seq/pbs.hpp:125`.

### INV-PBS-5: Parallel PBS core invariant is explicit and enforced
Parallel PBS documents and enforces:
- `n_busy == 0` outside `next()`.
- active assets are `0..n_active-1`; inactive/exhausted are `n_active..n_slaves-1`.
Evidence: `gecode/search/par/pbs.hh:164`, `gecode/search/par/pbs.hh:165`, `gecode/search/par/pbs.hh:167`, `gecode/search/par/pbs.hpp:217`, `gecode/search/par/pbs.hpp:233`.

### INV-PBS-6: Parallel stop propagation is shared-flag based
Parallel `PortfolioStop` stops when shared `tostop` is true or underlying stop triggers.
`tostop` becomes true when:
- an accepted solution is reported,
- or a slave exhausts search space.
Evidence: `gecode/search/par/pbs.cpp:39`, `gecode/search/par/pbs.cpp:40`, `gecode/search/par/pbs.hpp:175`, `gecode/search/par/pbs.hpp:177`, `gecode/search/par/pbs.hpp:188`, `gecode/search/par/pbs.hpp:189`.

### INV-METAINFO-1: Accessors are type-safe by assertion
Restart-only accessors assert `type()==RESTART`, and portfolio accessor asserts `type()==PORTFOLIO`.
Evidence: `gecode/kernel/core.hpp:3116`, `gecode/kernel/core.hpp:3125`, `gecode/kernel/core.hpp:3130`, `gecode/kernel/core.hpp:3135`, `gecode/kernel/core.hpp:3145`.

### INV-SPACE-1: Default restart master behavior posts nogoods and restarts
Default `Space::master(RESTART)`:
- applies `constrain(*mi.last())` if last exists,
- posts `mi.nogoods()`,
- returns `true` (restart even after solution).
Evidence: `gecode/kernel/core.cpp:848`, `gecode/kernel/core.cpp:849`, `gecode/kernel/core.cpp:851`, `gecode/kernel/core.cpp:853`.

### INV-SPACE-2: Default portfolio master kills branchers
Default `Space::master(PORTFOLIO)` kills all branchers on the master.
Evidence: `gecode/kernel/core.cpp:854`, `gecode/kernel/core.cpp:856`.

### INV-SPACE-3: Default slave is complete (`true`)
Default `Space::slave` returns `true`.
Evidence: `gecode/kernel/core.cpp:864`, `gecode/kernel/core.cpp:865`.

### INV-INCOMPLETE-1: `slave(mi)==false` means incomplete and mandates restart semantics
Contract: `false` from restart `slave(mi)` marks search as incomplete; meta engine restarts regardless of local completion/timeout. RBS enforces this via `!complete` restart path.
Evidence: `gecode/kernel/core.hpp:2100`, `gecode/kernel/core.hpp:2103`, `gecode/kernel/core.hpp:2104`, `gecode/search/seq/rbs.cpp:90`, `gecode/search/seq/rbs.cpp:94`.

### INV-FZ-1: FlatZinc restart mode routes through RBS
When `opt.restart()!=RM_NONE`, FlatZinc runs through `RBS` meta engine.
Evidence: `gecode/flatzinc/flatzinc.cpp:1744`, `gecode/flatzinc/flatzinc.cpp:1747`.

### INV-FZ-2: FlatZinc on-restart model rewriting is explicitly incomplete
If any `on_restart_*` transformation path executes, `FlatZincSpace::slave` returns `false` (incomplete restart round).
Evidence: `gecode/flatzinc/flatzinc.cpp:2021`, `gecode/flatzinc/flatzinc.cpp:2183`, `gecode/flatzinc/flatzinc.cpp:2184`.

### INV-FZ-3: FlatZinc LNS restart behavior is explicitly incomplete for both bootstrap and post-solution
For restart rounds (`mi.restart()!=0`) with `_lns>0`, FlatZinc LNS relax/fix path returns `false` both when `mi.last()==nullptr` (bootstrap) and when a last solution exists.
Evidence: `gecode/flatzinc/flatzinc.cpp:2188`, `gecode/flatzinc/flatzinc.cpp:2189`, `gecode/flatzinc/flatzinc.cpp:2195`, `gecode/flatzinc/flatzinc.cpp:2197`, `gecode/flatzinc/flatzinc.cpp:2205`.

### INV-FZ-4: `mark_complete` forces global completion from slave side
If `mark_complete` is set, FlatZinc slave fails itself and returns `true` to signal global-space completion behavior.
Evidence: `gecode/flatzinc/flatzinc.cpp:2013`, `gecode/flatzinc/flatzinc.cpp:2015`, `gecode/flatzinc/flatzinc.cpp:2017`.

### INV-DRIVER-1: CLI policy forbids simultaneous restart mode and portfolio mode
Driver script run path exits if both `restart!=none` and `assets>0` are requested.
Evidence: `gecode/driver/script.hpp:302`, `gecode/driver/script.hpp:303`, `gecode/driver/script.hpp:304`.

### INV-DRIVER-2: PBS with RBS assets is valid via SEBs (not CLI mode flags)
Test coverage demonstrates `PBS` containing RBS assets through SEBs, despite CLI-level disallowance of restart+portfolio option combination.
Evidence: `test/search.cpp:606`, `test/search.cpp:609`, `test/search.cpp:610`, `test/search.cpp:627`, `test/search.cpp:630`, `test/search.cpp:631`.

## Interaction Matrix (Engine/Stop/RBS/PBS/LNS)

| Event | Primary actor | Trigger condition | State mutation | `next()` / `stopped()` effect |
|---|---|---|---|---|
| Solution found | RBS (`Seq::RBS`) | inner `e->next()` returns non-null | `restart=true`; replace `last` clone | returns solution immediately; later call processes `RR_SOL` restart path. Evidence: `gecode/search/seq/rbs.cpp:83`, `gecode/search/seq/rbs.cpp:86`, `gecode/search/seq/rbs.cpp:88` |
| Slave fail limit hit | RBS restart stop wrapper | `s.fail > l` in `RestartStop::stop` | `e_stopped=true`; `m_stat.restart++` | current round stops; RBS treats as true restart with `RR_LIM` when condition matches. Evidence: `gecode/search/seq/rbs.cpp:42`, `gecode/search/seq/rbs.cpp:44`, `gecode/search/seq/rbs.cpp:99` |
| Meta stop hit | RBS restart stop wrapper | wrapped `m_stop->stop(m_stat+s,o)` true | `e_stopped=false` | `next()` returns `nullptr` without forced restart path; outer `stopped()` follows inner engine. Evidence: `gecode/search/seq/rbs.cpp:48`, `gecode/search/seq/rbs.cpp:49`, `gecode/search/seq/rbs.cpp:111`, `gecode/search/seq/rbs.cpp:149` |
| Slice exhausted (seq PBS) | `Seq::PortfolioStop` + `Seq::PBS` | `s.fail >= ssi.l` for each active slave in slice | `ssi.done=true`; after all exhausted `ssi.l += slice` | engine keeps rotating assets; not treated as hard stop. Evidence: `gecode/search/seq/pbs.cpp:44`, `gecode/search/seq/pbs.cpp:45`, `gecode/search/seq/pbs.hpp:127`, `gecode/search/seq/pbs.hpp:130` |
| Asset exhausted (par PBS) | `Par::PBS::report` | slave reports `s==nullptr` and `!slave->stopped()` | swap exhausted slave out of active set; `n_active--`; `tostop=true` | peers are stopped via shared stop; next cycle runs remaining active assets only. Evidence: `gecode/search/par/pbs.hpp:181`, `gecode/search/par/pbs.hpp:188`, `gecode/search/par/pbs.hpp:214`, `gecode/search/par/pbs.cpp:40` |
| Incomplete slave returns `false` | `Space::slave` contract + `Seq::RBS` | `complete=false` after `slave(mi)` | marks round incomplete | when inner search finishes without stop, RBS forces true restart (`RR_CMPL`). Evidence: `gecode/kernel/core.hpp:2103`, `gecode/search/seq/rbs.cpp:76`, `gecode/search/seq/rbs.cpp:90`, `gecode/search/seq/rbs.cpp:99` |
| Best-solution constrain propagation | RBS/PBS + underlying best engines | better solution found or external constrain call | propagate `constrain` to master/inner/other assets | narrows future solutions; unsupported in non-best engines throws `NoBest`. Evidence: `gecode/search/seq/rbs.cpp:124`, `gecode/search/seq/rbs.cpp:136`, `gecode/search/seq/pbs.hpp:155`, `gecode/search/par/pbs.hpp:272`, `gecode/search/engine.cpp:41` |
| No-goods extraction/posting | RBS + `Space::master` default | restart processing (`RR_SOL`, `RR_CMPL`, `RR_LIM`) | clear/recount `ng`; accumulate `m_stat.nogood`; default master posts nogoods | restart rounds include nogood transfer into master before reset. Evidence: `gecode/search/seq/rbs.cpp:60`, `gecode/search/seq/rbs.cpp:62`, `gecode/search/seq/rbs.cpp:65`, `gecode/search/seq/rbs.cpp:97`, `gecode/search/seq/rbs.cpp:101`, `gecode/kernel/core.cpp:851` |

Stop-path classification summary:
- `slice`: sequential PBS `s.fail >= ssi.l` path. Evidence: `gecode/search/seq/pbs.cpp:44`.
- `engine cutoff`: RBS per-round fail cutoff `s.fail > l`. Evidence: `gecode/search/seq/rbs.cpp:42`.
- `meta stop`: wrapped stop object in RBS/PBS wrappers. Evidence: `gecode/search/seq/rbs.cpp:48`, `gecode/search/seq/pbs.cpp:40`, `gecode/search/par/pbs.cpp:40`.

## Failure/Regression Patterns

Historical issues and the invariants they map to:
- Sequential portfolio ignored stop objects (mapped to `INV-STOP-2`, `INV-PBS-3`). Evidence: `changelog.in:1245`.
- Parallel portfolio double-deletion bug (mapped to `INV-PBS-4`, `INV-PBS-5`, `INV-PBS-6`). Evidence: `changelog.in:1481`.
- Portfolio race condition (mapped to `INV-PBS-5`, `INV-PBS-6`). Evidence: `changelog.in:1642`.
- Crashes with sequential/parallel portfolios using parallel or restart-based best-solution assets (mapped to `INV-DRIVER-2`, `INV-PBS-1`, `INV-PBS-2`). Evidence: `changelog.in:1709`.
- Restart engine data-structure deletion bug (mapped to `INV-RBS-1`, `INV-RBS-2`, lifecycle ownership in `RBS::~RBS`). Evidence: `changelog.in:2585`, `gecode/search/seq/rbs.cpp:152`.
- LNS failure when restarting before first solution (mapped to `INV-FZ-3` bootstrap branch `mi.last()==nullptr`). Evidence: `changelog.in:2842`, `gecode/flatzinc/flatzinc.cpp:2189`.
- Restart-based search root-failed leak (mapped to dead-engine/root-failed initialization paths). Evidence: `changelog.in:1561`, `changelog.in:1562`, `gecode/search/rbs.hpp:92`, `gecode/search/seq/dead.cpp:90`.

## Validation Checklist

Use this checklist to validate this document and any future code changes touching these invariants.

- [ ] Every invariant entry includes at least one file:line citation.
- [ ] Every stop path in RBS/PBS is classified as one of `slice`, `engine cutoff`, or `meta stop`.
- [ ] Every `slave(mi)==false` code path is explicitly documented as incomplete restart behavior.
- [ ] Sequential and parallel PBS behaviors are contrasted (slicing vs shared-stop-and-active-set).
- [ ] FlatZinc LNS bootstrap (`mi.last()==nullptr`) and post-solution (`mi.last()!=nullptr`) restart paths are both documented.
- [ ] Document terminology is consistent (`master`, `slave`, `asset`, `restart`, `slice`, `complete`, `incomplete`).
- [ ] No statement here contradicts cited source lines.

Test cases and scenarios to execute/review:
- [ ] Portfolio + SEB + RBS-asset representation is covered via `test/search.cpp` scenarios. Evidence: `test/search.cpp:606`, `test/search.cpp:609`, `test/search.cpp:610`, `test/search.cpp:630`, `test/search.cpp:631`.
- [ ] Incomplete LNS pattern is covered via `examples/photo.cpp` (`slave` returns `false` after `relax`). Evidence: `examples/photo.cpp:132`, `examples/photo.cpp:136`, `examples/photo.cpp:137`.
- [ ] FlatZinc on-restart annotations are covered via `test/flatzinc/on_restart_*` tests. Evidence: `test/flatzinc/on_restart_complete.cpp:45`, `test/flatzinc/on_restart_sol_int.cpp:45`, `test/flatzinc/on_restart_last_val_int.cpp:45`.
- [ ] Stop-threshold semantics are verified against both global stop and sequential PBS slice logic. Evidence: `gecode/search/stop.cpp:65`, `gecode/search/stop.cpp:75`, `gecode/search/seq/pbs.cpp:44`.
- [ ] Parallel synchronization invariants are verified against parallel PBS internals. Evidence: `gecode/search/par/pbs.hh:164`, `gecode/search/par/pbs.hpp:217`, `gecode/search/par/pbs.hpp:233`.

## Source Index (file:line references)

Core interfaces and contracts:
- `gecode/search.hh:748`
- `gecode/search.hh:759`
- `gecode/search.hh:762`
- `gecode/search.hh:766`
- `gecode/search.hh:768`
- `gecode/search.hh:800`
- `gecode/search.hh:921`
- `gecode/kernel/core.hpp:1615`
- `gecode/kernel/core.hpp:1625`
- `gecode/kernel/core.hpp:2066`
- `gecode/kernel/core.hpp:2091`
- `gecode/kernel/core.hpp:3099`
- `gecode/kernel/core.hpp:3116`
- `gecode/kernel/core.hpp:3145`

Default behavior and stop semantics:
- `gecode/search/stop.cpp:65`
- `gecode/search/stop.cpp:75`
- `gecode/search/stop.cpp:85`
- `gecode/search/stop.cpp:94`
- `gecode/search/worker.hh:74`
- `gecode/search/worker.hh:79`
- `gecode/search/worker.hh:82`
- `gecode/search/engine.cpp:39`
- `gecode/search/engine.cpp:44`
- `gecode/search/engine.cpp:48`
- `gecode/kernel/core.cpp:846`
- `gecode/kernel/core.cpp:851`
- `gecode/kernel/core.cpp:856`
- `gecode/kernel/core.cpp:864`

RBS internals:
- `gecode/search/rbs.hpp:84`
- `gecode/search/rbs.hpp:89`
- `gecode/search/rbs.hpp:100`
- `gecode/search/seq/rbs.hh:44`
- `gecode/search/seq/rbs.hh:72`
- `gecode/search/seq/rbs.hpp:42`
- `gecode/search/seq/rbs.hpp:71`
- `gecode/search/seq/rbs.cpp:40`
- `gecode/search/seq/rbs.cpp:55`
- `gecode/search/seq/rbs.cpp:63`
- `gecode/search/seq/rbs.cpp:90`
- `gecode/search/seq/rbs.cpp:99`
- `gecode/search/seq/rbs.cpp:119`
- `gecode/search/seq/rbs.cpp:141`
- `gecode/search/seq/rbs.cpp:152`

PBS internals (seq/par):
- `gecode/search/pbs.hpp:111`
- `gecode/search/pbs.hpp:114`
- `gecode/search/pbs.hpp:137`
- `gecode/search/pbs.hpp:172`
- `gecode/search/pbs.hpp:201`
- `gecode/search/pbs.hpp:247`
- `gecode/search/pbs.hpp:295`
- `gecode/search/seq/pbs.hh:41`
- `gecode/search/seq/pbs.cpp:39`
- `gecode/search/seq/pbs.cpp:44`
- `gecode/search/seq/pbs.hpp:97`
- `gecode/search/seq/pbs.hpp:111`
- `gecode/search/seq/pbs.hpp:119`
- `gecode/search/seq/pbs.hpp:127`
- `gecode/search/seq/pbs.hpp:130`
- `gecode/search/par/pbs.hh:164`
- `gecode/search/par/pbs.cpp:39`
- `gecode/search/par/pbs.hpp:170`
- `gecode/search/par/pbs.hpp:188`
- `gecode/search/par/pbs.hpp:210`
- `gecode/search/par/pbs.hpp:233`

Driver/FlatZinc/LNS/test coverage:
- `gecode/driver/script.hpp:302`
- `gecode/driver/script.hpp:307`
- `gecode/flatzinc/flatzinc.cpp:1744`
- `gecode/flatzinc/flatzinc.cpp:1747`
- `gecode/flatzinc/flatzinc.cpp:1871`
- `gecode/flatzinc/flatzinc.cpp:2013`
- `gecode/flatzinc/flatzinc.cpp:2188`
- `gecode/flatzinc/flatzinc.cpp:2197`
- `examples/photo.cpp:132`
- `examples/photo.cpp:137`
- `test/search.cpp:606`
- `test/search.cpp:609`
- `test/search.cpp:610`
- `test/search.cpp:630`
- `test/search.cpp:631`
- `test/flatzinc/on_restart_complete.cpp:45`
- `test/flatzinc/on_restart_sol_int.cpp:45`
- `test/flatzinc/on_restart_last_val_int.cpp:45`

Regression history:
- `changelog.in:1245`
- `changelog.in:1481`
- `changelog.in:1561`
- `changelog.in:1562`
- `changelog.in:1642`
- `changelog.in:1709`
- `changelog.in:2585`
- `changelog.in:2842`
