# mini-api-gateway-c

`mini-api-gateway-c`는 C로 구현한 경량 API Gateway / Reverse Proxy 프로젝트입니다.
HTTP 요청을 직접 읽고 파싱한 뒤, 요청 경로의 prefix에 따라 알맞은 업스트림 서비스로 전달합니다.

이 프로젝트는 운영 환경용 API Gateway를 목표로 하지 않습니다. C 소켓 프로그래밍, HTTP 요청 흐름, reverse proxy 구조, 경로 기반 라우팅을 학습하고 포트폴리오로 설명하기 위해 작고 명확하게 만든 프로토타입입니다.

## 주요 목표

- C 소켓 기반 TCP 서버 구현
- HTTP request line과 일부 header 파싱
- forward proxy와 reverse proxy의 차이 이해
- 경로 prefix 기반 업스트림 라우팅 구현
- 게이트웨이 오류 응답 처리
- Docker Compose 기반 end-to-end 테스트 환경 구성

## 주요 기능

- 설정 가능한 gateway port에서 요청 수신
- 클라이언트 연결마다 worker thread 생성
- HTTP method, target, version, Host header 파싱
- `/users`, `/orders` 같은 경로 prefix 기준 라우팅
- 업스트림 서비스로 HTTP 요청 전달
- reverse proxy 동작에 맞춰 일부 header 재작성
- 업스트림 응답을 클라이언트로 relay
- 라우트 미매칭 시 `404`, 업스트림 연결 실패 시 `502` 반환

## 예시 라우팅

기본 실행 환경:

- `/users` -> `127.0.0.1:9001`
- `/orders` -> `127.0.0.1:9002`

Docker Compose 환경:

- `/users` -> `users-service:9001`
- `/orders` -> `orders-service:9002`

라우팅 설정은 `routes.conf`와 `ROUTES_CONFIG` 환경 변수를 통해 변경할 수 있습니다.

## 프로젝트 구조

```text
mini-api-gateway-c/
├── Makefile
├── README.md
├── Dockerfile
├── docker-compose.yml
├── routes.conf
├── docs/
│   └── architecture.md
├── include/
│   ├── http.h
│   ├── proxy.h
│   └── route.h
└── src/
    ├── http.c
    ├── main.c
    ├── proxy.c
    └── route.c
```

## 빌드

```bash
make
```

## 실행

```bash
./mini-api-gateway-c
./mini-api-gateway-c 8081
```

## Docker 실행

게이트웨이 이미지만 빌드하고 실행:

```bash
docker build -t mini-api-gateway-c:dev .
docker run --rm -p 8080:8080 mini-api-gateway-c:dev
```

모의 업스트림 서비스까지 함께 실행:

```bash
docker compose up --build
```

다른 터미널에서 요청 테스트:

```bash
curl http://127.0.0.1:8080/users/1
curl http://127.0.0.1:8080/orders/42
curl http://127.0.0.1:8080/unknown
```

종료:

```bash
docker compose down
```

## 스모크 테스트

```bash
make smoke-test
```

`scripts/smoke_test.sh`는 `/users`, `/orders`, `/unknown` 요청을 통해 정상 라우팅과 `404` 응답을 확인합니다.

## 실행 테스트 예시

Docker Compose로 전체 서비스를 실행합니다.

```bash
cd /home/liliy456/Desktop/mini-api-gateway-c
docker compose up --build
```

다른 터미널에서 정상 라우팅을 확인합니다.

```bash
curl http://127.0.0.1:8080/users/1
```

예상 응답:

```json
{"service": "users-service", "method": "GET", "path": "/users/1", "message": "response from users-service"}
```

```bash
curl http://127.0.0.1:8080/orders/42
```

예상 응답:

```json
{"service": "orders-service", "method": "GET", "path": "/orders/42", "message": "response from orders-service"}
```

등록되지 않은 경로는 `404`를 반환합니다.

```bash
curl -i http://127.0.0.1:8080/unknown
```

예상 응답:

```text
HTTP/1.1 404 Not Found
Content-Type: text/plain; charset=utf-8
Content-Length: 46
Connection: close

No upstream route matched the requested path.
```

스모크 테스트를 실행하면 다음과 같이 출력됩니다.

```bash
make smoke-test
```

예상 출력:

```text
smoke tests passed
```

테스트가 끝나면 Compose stack을 종료합니다.

```bash
docker compose down
```

## 자주 발생하는 오류

`make: *** No rule to make target 'smoke-test'. Stop.`

이 오류는 보통 `mini-api-gateway-c` 폴더가 아닌 다른 폴더에서 `make smoke-test`를 실행했을 때 발생합니다. 초기 프로젝트인 `cproxy`의 `Makefile`에는 `smoke-test` 타깃이 없습니다.

