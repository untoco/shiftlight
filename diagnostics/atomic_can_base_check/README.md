# Проверка Atomic CAN Base

Безопасный диагностический скетч для `AtomS3R + Atomic CAN Base`.

Он запускает встроенный CAN-контроллер ESP32-S3 на GPIO 5/6 в режиме
`listen-only`: скетч не передаёт и не подтверждает CAN-кадры. На экране должно
появиться `CAN READY`.

Запуск из корня репозитория:

```bash
.tooling/platformio/bin/pio run -d diagnostics/atomic_can_base_check
.tooling/platformio/bin/pio run -d diagnostics/atomic_can_base_check --target upload --upload-port /dev/cu.usbmodem101
```

Этот тест подтверждает, что TWAI-контроллер запущен с правильными пинами. Сам
CAN-трансивер не имеет отдельного интерфейса для опроса, поэтому без шины он не
может подтвердить состояние CANH/CANL. Это проверяется только при подключении
к рабочей CAN-шине или второму CAN-узлу.
