# TOTK Explorer v3.0

Оверлей Tesla для The Legend of Zelda: Tears of the Kingdom.

Цель:
- Version 1.4.3
- Title ID 0100F2C0115B6000
- Build ID 277178B7DBA1B6D4

## Что уже реализовано

1. Подключение к Atmosphère `dmnt:cht`.
2. Проверка Title ID текущего процесса.
3. Чтение heap процесса.
4. Автоматический поиск кандидатов `[float X, float Y, float Z]`.
5. Двухэтапная калибровка: движение → изменение высоты.
6. Выбор наиболее стабильного кандидата.
7. Сохранение профиля по BID на SD.
8. Повторное чтение координат по сохранённому профилю.
9. Определение слоя: Sky / Surface / Depths.
10. Динамическая локальная ASCII-карта внутри Tesla:
    `@` = Link, `S` = Shrine, `K` = Korok, `L` = Lightroot, `T` = Tower.
11. `Nearby` с расстоянием до объектов из `points.csv`.

## Важная честная оговорка

Автоматический memory scan — эвристика. Он может получить ложных кандидатов. Поэтому v3 не считает первый найденный float автоматически истинными координатами: сначала выполняются два контрольных измерения.

Полная база Shrines/Koroks/Lightroots и т. п. не зашита в исходник. Для неё предусмотрен внешний файл:

`sd:/switch/totk_explorer/points.csv`

Формат:

```text
Type,Name,X,Y,Z
Shrine,Example Shrine,100,25,-200
Korok,Example Korok,140,25,-230
Lightroot,Example Lightroot,-300,-400,700
Tower,Example Tower,800,100,300
```

Так база может обновляться независимо от `.ovl`.

## Управление

В главном меню:
- A — открыть раздел.
- B — назад/закрыть Tesla.

Auto Discovery:
- A на `Start / Restart Scan` — запустить скан.
- После `Scan 1 complete` пройтись с Link.
- X — снять второй снимок.
- После этого подняться/прыгнуть.
- X — новый снимок движения.
- Y — сбросить калибровку в других состояниях.

## Сборка

Нужны devkitPro/devkitA64/libnx.

```bash
make setup
make
```

`make setup` получает актуальные libtesla и `libdmntcht.a`.

GitHub Actions уже настроен и после запуска workflow отдаёт готовый `TOTK-Explorer-v3.ovl` в artifact.

## Установка

```text
sd:/switch/.overlays/TOTK-Explorer-v3.ovl
sd:/switch/totk_explorer/points.csv
```

Запусти TOTK 1.4.3, открой Tesla и выбери `TOTK Explorer v3`.

## Почему нет записи в память

v3 — read-only Explorer. Он не пишет память игры и не изменяет сохранение.

## Дальнейшие расширения

- полноценная PNG/vector карта Hyrule/Sky/Depths;
- импорт открытых/неоткрытых collectibles из сохранения;
- live progress;
- поиск по объектам;
- waypoint и навигация;
- отдельные memory profiles для следующих BID.
