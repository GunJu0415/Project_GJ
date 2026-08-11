# M1: 런 루프 닫기 — 설계 문서

> 작성일: 2026-08-08
> 대상: Project_GJ (UE 5.8, C++ 우선 + 얇은 블루프린트 레이어)
> 상태: 승인됨, 구현 계획 작성 대기

---

## 1. 배경과 문제

Project_GJ는 **로그라이트(회차제)**를 지향한다. 플레이어가 죽으면 런이 끝나고, 허브로 돌아가 다음 런을 시작하는 구조다.

현재 상태를 확인한 결과, **게임 루프가 닫혀 있지 않다**:

- `AGJBaseCharacter::HandleDeath()`는 상태를 `Dead`로 바꾸고 이동/콜리전을 끄는 데서 끝난다. 게임오버 화면도, 재시작도 없다.
- `UGJGameInstance::RebirthCount`와 `IncrementRebirthCount()`는 선언돼 있지만 **호출하는 곳이 한 곳도 없다.**
- `FItemData::bPersistAcrossRuns`도 **한 번도 읽히지 않는다.**
- 레벨은 `TestLev` 하나뿐이고 전환이 없다.

즉 플레이는 되지만 지고, 성장하고, 다시 시작하는 것이 하나도 없다. 이 때문에 세이브/로드 같은 후속 시스템도 "무엇을 저장할지"를 정할 수 없는 상태였다.

**M1의 목표는 런의 시작과 끝을 정의해서, 이후 모든 메타 시스템(회차 인계, 세이브, 메타 프로그레션)이 기댈 경계를 만드는 것이다.**

---

## 2. 범위

### 포함

- 플레이어 사망 → 짧은 딜레이 → 게임오버 화면 → 허브 레벨로 이동
- 허브 레벨에서 포탈과 상호작용 → 새 런 시작 (전투 레벨로 이동)
- 런 종료 시 `RebirthCount` 증가

### 제외 (후속 마일스톤)

| 항목 | 마일스톤 |
|---|---|
| 인벤토리/무기 회차 인계 (`bPersistAcrossRuns`) | M3 |
| EXP·레벨업 (`RequiredEXP`) | M2 |
| 세이브/로드 | M4 |
| 런 클리어(승리) 조건 | M5 |
| 허브의 상점·영구 강화 | M6 |

M1에서 런은 **사망으로만** 끝난다.

---

## 3. 아키텍처 결정

### 결정: GameInstance가 런 상태를 소유하고, GameMode가 레벨별 흐름을 조율한다

허브와 전투 레벨을 오가야 하므로, **레벨 전환에도 살아남는 주체**가 런 상태를 들고 있어야 한다. UE의 수명 규칙상 이는 `GameInstance`다.

| 클래스 | 책임 | 수명 |
|---|---|---|
| `UGJGameInstance` | 런 상태(`RebirthCount`), 레벨 전환 실행 | 앱 전체 (레벨 전환에도 생존) |
| `AGJGameMode` | 사망 감지, 게임오버 연출, 입력 모드 전환 | 레벨마다 새로 생성 |
| `AGJCharacter` | "죽었다"고 알리기만 함 | 레벨마다 새로 생성 |

### 검토했으나 채택하지 않은 대안

**GameMode 단독 처리** — 흐름이 한 파일에 모여 읽기 쉽지만, GameMode는 레벨 전환 시 파괴되므로 `RebirthCount` 같은 런 간 상태를 들고 있을 수 없다. 결국 GameInstance를 끌어오게 되어 채택안으로 수렴한다.

**`UGJRunSubsystem` (GameInstanceSubsystem) 신설** — 분리는 가장 깔끔하나, 이미 `UGJGameInstance`에 `RebirthCount`가 있어 중복·이사 작업이 생긴다. 런 상태가 카운터 하나 수준인 현 규모에서는 과설계다. M6(메타 프로그레션)에서 런 상태가 복잡해지면 그때 옮겨도 늦지 않다.