```bash
cd /home/liliy456/Desktop/mini-api-gateway-c
make smoke-test
```

`curl: (7) Failed to connect to 127.0.0.1 port 8080`

게이트웨이가 아직 실행되지 않았거나 Docker Compose stack이 내려간 상태입니다.

```bash
docker compose up --build
```

`HTTP/1.1 502 Bad Gateway`

게이트웨이는 실행 중이지만 업스트림 서비스에 연결하지 못한 경우입니다. Docker Compose 환경에서는 `users-service`, `orders-service` 컨테이너가 함께 실행 중인지 확인합니다.

```bash
docker compose ps
```

`error during connect: open //./pipe/dockerDesktopLinuxEngine`

Windows 환경에서 Docker Desktop이 실행되지 않았거나 Docker Engine에 연결할 수 없을 때 발생합니다. Docker Desktop을 먼저 실행한 뒤 다시 명령을 실행합니다.

## 구현 흐름

1. `src/main.c`에서 게이트웨이 포트를 읽고 서버를 시작합니다.
2. `src/proxy.c`에서 listening socket을 만들고 클라이언트 연결을 accept합니다.
3. 각 클라이언트 연결은 별도 worker thread에서 처리됩니다.
4. `src/http.c`에서 HTTP 요청을 읽고 request line/header를 파싱합니다.
5. `src/route.c`에서 경로 prefix에 맞는 업스트림 라우트를 찾습니다.
6. 게이트웨이가 업스트림 서비스에 연결해 요청을 전달하고 응답을 클라이언트로 relay합니다.

## cproxy에서 발전한 점

이 프로젝트는 초기 구현인 `cproxy`를 바탕으로 확장했습니다. `cproxy`는 클라이언트가 요청 안에 목적지 서버를 지정하면 프록시가 해당 서버로 요청을 전달하는 forward proxy 구조였습니다. `mini-api-gateway-c`는 여기서 한 단계 발전해, 게이트웨이가 요청 경로를 기준으로 목적지 서비스를 직접 선택하는 reverse proxy / API Gateway 구조로 바뀌었습니다.

주요 개선 사항은 다음과 같습니다.

- forward proxy에서 reverse proxy / API Gateway 구조로 변경
- 클라이언트가 목적지를 지정하는 방식에서 게이트웨이가 라우팅을 결정하는 방식으로 변경
- `Host` header 기반 전달에서 `/users`, `/orders` 같은 경로 prefix 기반 라우팅으로 확장
- `route.c`, `route.h`를 추가해 라우팅 책임을 별도 모듈로 분리
- `routes.conf`와 `ROUTES_CONFIG` 환경 변수를 통해 라우팅 설정을 외부에서 주입할 수 있도록 개선
- Dockerfile과 Docker Compose를 추가해 게이트웨이와 모의 업스트림 서비스를 함께 실행할 수 있도록 구성
- `users-service`, `orders-service` 모의 서비스를 추가해 실제 서비스 라우팅 흐름을 재현
- `scripts/smoke_test.sh`를 추가해 정상 라우팅과 `404` 응답을 자동 검증
- `X-Forwarded-For`, `X-Forwarded-Proto`, upstream용 `Host` header 재작성 등 reverse proxy에 가까운 header 처리 추가
- socket timeout과 종료 signal 처리 등 실행 안정성 개선
- GitHub Actions CI를 추가해 빌드와 Docker Compose 기반 스모크 테스트를 자동화

정리하면 `cproxy`가 HTTP 프록시의 기본 동작을 이해하기 위한 프로젝트였다면, `mini-api-gateway-c`는 여러 백엔드 서비스 앞단에서 요청을 받아 서비스별로 분기하는 API Gateway 흐름을 학습하기 위한 프로젝트입니다.

## 현재 한계

- plain HTTP만 지원합니다.
- TLS termination은 구현하지 않았습니다.
- 인증, 인가, rate limiting은 없습니다.
- request body forwarding은 제한적입니다.
- load balancing이나 service discovery는 없습니다.
- thread-per-connection 구조라 대규모 트래픽 처리에는 적합하지 않습니다.

## 포트폴리오 설명 포인트

이 프로젝트는 C로 구현한 작은 reverse proxy gateway입니다. HTTP 요청을 직접 파싱하고 경로 기반으로 업스트림 서비스에 라우팅하며, Docker Compose와 모의 서비스를 사용해 end-to-end 동작을 검증합니다.

백엔드 개발과 인프라 학습 과정에서 네트워크 요청이 gateway를 통과해 서비스로 전달되는 흐름을 낮은 수준에서 이해하기 위해 만들었습니다.
