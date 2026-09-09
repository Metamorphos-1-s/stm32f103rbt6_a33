# Firmware 0x050E runtime and persistent state separation

Firmware 0x050E separates transient display browsing from persistent
configuration ownership. The Register Map remains `0x0104` and Schema remains
`2`; the persistent payload layout is unchanged.

## State domains

`runtime.weight_view` is now a volatile view selected by normal-page
FUNCTION/HASH navigation and by `COMMAND_SET_WEIGHT_VIEW`. The runtime setter
does not mark the configuration dirty and does not increment the persistence
revision. `config.display.default_weight_view` remains the power-on default.
Persistent Codec V1/V2 continues consuming the historical runtime-view byte for
layout compatibility, but writes and restores the configured default view so a
transient browse cannot hitchhike into a later SAVE.

Internal tare synchronization also has a no-revision path. Real TARE and CLEAR
TARE operations retain their existing persistent-retention behavior; internal
WeightEngine/SystemContext reconciliation cannot consume a configuration
revision.

## Root-cause fix

The 0x050D failure evidence showed one dP confirmation could reach revision
`N+2` when internal tare synchronization and the actual config apply both used
the dirty revision API. 0x050E keeps the ownership gate intact and removes the
internal synchronization increment. A single confirmed dP edit now increments
exactly once; runtime page navigation leaves the revision and dirty state
unchanged.

This document records software scope only. 0x050E has not yet been flashed or
hardware-validated, and PC Client identity re-freezing remains a separate
task.