---

## 4. 구성 요소

### 4.1 신규 C++ 클래스

#### `UGJGameOverWidget` (UUserWidget)

게임오버 화면. 기존 `UGJPlayerHUDWidget`/`UGJInventoryWidget`과 동일한 **C++ 베이스 + `BindWidget`** 패턴을 따른다 (블루프린트 이벤트 그래프에 로직을 두지 않음).

| 멤버 | 종류 | 설명 |
|---|---|---|
| `ReturnToHubButton` | `BindWidget`, `UButton*` | 클릭 시 허브로 이동 |
| `RunCountText` | `BindWidgetOptional`, `UTextBlock*` | 몇 회차였는지 표시 (선택) |

`NativeOnInitialized()`에서 버튼의 `OnClicked`에 핸들러를 바인딩하고, 핸들러는 `UGJGameInstance::EndRunAndReturnToHub()`를 호출한다.

#### `AGJRunPortal` (AActor, `IGJInteractable` 구현)

허브에 배치하는 "런 시작" 포탈. **기존 상호작용 시스템을 그대로 재사용**한다 — `AGJItemBase`/`AGJWeaponBase`와 같은 방식으로 `InteractionCollision`(Sphere, "Trigger" 프로필)을 두고, `Interact_Implementation()`에서 범위를 확인한 뒤 `UGJGameInstance::StartNewRun()`을 호출한다.

플레이어 쪽은 이미 `AGJCharacter::InteractInputPressed()`가 겹친 액터 중 `IGJInteractable` 구현체를 찾아 호출하므로 **추가 입력 작업이 없다.**

### 4.2 기존 클래스 확장

#### `UGJGameInstance`

```
UCLASS(config=Game)
```

| 멤버 | 설명 |
|---|---|
| `HubLevelName` (`FName`, `Config`) | 허브 레벨 이름 |
| `CombatLevelName` (`FName`, `Config`) | 전투 레벨 이름 |
| `EndRunAndReturnToHub()` | `RebirthCount++` 후 허브 레벨 열기 |
| `StartNewRun()` | 전투 레벨 열기 (카운트는 증가시키지 않음) |
| `IsHubLevel()` | 현재 열린 레벨이 허브인지 판정 (`HubLevelName`과 비교) |

**`RebirthCount`의 의미**: **종료된 런의 횟수**다. 게임을 처음 켰을 때 0이고, 첫 사망 후 1이 된다. 따라서 지금 진행 중인 런의 회차는 `RebirthCount + 1`이다. 증가 시점은 런이 **끝날 때**(`EndRunAndReturnToHub`) 한 곳뿐이며, `StartNewRun()`은 카운트를 건드리지 않는다. 이렇게 해야 "사망 → 자동 허브 이동"(위젯 미할당 시 폴백)에서도 카운트가 정확히 한 번만 오른다.

`UGJGameOverWidget::RunCountText`는 방금 끝난 런의 회차, 즉 증가 후의 `RebirthCount` 값을 표시한다.

레벨 이름을 `Config` 프로퍼티로 두는 이유: `Config/DefaultGame.ini`에서 값을 지정할 수 있어 **블루프린트 서브클래스를 새로 만들 필요가 없다.** 현재 `DefaultEngine.ini`가 네이티브 클래스(`/Script/Project_GJ.GJGameInstance`)를 직접 가리키고 있으므로, BP 서브클래스를 만들면 그 설정도 함께 바꿔야 한다.

트레이드오프: `FName` 기반이라 레벨 에셋 이름을 바꾸면 참조가 깨진다(컴파일 타임에 못 잡음). `TSoftObjectPtr<UWorld>`가 타입 안전하지만 에디터에서 직접 할당해야 한다. 레벨 이름이 자주 바뀌지 않는 현 단계에서는 설정 편의를 택한다.

#### `AGJGameMode`

