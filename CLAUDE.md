# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Project_GJ is a top-down-camera run-and-gun / hack-and-slash game built in Unreal Engine 5.8 (`Project_GJ.uproject` → `EngineAssociation: 5.8`), C++-first with a thin Blueprint layer on top. Solo project, gameplay logic lives in C++; Blueprints mostly wire meshes/animations/data-table references onto the C++ base classes.

Engine install used for this project: `C:\Program Files\Epic Games\UE_5.8`.

## Build

There is no separate test suite — verification is "does it compile" plus manual play-in-editor testing.

- The editor is normally open with **Live Coding** active during development. UBT (`Build.bat`) will refuse to build while Live Coding holds the lock ("Unable to build while Live Coding is active") — ask the user to either press **Ctrl+Alt+F11** in-editor to compile via Live Coding, or close the editor first.
- Full external build (only when the editor is closed):
  ```
  "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Project_GJEditor Win64 Development -Project="C:\Project_GJ\Project_GJ.uproject" -WaitMutex
  ```
- UHT (header tool) errors show up early in that log if a `UCLASS`/`UPROPERTY`/`UFUNCTION`/delegate declaration is malformed — a clean UHT pass doesn't guarantee the full C++ compiles, but it does confirm reflection macros are syntactically valid.

## Editing source files: encoding

All existing Korean comments in `Source/Project_GJ/*.cpp/.h` are **CP949/EUC-KR**, not UTF-8 — they render as mojibake through UTF-8-based tools but the files are valid CP949 on disk. Don't let a UTF-8 write silently re-encode/corrupt them; when editing near existing Korean comments, preserve them as-is rather than rewriting.

## Architecture

Custom gameplay code lives under `Source/Project_GJ/*.h/.cpp` (root level of the module). **`Source/Project_GJ/Variant_Strategy/` and `Variant_TwinStick/` are unused Epic "Top Down" template leftovers** — not part of the actual game, don't treat them as reference for how this game works. Same split in `Content/`: `Content/GJ/` is the real custom content; `Content/Variant_Strategy/` and `Content/Variant_TwinStick/` are template leftovers.

### Character hierarchy

```
AGJBaseCharacter (abstract, ACharacter + IAbilitySystemInterface)
  - Owns UCharacterStateComponent + UMotionWarpingComponent
  - Owns MaxHP/CurrentHP + TakeDamage() override + OnDamaged/OnDeath hooks (see Combat/Damage below)
  - GetAbilitySystemComponent() currently returns nullptr — GAS modules are in Project_GJ.Build.cs
    (GameplayAbilitySystem/GameplayTags/GameplayTasks) but no ASC/AttributeSet is wired up anywhere yet.
  ├─ AGJCharacter — player (BP: Content/GJ/BluePrint/BP_GJCharacter)
  │   - Mouse-aim rotation: deprojects cursor to world, intersects the character's ground plane, rotates toward it
  │   - Camera pulls toward the mouse side (UpdateCameraOffset/ApplyCameraOffset) on top of a top-down spring arm
  │   - Enhanced Input actions (assets in Content/GJ/Input/IMC_GJ, IA_Move/IA_Dodge/IA_Attack): Move, Dodge, Attack
  │   - 8-direction dodge: picks Forward/Back/Left/Right montage by angle to move input, uses MotionWarpingComponent
  │     to warp to a target point ahead of the dodge
  │   - Melee combo: AttackInputPressed starts combo, jumps through montage sections named "Attack1"/"Attack2"/...;
  │     AdvanceCombo()/ResetCombo() are called from Blueprint/AnimNotify hooks in the attack montage
  │   - Per-level stats pulled from a DataTable (`DT_CharacterStat`, row = level number, struct FCharacterStat)
  │     via UpdateCharacterStat() — also drives MaxHP/CurrentHP
  │   - EquipWeapon() spawns DefaultWeaponClass and attaches it to the mesh's "WeaponSocket"
  └─ AGJEnemyCharacter — enemy (BP: Content/GJ/BluePrint/BP_GJEnemyCharacter). Currently a near-empty skeleton:
      AI auto-possess, orients to movement, no attack behavior yet.
```

### Weapons / projectiles

