# OBS용 Warudo RhyLive 캘리 버튼

OBS **제어창 → 설정 바로 아래**에서 버튼 하나로 RhyLive를 재보정합니다.

[English](README.md) · [다국어 빠른 시작](docs/QUICKSTART.md)

별도의 제어창이나 상주 프로그램 없이 동작합니다. 같은 PC에서 와루도는 실행 중이어야 합니다.

## 기능

- OBS 기본 버튼과 어울리는 크기·테마로 표시됩니다.
- OBS 언어 설정에 따라 한국어, 영어, 일본어, 중국어 간체·번체, 독일어, 프랑스어, 스페인어, 브라질 포르투갈어를 사용합니다.
- 지원하지 않는 언어는 영어, 같은 언어의 다른 지역 설정은 해당 언어 번역으로 표시합니다.
- 보정 결과와 오류를 버튼·상태 표시줄에서 확인할 수 있습니다.
- 번역을 DLL에 포함하므로 설치할 파일은 하나입니다.

## 설치

1. OBS를 완전히 종료합니다.
2. Windows 배포 ZIP을 OBS 설치 폴더에 풉니다. `obs-plugins/64bit/warudo-calibrate.dll` 구조를 유지합니다.
3. OBS와 와루도를 실행하고 RhyLive Receiver 하나가 활성화된 장면을 엽니다.
4. 정면을 바라보고 머리를 고정한 뒤 **설정 아래의 캘리**를 누릅니다.

기본 설치 위치의 예:

```text
C:\Program Files\obs-studio\obs-plugins\64bit\warudo-calibrate.dll
```

다른 위치에 설치했다면 해당 OBS 폴더의 `obs-plugins\64bit`를 사용합니다. 번역 파일이나 Qt DLL을 추가로 복사할 필요는 없습니다.

업데이트할 때는 OBS 종료 후 DLL을 교체합니다. 삭제할 때는 OBS 종료 후 `warudo-calibrate.dll`만 제거합니다.

## 호환성과 검증 범위

Windows x64, OBS 33.0.0, Qt 6.11.1 MSVC x64를 대상으로 빌드합니다. 다른 버전의 호환성은 확인하지 않았습니다.

현재 장면에서 활성화된 RhyLive Receiver가 하나일 때 동작합니다. 와루도의 내부 에디터 통신을 사용하므로 와루도 업데이트에 따라 수정이 필요할 수 있습니다.

빌드, 다국어 표시, 버튼 위치, 가상 로컬 API 요청·응답을 테스트합니다. 실제 와루도 세션에서의 보정은 안정 버전 공개 전에 추가 확인이 필요합니다.

## 문제 해결

| 상태 | 확인할 내용 |
| --- | --- |
| 버튼이 안 보임 | 실행 중인 OBS의 설치 폴더에 넣었는지 확인하고 재시작합니다. OBS 로그에서 플러그인 로드 오류를 확인합니다. |
| 와루도 연결 안 됨 | 같은 PC에서 와루도를 실행하고 장면 로딩을 기다립니다. |
| 와루도 인증 확인 | 와루도를 재실행한 후 다시 누릅니다. |
| RhyLive 없음 | 현재 장면에 활성화된 RhyLive Receiver가 있는지 확인합니다. |
| RhyLive 선택 필요 | 사용할 Receiver 하나만 활성화합니다. |
| 언어가 다르게 표시됨 | OBS 언어를 변경한 뒤 OBS를 재시작합니다. |

## 소스 빌드와 번역

Visual Studio 2022 C++ 도구, Windows SDK, CMake 3.22 이상, Python 3.10 이상, Qt 6.11.1 MSVC x64가 필요합니다. 빌드 명령은 [영문 README](README.md#build-from-source)에 있습니다.

`locale/*.json`에서 번역을 수정한 뒤 다시 빌드합니다. 키 누락, 중복 키, 빈 문구는 빌드 과정에서 검사합니다. 테스트는 임시 로컬 서버와 가짜 토큰을 사용하며 실제 OBS·와루도에 접속하지 않습니다.

`python scripts/package.py build/Release/warudo-calibrate.dll dist`로 소스·배포 ZIP과 SHA-256 체크섬을 생성할 수 있습니다. 검증 범위는 [VALIDATION.md](docs/VALIDATION.md)에 정리했습니다.

## 연결 정보와 라이선스

같은 PC의 `localhost:19052`, `localhost:19053`에만 연결합니다. 와루도 로컬 로그에서 현재 실행의 인증 토큰을 읽으며, 로그를 업로드하거나 토큰을 저장하지 않습니다. 장면을 저장하거나 트래킹 설정을 변경하지 않습니다.

소스와 스크립트에는 코드 주석을 포함하지 않습니다. GPL-2.0-or-later이며 전문은 [LICENSE](LICENSE)에 있습니다. OBS Project 및 와루도 개발사와 관계없는 독립 프로젝트입니다.