| 멤버 | 종류 | 설명 |
|---|---|---|
| `GameOverWidgetClass` | `EditDefaultsOnly`, `TSubclassOf<UGJGameOverWidget>` | 이미 존재하는 `BP_GJGameMode`에서 할당 |
| `DeathToGameOverDelay` | `EditDefaultsOnly`, `float`, 기본 2.0 | 사망부터 화면까지 딜레이 |
| `OnPlayerDied()` | `public` | 캐릭터가 호출 |
| `ShowGameOverScreen()` | `protected` | 타이머 콜백. 위젯 생성·표시 + 입력 모드 전환 |
| `bRunEnded` | `protected`, `bool` | 중복 호출 방지 플래그 |
| `GameOverTimerHandle` | `protected`, `FTimerHandle` | 딜레이 타이머 |

#### `AGJCharacter::HandleDeath()`

`Super::HandleDeath()` 및 기존 `bIsAutoFiring = false` 처리 뒤에, GameMode에 사망을 알리는 호출 한 줄을 추가한다. 캐릭터는 게임 흐름(레벨 전환, 회차 카운트)을 알지 못한다.

---

## 5. 데이터 흐름

```
[전투 레벨]  CurrentHP <= 0
  → AGJBaseCharacter::TakeDamage() → HandleDeath()
       (이동 정지, 캡슐·메시 콜리전 해제 — 이미 구현되어 있음)
  → AGJCharacter::HandleDeath() → GameMode->OnPlayerDied()
  → AGJGameMode::OnPlayerDied()
       IsHubLevel()이면 즉시 반환
       bRunEnded 확인 후 DeathToGameOverDelay 타이머 시작
  → AGJGameMode::ShowGameOverScreen()
       위젯 생성 + AddToViewport + FInputModeUIOnly
  → 사용자가 ReturnToHubButton 클릭
  → UGJGameInstance::EndRunAndReturnToHub()
       RebirthCount++  →  OpenLevel(HubLevelName)

[허브 레벨]  포탈 범위에서 상호작용 입력
  → AGJCharacter::InteractInputPressed()  (기존 코드, 수정 없음)
  → AGJRunPortal::Interact_Implementation()
  → UGJGameInstance::StartNewRun()  →  OpenLevel(CombatLevelName)
```

### 설계 판단

**게임을 일시정지하지 않는다.** 사망 후에도 적은 계속 움직이지만, 플레이어의 캡슐·메시 콜리전이 이미 해제되어 있어 무해하다. 인벤토리 구현 당시 일시정지와 입력 모드 조합에서 포커스 관련 버그를 반복해서 겪었으므로(`FInputModeGameAndUI`의 포커스 유실, `SetGamePaused` 중 월드 타이머 정지 등), 여기서는 입력만 UI로 전환하고 정지는 걸지 않는다.

**인벤토리·무기를 명시적으로 초기화하지 않는다.** 레벨을 다시 여는 순간 캐릭터가 새로 스폰되므로 자동으로 초기 상태가 된다. "일부만 인계"는 M3에서 추가하며, 그때 이 자연 초기화가 올바른 기본값이 된다.

---

## 6. 에러 처리

모든 처리의 목적은 **소프트락 방지**다. 로그라이트 루프에서 가장 위험한 실패는 플레이어가 아무 조작도 할 수 없는 상태로 갇혀 게임을 재시작해야 하는 것이다.

| 상황 | 처리 | 이유 |
|---|---|---|
| `HubLevelName` 또는 `CombatLevelName`이 비어 있음 | `OpenLevel` 호출하지 않고 에러 로그 | 잘못된 이름으로 열면 빈 맵에 갇힌다 |
| `GameOverWidgetClass` 미할당 | 위젯 없이 딜레이 후 **자동으로 허브 이동** | 위젯 에셋을 아직 만들지 않아도 루프가 돌아간다 |
| `OnPlayerDied()` 중복 호출 | `bRunEnded` 플래그로 1회만 처리 | 타이머 중복 등록 방지 |
| GameMode 캐스팅 실패 | 로그만 남기고 무시 | 다른 GameMode를 쓰는 레벨에서 죽어도 크래시하지 않는다 |
| 허브 레벨에서 사망 | `UGJGameInstance::IsHubLevel()`이 참이면 `OnPlayerDied()`가 즉시 반환 | 현재 허브에는 적이 없지만, 허브에서 죽어 다시 허브로 이동하며 회차만 오르는 상황을 막는다 |

