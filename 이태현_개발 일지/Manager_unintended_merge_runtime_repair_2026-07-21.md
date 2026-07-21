# Manager merge runtime repair (2026-07-21)

## Purpose

This repair restores runtime behavior that was unintentionally lost while resolving the five-file merge conflict. Friend-authored shop content remains in place. The health baseline is explicitly fixed at 150.

## Restored and repaired behavior

1. Fire ability
   - Restored the two server-only full-auto fields referenced by `Ability_Fire.cpp` but deleted from `Ability_Fire.h`.
   - Kept all client prediction state in `WeaponComponent`; it was not moved back into the shared ability CDO.
   - Added the missing owner validity guard during fire cancellation and reduced the per-shot bloom log to `VeryVerbose`.

2. Shop-to-battle movement recovery
   - `CardGateReady` now enters the existing `StartPreBattleShopPhase()` lifecycle.
   - Shop timeout now exits through `FinishPreBattleShopPhase()` so movement mode and controller input lock are restored.
   - Friend-authored spawn barriers remain active during the shop and are released inside the lifecycle helpers.
   - Shop RPCs now require both the `PreBattleShop` server phase and replicated `ShopAvailable` state.

3. Server-authoritative respawn
   - Respawn placement now uses `AMainGameMode::TeleportPlayerAuthoritatively()`.
   - HP is restored only after placement succeeds; unavailable streamed spawns cause a one-second retry instead of false success.
   - Movement, collision, visibility, weapon state, controller input and TPS UI are restored together.
   - A late death timer finishing outside battle restores health without forcing the client into TPS over the current phase.

4. Spawn pool recovery
   - Invalid streamed actor pointers are removed before use.
   - Empty pools trigger a world rescan, covering spawn levels that were not loaded during `BeginPlay()`.
   - A missing `CenterSpawner` falls back only to a valid regular spawn and leaves a diagnostic warning.

5. HP baseline and delegate lifecycle
   - `AMainPlayerState::BaseMaxHealth` is 150 and is now the single max-HP baseline for reset, regeneration and UI ratio calculations.
   - Health upgrades remain included through `GetCurrentMaxHP()`.
   - Duplicate HP/player-upgrade delegate bindings are removed before rebinding.

6. Character and weapon cleanup
   - Death and TPS phase exit cancel fire, aim, crouch and card-discard hold abilities.
   - Reload locks are explicitly cancelled on death/respawn.
   - `Super::UnPossessed()` is restored and weapon selection mutation is server-only.

## Intentionally preserved

- Friend-authored shop barriers and shop upgrade flow.
- The current test policy that fixes the round weapon to SMG.
- Client fire prediction state in `WeaponComponent`.
- Existing card loading gate, card drop placement and Dedi/IOCP control work.

## Verification

- All touched sources were backed up before deployment.
- Merge markers and stale bypass patterns were checked after deployment.
- This operation does not start an Unreal or IOCP build.
