# ── Stage 1: Build Go binary ──
FROM golang:1.22-alpine AS builder

WORKDIR /app

COPY go.mod go.sum* ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o /lumina-backend .

# ── Stage 2: Minimal runtime ──
FROM alpine:3.20

RUN apk add --no-cache ca-certificates tzdata curl

ENV TZ=Asia/Shanghai
ENV PORT=8080
ENV MQTT_HOST=localhost
ENV MQTT_PORT=1883
ENV MQTT_TLS=false
ENV MQTT_USERNAME=
ENV MQTT_PASSWORD=
ENV SQLITE_PATH=/data/lumina.db
ENV SQLITE_DIR=/data

VOLUME [ "/data" ]

EXPOSE 8080

COPY --from=builder /lumina-backend /usr/local/bin/lumina-backend

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD curl -f http://127.0.0.1:${PORT}/health || exit 1

ENTRYPOINT [ "/usr/local/bin/lumina-backend" ]