---

## 7. 검증 방법

이 프로젝트에는 자동화된 테스트 스위트가 없다. 검증은 **컴파일 통과 + 수동 PIE 확인**으로 한다.

1. Live Coding 컴파일 통과 (Ctrl+Alt+F11)
2. 전투 레벨에서 사망 → 약 2초 뒤 게임오버 위젯 표시 → 버튼 클릭 → 허브 레벨 로드
3. 허브에서 포탈에 상호작용 → 전투 레벨 로드
4. 2~3번을 **두 번 이상 반복** → 로그에서 `RebirthCount`가 1, 2로 증가하는지 확인
5. 새 런 시작 시 HP/MP·인벤토리·무기가 **초기 상태로 돌아가 있는지** 확인

5번은 "레벨 재로드로 자동 초기화된다"는 설계 가정을 실제로 검증하는 지점이므로 생략하지 않는다.

---

## 8. 필요한 에디터 작업

`.uasset`/`.umap`은 바이너리이므로 텍스트 편집으로 만들 수 없다. 언리얼 MCP 서버가 연결되어 있으면 상당 부분을 자동화할 수 있고, 실패하는 항목만 수동으로 처리한다.

| 작업 | 방법 |
|---|---|
| 허브 레벨 `.umap` 생성 + PlayerStart 배치 | MCP 시도 → 실패 시 수동 |
| `BP_RunPortal` (`AGJRunPortal` 상속) 생성 및 허브 배치 | MCP 시도 |
| `WBP_GameOver` (`UGJGameOverWidget` 상속) 생성, 버튼 이름을 `ReturnToHubButton`으로 지정 | MCP 시도 |
| `BP_GJGameMode`에 `GameOverWidgetClass` 할당 | MCP 시도 |
| `Config/DefaultGame.ini`에 레벨 이름 설정 | 텍스트 편집 |
| `Config/DefaultEngine.ini`의 `GameDefaultMap`·`EditorStartupMap` 정리 (현재 Epic 템플릿 레벨 `Lvl_TopDown`을 가리킴) | 텍스트 편집 |
| C++ 컴파일 (Live Coding) | 사용자 |

새로 만든 C++ 클래스(`UGJGameOverWidget`, `AGJRunPortal`)는 **먼저 컴파일해서 리플렉션에 등록한 뒤에야** 블루프린트 부모 클래스로 선택할 수 있다. 라이브 코딩만으로 새 UCLASS가 인식되지 않으면 에디터 재시작이 필요하다.

---

## 9. 후속 마일스톤과의 관계

M1이 정의하는 "런 경계"는 이후 마일스톤 전체의 전제가 된다.

```
M1 런 루프 닫기  ← 이 문서
 ├─ M2 인런 성장 (EXP/레벨업)        — 언제 리셋되는지가 M1에서 결정됨
 ├─ M3 회차 인계 규칙                — 무엇이 남는지를 정의
 │    └─ M4 세이브/로드              — M3에서 정한 것을 앱 재시작에도 유지
 │         └─ M6 메타 프로그레션
 └─ M5 스테이지 진행 (다중 레벨)      — 런에 구조 추가, 클리어 조건 도입

M7 근접 무기 + 히트 판정  ← 독립적, 순서 무관
```

M5에서 런 클리어(승리) 조건이 추가되면 `EndRunAndReturnToHub()`가 종료 사유를 받도록 확장될 수 있다. M1에서는 사망 경로만 존재하므로 지금은 매개변수를 두지 않는다.
