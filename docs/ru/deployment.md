<p align="center">
  <a href="../deployment.md">🇬🇧 English</a> &nbsp;·&nbsp; <b>🇷🇺 Русский</b>
</p>

# Развёртывание

Демон — единственный компонент, который должен работать на машине, чей трафик
модифицируется. На машинах без графического интерфейса можно собрать только демон:

```bash
cmake --preset daemon-only
cmake --build --preset daemon-only
```

В результате получается один бинарник `deimosd` (`out/build/daemon-only/bin/deimosd`) со
статическим рантаймом C++ и без зависимостей от UI, CLI и тестов.

## Требования

| Платформа | Что нужно демону |
| --- | --- |
| Windows | zapret с `winws.exe`; WinDivert поставляется внутри zapret. Служба должна работать от `LocalSystem` или от учётной записи администратора |
| Хост Linux | zapret с `nfqws`/`tpws`, модуль `nfnetlink_queue` и `CAP_NET_ADMIN` + `CAP_NET_RAW` |
| Роутер Linux | то же самое плюс `nfqws`, подключённый к цепочке forward. Загрузка и выгрузка — разные пути трафика, поэтому роутеру обычно нужны два раунда, настроенных на оба направления |
| macOS | zapret, собранный для этой платформы; демону нужны права root |

Если zapret установлен не в стандартном месте, укажите демону каталог установки явно:

```json
"zapret": { "installDirectory": "/opt/zapret" }
```

`deimos doctor` (или `GET /api/v1/zapret`) сообщает, что найдено и какие пути
проверялись.

## Первый запуск

```bash
deimosd --generate-token              # prints the token once, writes <state>/token
deimosd --config /etc/deimos/deimos.json --validate-config
deimosd --config /etc/deimos/deimos.json --foreground
```

Состояние и конфигурация находятся в:

| Платформа | Конфигурация | Состояние, журналы, токен |
| --- | --- | --- |
| Windows | `%APPDATA%\Deimos\deimos.json` | `%LOCALAPPDATA%\Deimos\` |
| Linux | `~/.config/deimos/deimos.json` | `~/.local/state/deimos/` |
| macOS | `~/Library/Application Support/Deimos/deimos.json` | тот же каталог |

Журналы записываются в `deimosd.log` в каталоге состояния в дополнение к истории в
памяти, которую отдаёт API.

### Какие команды пишут на диск

Файл конфигурации записывает только реальный запуск демона: при первом запуске `deimosd`
создаёт его со значениями по умолчанию, если путь отсутствует. Команды только для
чтения никогда не создают и не изменяют его — `--validate-config` проверяет файл (или
значения по умолчанию в памяти, когда путь не существует) и ничего не оставляет после
себя. `--generate-token` записывает только файл токена.

Демон поддерживает английский и русский. По умолчанию он выбирает язык интерфейса
системы; зафиксировать его можно через `daemon.language` в конфигурации или
`--lang auto|en|ru` в командной строке:

```bash
deimosd --config /etc/deimos/deimos.json --lang ru --validate-config
```

Та же опция есть у клиента и действует только в рамках этого запуска:

```bash
deimos status --lang ru
deimos --gui --lang ru
```

Порядок определения языка и список переведённых поверхностей см. в разделе
[Локализация](./localization.md).

## Служба Windows

Запустите консоль с повышенными правами:

```powershell
deimosd --install-service --config C:\ProgramData\Deimos\deimos.json
sc query deimosd
deimosd --uninstall-service
```

Служба регистрируется под именем `deimosd`, запускается автоматически, и установщик
сразу её запускает. Это обычная служба Windows: `sc stop deimosd`, `sc start deimosd` и
журнал событий работают как ожидается.

## systemd

```bash
deimosd --print-unit > /etc/systemd/system/deimosd.service
systemctl daemon-reload
systemctl enable --now deimosd
```

`deimosd --install-service` выполняет те же шаги автоматически при запуске от root.
Сгенерированный unit-файл даёт только `CAP_NET_ADMIN` и `CAP_NET_RAW`.

## launchd

```bash
sudo deimosd --print-plist > /Library/LaunchDaemons/com.deimos.daemon.plist
sudo launchctl load -w /Library/LaunchDaemons/com.deimos.daemon.plist
```

## Клиенты

```bash
deimos --gui                                  # desktop interface
deimos --endpoint http://192.168.1.1:8787 status
deimos --endpoint http://192.168.1.1:8787 start --profile tls-split
```

Сохранённые эндпоинты хранятся в `<config>/cli.json` (CLI) и `<config>/ui.json` (GUI):

```bash
deimos endpoints add router http://192.168.1.1:8787 --token-file /root/router.token
deimos endpoints use router
deimos doctor
```

Порядок поиска токена для обоих клиентов:

1. `--token`
2. `$DEIMOS_TOKEN`
3. `--token-file`
4. файл токена, записанный для выбранного эндпоинта
5. путь токена по умолчанию для платформы

## Публикация демона наружу

По умолчанию конфигурация слушает только loopback. Чтобы управлять удалённым демоном,
либо туннелируйте порт, либо явно включите удалённый доступ:

```json
"daemon": {
  "listenAddress": "0.0.0.0",
  "port": 8787,
  "allowRemoteClients": true,
  "requireAuthentication": true
}
```

Демон отказывается запускаться, если `allowRemoteClients` равно false для адреса, не
являющегося loopback, или если удалённый доступ сочетается с отключённой
аутентификацией.

Для всего, что выходит за пределы доверенной локальной сети, терминируйте TLS в обратном
прокси (Caddy, nginx, stunnel) или используйте SSH-туннель, оставив демон на loopback:

```bash
ssh -L 8787:127.0.0.1:8787 root@router
deimos status
```

Сборка с `-DDEIMOS_ENABLE_TLS=ON` и настройка `enableTls`, `tlsCertificateFile` и
`tlsPrivateKeyFile` позволяют демону отдавать HTTPS напрямую.

## Эксплуатация

| Задача | Команда |
| --- | --- |
| Проверка связи, аутентификации и zapret | `deimos doctor` |
| Просмотр того, что будет выполнено | `deimos profiles preview <id>` |
| Слежение за журналом демона | `deimos logs --follow` |
| Перезапуск движка после изменения правил | `deimos restart` |
| Смена активного профиля | `deimos profiles activate <id>` |
| Ротация токена | `deimosd --generate-token` и перезапуск демона |

После ротации токена перезапустите демон и обновите клиентов.

## Поиск и устранение неисправностей

| Симптом | Вероятная причина |
| --- | --- |
| `zapret: not detected` | `installDirectory` указан неверно либо в дереве нет `winws`/`nfqws`; журнал демона перечисляет все проверенные пути |
| `start failed: zapret 'winws' is not available` | профиль фиксирует инструмент, который не установлен на этом хосте; используйте `auto` |
| Движок переходит в `faulted` сразу после запуска | zapret завершился немедленно; в журнале есть его строки stderr под `zapret/<profile>` |
| `401` от клиента | файл токена отличается на клиенте и на демоне либо пользователь службы не может его прочитать |
| Трафик не меняется, хотя движок работает | на роутерах zapret требует правильного фильтра и интерфейса, а на Windows — прав администратора |

---

Перевод файла [deployment.md](../deployment.md). Синхронизировано с коммитом c679559.
