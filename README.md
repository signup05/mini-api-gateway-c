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

## 구현 흐름

1. `src/main.c`에서 게이트웨이 포트를 읽고 서버를 시작합니다.
2. `src/proxy.c`에서 listening socket을 만들고 클라이언트 연결을 accept합니다.
3. 각 클라이언트 연결은 별도 worker thread에서 처리됩니다.
4. `src/http.c`에서 HTTP 요청을 읽고 request line/header를 파싱합니다.
5. `src/route.c`에서 경로 prefix에 맞는 업스트림 라우트를 찾습니다.
6. 게이트웨이가 업스트림 서비스에 연결해 요청을 전달하고 응답을 클라이언트로 relay합니다.

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
