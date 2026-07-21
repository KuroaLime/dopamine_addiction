# Manager 병합 후 컴파일 오류 수정

작성일: 2026-07-21

- `UWeaponComponent::WeaponType` 보호 멤버 직접 접근을 공개 setter 호출로 변경했다.
- `WeaponType` 자체는 protected로 유지해 캡슐화를 보존했다.
- `AMainGameMode` 멤버와 충돌하던 지역 변수 `SpawnManager`를 `ActiveSpawnManager`로 변경했다.
- 중앙 스폰, 리스폰 재시도 및 라운드별 무기 선택 동작은 변경하지 않았다.
- 빌드는 별도로 수행해야 한다.