```
AGJWeaponBase (abstract AActor)
  - FDataTableRowHandle (WeaponDataHandle) resolves a FWeaponStat row (DT_WeaponStat) in OnConstruction,
    which supplies BaseDamage, AttackSpeedRate, WeaponMeshAsset, AttackMontageAsset
  - Fire() is a no-op virtual for subclasses to implement
  └─ AGJWeapon_Ranged (BP: BP_GJWeapon_Ranged)
      - Object-pools AGJProjectile (default PoolSize = 30, class from BP_GJProjectile) instead of spawn/destroy
        per shot — CreateProjectilePool() in BeginPlay, GetAvailableProjectile() scans for an inactive one
      - Fire() pulls an inactive projectile from the pool, positions it at the "MuzzleSocket", and calls
        FireInDirection(direction, WeaponStat.BaseDamage)

AGJProjectile
  - Sphere collision (CollisionComp, profile "Projectile") + static mesh + UProjectileMovementComponent
  - FireInDirection(dir, damage) activates it (unhides, re-enables collision/tick, sets velocity)
  - Deactivate() hides/disables collision+tick+velocity instead of Destroy(), returning it to the pool
  - OnHit applies damage via UGameplayStatics::ApplyDamage (skips self/instigator) then Deactivate()s
```

`AnimNotify_GameplayEvent` is a custom AnimNotify with an `EGameplayNotifyType` enum (Fire/Footstep/Reload/ShellEject/MeleeHit) set per-notify in the montage editor; the `Fire` case calls `Character->PerformFire()` so the projectile spawn is timed to the attack montage rather than the input press. Only `Fire` is implemented — the others are stubbed.

`AMyGJWeaponBase` is an empty unused stub subclass of `AGJWeaponBase`.

### Combat / damage

Hit detection uses the engine-standard `AActor::TakeDamage` pipeline (not GAS — GAS isn't wired up, see above; adding it later is still compatible since `TakeDamage` stays the entry point):
- `AGJBaseCharacter::TakeDamage` reduces `CurrentHP`, broadcasts `OnDamaged` (BlueprintAssignable), and calls `HandleDeath()` at 0 HP (sets `ECharacterState::Dead` via the state component, disables movement/capsule collision, fires the `OnDeath` BlueprintImplementableEvent).
- Currently only ranged projectiles deal damage (`GJProjectile::OnHit` → `UGameplayStatics::ApplyDamage`). Melee combo attacks (`Attack1`/`Attack2`/... montage sections) have no hit detection yet — no trace/collision exists for them.
- `OnDamaged`/`OnDeath` are intentionally empty hooks for Blueprint-side hit reactions/VFX/death animation — nothing is bound to them yet.

### State machine

`UCharacterStateComponent` holds a single `ECharacterState` (Idle/Rolling/Attacking/Hit/Dead/Reloading/Dashing/Dodge/Attack) with a `SetState`/`GetState` API and an `OnStateChanged` delegate. Note the enum has near-duplicate values (`Rolling`/`Dodge`, `Attacking`/`Attack`) from incremental additions — check which one a given code path actually uses before assuming. There's no timed reset back to `Idle` for `Hit`-type states; only `Dodge`→`Idle` (on montage end) and terminal `Dead` are currently handled.

### Data assets (Content/GJ)

- `DataTables/DT_CharacterStat.uasset` — rows keyed by level number (as string), struct `FCharacterStat` (MaxHP, BaseAttackPower, RequiredEXP)
- `DataTables/DT_WeaponStat.uasset` — struct `FWeaponStat` (BaseDamage, AttackSpeedRate, WeaponMeshAsset, AttackMontageAsset)
- `Input/IMC_GJ.uasset` + `IA_Move`/`IA_Dodge`/`IA_Attack`/`IA_Look` — Enhanced Input assets
- `Level/TestLev.umap` — the test/dev level
- `BluePrint/BP_GJCharacter`, `BP_GJEnemyCharacter`, `BP_GJGameMode`, `BP_GJProjectile`, `BP_GJWeapon_Ranged` — BP subclasses of the corresponding C++ classes above

## What Claude can and can't touch directly

Claude can read/edit C++ source and text-based project files (`.h`/`.cpp`, `.uproject`, `.ini`, `Build.cs`) and run builds/git from the shell. Claude **cannot** open or drive the Unreal Editor UI — `.uasset`/`.umap` are binary formats, so Blueprint graph edits, data table row values, material graphs, and level actor placement all have to be done by the user inside the editor. When a change requires touching one of those, say so explicitly rather than assuming it's been done.
