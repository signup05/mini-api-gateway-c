# Architecture Notes

## 목적

`mini-api-gateway-c`는 초기 HTTP 프록시 구현을 바탕으로 만든 경량 reverse proxy gateway입니다. 클라이언트가 직접 목적지를 지정하는 forward proxy 방식에서 벗어나, 게이트웨이가 요청 경로를 기준으로 업스트림 서비스를 선택하는 구조를 학습하기 위해 만들었습니다.

## 요청 처리 흐름

1. `src/main.c`
   - 명령행 인자로 gateway port를 읽습니다.
   - 기본 포트는 `8080`입니다.
   - `run_gateway_server()`를 호출해 서버를 시작합니다.
2. `src/proxy.c`
   - listening socket을 생성합니다.
   - 클라이언트 연결을 accept합니다.
   - 연결마다 detached worker thread를 생성합니다.
   - HTTP 요청을 파싱하고 경로 기반 route를 찾습니다.
   - 선택된 업스트림 서비스에 연결합니다.
   - 업스트림 응답을 클라이언트로 relay합니다.
3. `src/http.c`
   - 클라이언트 요청을 읽습니다.
   - request line과 일부 header를 파싱합니다.
   - reverse proxy 전달용 upstream request를 다시 구성합니다.
   - `Connection`, `Proxy-Connection` 같은 hop-by-hop header를 제거합니다.
   - `Host`, `X-Forwarded-For`, `X-Forwarded-Proto` header를 재작성하거나 추가합니다.
4. `src/route.c`
   - built-in 기본 route를 제공합니다.
   - `ROUTES_CONFIG` 환경 변수가 있으면 `routes.conf` 형식의 파일에서 route를 읽습니다.
   - 요청 경로의 prefix와 route prefix를 비교해 업스트림 대상을 선택합니다.

## 라우팅 모델

라우팅 설정은 다음 형식을 사용합니다.

```text
path_prefix upstream_host upstream_port service_name
```

예시:

```text
/users users-service 9001 users-service
/orders orders-service 9002 orders-service
```

Docker Compose 환경에서는 `ROUTES_CONFIG=/app/routes.conf`를 사용해 컨테이너 내부 설정 파일을 읽습니다. 로컬 실행에서 별도 설정이 없으면 built-in 기본 route인 `127.0.0.1:9001`, `127.0.0.1:9002`를 사용합니다.

## 설계 선택

### Reverse Proxy 구조

초기 `cproxy`는 클라이언트 요청의 absolute URI 또는 `Host` header를 기준으로 목적지를 찾는 forward proxy였습니다. 현재 프로젝트는 gateway가 `/users`, `/orders` 같은 경로 prefix를 기준으로 목적지 서비스를 선택합니다. 이 구조는 여러 백엔드 서비스 앞단에서 요청을 분기하는 API Gateway 흐름을 설명하기 좋습니다.

### Thread-Per-Connection

각 클라이언트 연결을 별도 thread에서 처리합니다. 대규모 트래픽 처리에는 효율적이지 않지만, accept 이후 요청 처리 흐름과 concurrency model이 명확하게 드러나 학습용으로 적합합니다.

### 파일 기반 Route 설정

라우팅은 built-in 기본값과 `routes.conf` 기반 외부 설정을 함께 지원합니다. 동적 reload는 없지만, Docker Compose 환경에서 서비스 이름과 포트를 주입해 end-to-end 테스트를 구성할 수 있습니다.

### 최소 Header 재작성

게이트웨이는 reverse proxy 동작에 필요한 최소 header만 다룹니다.

- upstream용 `Host` header 재작성
- 기존 `Connection`, `Proxy-Connection` 제거
- `X-Forwarded-For` 추가
- `X-Forwarded-Proto: http` 추가

## 검증 구조

Docker Compose는 세 개의 서비스를 실행합니다.

- `gateway`: C로 구현한 API Gateway
- `users-service`: Python 기반 모의 업스트림 서비스
- `orders-service`: Python 기반 모의 업스트림 서비스

`scripts/smoke_test.sh`는 `/users/1`, `/orders/42`, `/unknown` 요청을 통해 정상 라우팅과 `404` 오류 응답을 확인합니다. GitHub Actions CI도 같은 흐름으로 빌드, Compose 실행, healthcheck, smoke test를 수행합니다.

## 현재 한계

- plain HTTP만 지원합니다.
- TLS termination은 없습니다.
- 인증, 인가, rate limiting은 없습니다.
- request body forwarding은 제한적입니다.
- load balancing, retry, circuit breaker는 없습니다.
- route 설정의 동적 reload는 없습니다.
- thread-per-connection 방식이라 고성능 gateway 구조는 아닙니다.

이 한계들은 의도적으로 남겨둔 범위입니다. 목표는 production gateway를 만드는 것이 아니라, gateway 내부 요청 흐름과 시스템 프로그래밍 요소를 작고 설명 가능한 형태로 구현하는 것입니다.
