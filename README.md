# Ember — Lab 03 «Адреси, а не імена», Standard

Навчальна 8-бітна машина C++17: 4096 байтів пам'яті, A/B (8 бітів),
PC/H (16 бітів), ALU, прапорці Z/N/C, інструкції довжиною 1–3 байти.
Виконано основну стежку M1–M4. Функціональність Lab 01–02 збережена.
Попередній README без змін збережено як [історичний звіт Lab 02](docs/lab-02-report.md).
Він описує стан до Lab 03; поточна поведінка задокументована тут.

Офіційні джерела:
[умова](https://github.com/rmalkevy/Programming-Practice-Projects/blob/main/courses/programming-fundamentals/lab-03-addresses-not-names.md),
[Notes](https://github.com/rmalkevy/Programming-Practice-Projects/blob/main/courses/programming-fundamentals/lab-03-addresses-not-names.notes.md),
[ISA](https://github.com/rmalkevy/Programming-Practice-Projects/blob/main/courses/programming-fundamentals/ISA.uk.md),
[перевірка](https://github.com/rmalkevy/Programming-Practice-Projects/blob/main/courses/programming-fundamentals/checks/lab-03.txt).
Файл `checks/lab-03.txt` відтворює офіційний сценарій. У власних демонстраціях
дані повні: `65 66 67 0`, починаючи з `0x800`; код починається з `0x000`.

## Збірка та запуск

Windows PowerShell, Visual Studio 18 2026, MSVC x64:

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
& $cmake -S . -B build -G 'Visual Studio 18 2026' -A x64
& $cmake --build build --config Debug
& $ctest --test-dir build -C Debug --output-on-failure
Get-Content checks/lab-03.txt | .\build\Debug\ember.exe
Get-Content checks/lab-03-trace.txt | .\build\Debug\ember.exe
```

У Debug увімкнено MSVC AddressSanitizer; DLL копіюється до виконуваних файлів.
Попередження `/W4 /WX`, стандарт C++17. `/RTC1` та інкрементне компонування
відключено через несумісність із ASan. `-DEMBER_MSVC_ASAN=OFF` — явний варіант
для систем без установленого компонента ASan; наведені результати отримано з ASan ON.
MSVC не підтримує UBSan: його запуск тут не заявляється.
Для GCC/Clang збережені налаштування Debug ASan/UBSan, але ці компілятори в цій
роботі не запускалися.

## Поточні файли

| Файли | Призначення |
|---|---|
| `src/memory.*` | Безпечний доступ до байтів і 16-бітних слів |
| `src/cpu.*` | PC, H, регістри, декодування, step/run, діагностика |
| `src/main.cpp` | Консольні команди |
| `src/alu.*`, `src/dump.*` | ALU Lab 02 і дамп Lab 01, без змін |
| `tests/test_ember.cpp` | Регресійні перевірки Lab 01–02 |
| `tests/test_lab03.cpp` | Слова, нові опкоди, довжини, прапорці, межі, run |
| `tests/pointer_experiments.cpp` | Безпечні досліди з вказівниками |
| `tests/asan_oob.cpp` | Ізольований ASan-дослід та виправлена гілка |
| `tests/check_cli.cmake`, `tests/capture_asan.cmake` | Перевірка CLI та збереження реального звіту |
| `checks/lab-03*.txt` | Офіційний сценарій і власні демонстрації |
| `evidence/` | Постійні матеріали дослідів: звіт ASan, код завершення, виправлення та вказівники |

## Команди хоста

| Команда | Результат |
|---|---|
| `get <addr>`, `set <addr> <byte>` | Читання/запис байта |
| `inc <addr>` | Збільшення байта пам'яті за модулем 256, як у Lab 01 |
| `get16 <addr>`, `set16 <addr> <word>` | Little-endian слово, допустимий початок 0..4094 |
| `dump` | Дамп усієї пам'яті з ASCII |
| `reg a <byte>`, `reg b <byte>` | Установлення A/B без зміни прапорців |
| `regs` | Десяткові PC, A, B, Z, N, C, H |
| `step` | Одна інструкція |
| `run [limit]` | Виконання до HALT, помилки або ліміту; типово 1 000 000 кроків |
| `help`, `quit`, `exit` | Довідка, вихід |

Числа: десяткові або `0x` hex. Новий процес починає з нульової пам'яті та CPU.
Після HALT наступні step/run відмовляють із `halted`, PC не змінюється.
Помилка CPU запам'ятовується в `error`; подальші step/run не виконують інструкцій.
Повторний запуск Ember створює свіжий стан. Помилки команд консолі виправляються
повторним уведенням і не переводять CPU у стан помилки.

Для сумісності Lab 01 функція `mem_get` повертає 0 за недопустимим індексом;
`mem_set` відмовляє. CPU перевіряє адреси самостійно до виклику цих функцій,
тому помилковий PC не перетворюється на випадковий HALT. Консоль `get16` явно
повідомляє про недопустиму пару, тоді як функція `get16` повертає 0.
Невідомі/ще не реалізовані байти лишаються однобайтовими NOP за каркасом Lab 02.
Це збережена поведінка каркаса, а не додаткові інструкції ISA. Стрибки, CMP,
екран, купу та стек майбутніх лабораторних не реалізовано.

## M1 — вказівники та little-endian

```cpp
int x = 65;
int* p = &x;
*p = 66;
```

`&x` бере адресу об'єкта, `p` зберігає її, `*p` звертається до самого `x`.
`&p` — адреса окремої змінної-вказівника; її значення відрізняється від `p`.
Схема: `p → x[65]`, після запису через `*p`: `p → x[66]`.
`nullptr` позначає відсутність об'єкта; перед розіменуванням перевіряємо вказівник.
`sizeof(p)` дає розмір вказівника, `sizeof(*p)` — розмір об'єкта.

`p + 1` переміщує на один елемент відповідного масиву: `int*` на `sizeof(int)`
байтів, `char*` або `Byte*` на один байт. Віднімання вказівників на елементи
одного масиву дає кількість елементів. Вказівник на позицію після кінця можна
утворити для порівняння, але розіменовувати його не можна.

`void*` зберігає адресу об'єкта без типу; перед розіменуванням потрібне повернення
до правильного типу. Арифметика `void*` не визначена стандартним C++.
`inc_copy(int)` змінює копію; `inc_ptr(int*)` отримує копію адреси, але через неї
може змінити об'єкт викликача. Читання `float` через `int*` порушує правила
доступу до типів; у досліді використано `std::memcpy` у `uint32_t`.

Досліди справді виконано під MSVC x64 з ASan:

```text
x=65 *p=65
after *p=66: x=66
nullptr check=1
sizeof(p)=8 sizeof(*p)=4 sizeof(void*)=8
int elements=10 20 30 distance=1 bytes per step=4
char next=B distance=1 bytes per step=1
void* roundtrip=1 value=66
after inc_copy=1
after inc_ptr=2
float bits via memcpy=0x3f800000
```

Рядок із фактичними адресами хоста також збережено в
[evidence/pointer-experiments.txt](evidence/pointer-experiments.txt).
Адреси можуть змінюватися між запусками; розмір вказівника залежить від платформи.

`get16` містить наданий у Lab 03 алгоритм: прочитати `lo`, потім `hi`,
повернути `lo | (hi << 8)`. Додано попередню перевірку `addr >= MEM_SIZE - 1`.
`set16` дзеркально записує `value & 0xFF`, потім `value >> 8`.
Перевірка до `addr + 1` не допускає також переповнення `size_t` на `SIZE_MAX`.
Недопустимий запис не змінює жодного байта; остання коректна пара — 4094/4095.

```text
set16 0 0x1234
get16 0
4660
dump
0000  34 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |4...............|
```

Це little-endian: молодший байт `34` знаходиться за меншою адресою.
Порядок байтів хоста не впливає на ручне складання слова.

## M2 — виконання інструкцій

Перед виконанням CPU перевіряє `mem != nullptr`, `PC < 4096`, повну довжину
інструкції й адресу даних для LOAD/STORE. При помилці змінюється лише поле
`error` і друкується діагностика з PC; регістри, PC та пам'ять не змінюються.
Успішний крок додає до PC довжину 1, 2 або 3, включно з HALT.
Остання однобайтова інструкція за 4095 може завершитися з PC=4096;
якщо це не HALT, наступна спроба читання дає помилку.

H зберігає всі 16 бітів; LOADH може завантажити навіть `0xFFFF`.
INCH/DECH рахують за модулем 65536. Це не загортання доступу до RAM:
розіменування H поза 0..4095 дає помилку, а не доступ за молодшими 12 бітами.
`run` має окремий лічильник виконаних кроків. Вичерпаний бюджет зупиняє CPU
з `instruction limit reached`; HALT рівно на останньому дозволеному кроці успішний.

## Реалізовані опкоди ISA

| Опкод | Інструкція | Байти | Дія / прапорці |
|---|---|---:|---|
| 00 | HALT | 1 | Зупинка, прапорці збережені |
| 01 | NOP | 1 | Прапорці збережені |
| 02 | OUT | 1 | A як символ, без нового рядка; прапорці збережені |
| 03 | OUTN | 1 | A як десяткове число і пробіл; прапорці збережені |
| 10 | ADD A,B | 1 | Z/N/C, перенесення |
| 11 | SUB A,B | 1 | Z/N/C, позика |
| 12 | AND A,B | 1 | Z/N, C=0 |
| 13 | OR A,B | 1 | Z/N, C=0 |
| 14 | XOR A,B | 1 | Z/N, C=0 |
| 15 | NOT A | 1 | Z/N, C=0 |
| 16 | SHL A | 1 | Z/N/C, у C старий біт 7 |
| 17 | SHR A | 1 | Z/N/C, у C старий біт 0 |
| 18 | INC A | 1 | Z/N, C збережено |
| 19 | DEC A | 1 | Z/N, C збережено |
| 20 | LOADI A,imm8 | 2 | A=imm8 |
| 21 | LOADI B,imm8 | 2 | B=imm8 |
| 22 | LOAD A,[addr16] | 3 | A=mem[addr] |
| 23 | LOAD B,[addr16] | 3 | B=mem[addr] |
| 24 | STORE [addr16],A | 3 | mem[addr]=A |
| 25 | STORE [addr16],B | 3 | mem[addr]=B |
| 26 | MOV A,B | 1 | A=B |
| 27 | MOV B,A | 1 | B=A |
| 28 | LOADH H,imm16 | 3 | H=imm16 |
| 29 | LOAD A,[H] | 1 | A=mem[H] |
| 2A | STORE [H],A | 1 | mem[H]=A |
| 2B | INCH | 1 | H=H+1 |
| 2C | DECH | 1 | H=H-1 |

Опкоди в таблиці шістнадцяткові. **Усі 20–2C зберігають Z/N/C**,
навіть завантаження нуля. Декодування з Lab 02 збережено:
`group = (op >> 4) & 0x0F`, `index = op & 0x0F`.

## M3 — абсолютні LOAD/STORE

Запуск: `Get-Content checks/lab-03-absolute.txt | .\build\Debug\ember.exe`.
Це наведення мнемонік для пояснення байтів; асемблера в цій лабі ще немає.

```text
0000: 22 00 08    LOAD A,[0x0800]     ; A=65
0003: 24 10 08    STORE [0x0810],A    ; копія A
0006: 23 01 08    LOAD B,[0x0801]     ; B=66
0009: 25 11 08    STORE [0x0811],B    ; копія B
000C: 20 00       LOADI A,0
000E: 22 10 08    LOAD A,[0x0810]     ; читання записаної копії
0011: 02          OUT
0012: 22 11 08    LOAD A,[0x0811]
0015: 02          OUT
0016: 03          OUTN
0017: 00          HALT
```

Фрагменти справжнього виводу (повторні запрошення консолі опущено):

```text
ember> AB66
ember> PC=24 A=66 B=66 Z=0 N=0 C=0 H=0
0000  22 00 08 24 10 08 23 01 08 25 11 08 20 00 22 10  |"..$..#..%.. .".|
0010  08 02 22 11 08 02 03 00 00 00 00 00 00 00 00 00  |..".............|
0800  41 42 43 00 00 00 00 00 00 00 00 00 00 00 00 00  |ABC.............|
0810  41 42 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |AB..............|
```

Код і дані лежать в одному масиві; поділ на регіони — домовленість.
STORE у власний код дозволено ISA і окремо перевірено тестом.

## M3 — сім інструкцій через H

`checks/lab-03-h.txt` виконує run, `checks/lab-03-trace.txt` — сім step із regs.

```text
0000: 28 00 08    LOADH H,0x0800
0003: 29          LOAD A,[H]
0004: 02          OUT
0005: 2B          INCH
0006: 29          LOAD A,[H]
0007: 02          OUT
0008: 00          HALT
```

| Крок | PC до | Інструкція | PC після | H після | A після | Вивід |
|---:|---|---|---|---|---:|---|
| 1 | 0x000 | LOADH | 0x003 | 0x0800 | 0 | |
| 2 | 0x003 | LOAD A,[H] | 0x004 | 0x0800 | 65 | |
| 3 | 0x004 | OUT | 0x005 | 0x0800 | 65 | A |
| 4 | 0x005 | INCH | 0x006 | 0x0801 | 65 | |
| 5 | 0x006 | LOAD A,[H] | 0x007 | 0x0801 | 66 | |
| 6 | 0x007 | OUT | 0x008 | 0x0801 | 66 | B |
| 7 | 0x008 | HALT | 0x009 | 0x0801 | 66 | |

B і Z/N/C залишаються 0 на всіх кроках. Реальний результат run:

```text
ember> AB
ember> PC=9 A=66 B=0 Z=0 N=0 C=0 H=2049
0000  28 00 08 29 02 2b 29 02 00 00 00 00 00 00 00 00  |(..).+).........|
0800  41 42 43 00 00 00 00 00 00 00 00 00 00 00 00 00  |ABC.............|
```

При step літери A/B з'являються перед наступним запрошенням `ember>`, бо OUT
не додає нового рядка. При run літери друкуються поруч: `AB`.
Повні дампи і трасу можна відтворити сценаріями `checks/lab-03-h.txt`,
`checks/lab-03-trace.txt`, `checks/lab-03-absolute.txt`. CTest зберігає їхній
вивід у `build/lab-03*-output.txt`; службові журнали не входять до Git.

## M4 — адреса хоста і гостя

`CPU::mem` — справжній `Memory*` процесу C++; `pc` та `h` — числа-індекси
всередині 4096 байтів цього об'єкта. Наприклад, гостьова адреса `0x0800`
відповідає хостовому `&mem.data[0x800]`, але ці значення не тотожні.
A/B зберігають байти, H зберігає місце; 8 бітів могли б назвати лише адреси
0..255, тому для H потрібен ширший тип. 16 бітів достатньо для 4096 комірок,
але не кожне з 65536 значень H є коректною адресою RAM.

Якщо записати хостовий вказівник у 16-бітний H, адреса 64-бітного процесу
обріжеться; використання такого числа дасть помилку меж або звернення до
неправильного байта. Збережений хостовий вказівник також не переноситься
між запусками/копіями VM. Індекси гостя мають стабільний зміст у пам'яті машини.

Аналог двох інструкцій у C++ для коректного діапазону:

```cpp
Byte* p = &mem.data[0x800];
Byte a = *p; // LOAD A,[H]
++p;         // INCH
```

## M4 — справжній AddressSanitizer

Експеримент знаходиться тільки у `tests/asan_oob.cpp`. Виконуваний файл
`asan_oob` збирається лише явно, через `EXCLUDE_FROM_ALL`, з макросом
`EMBER_ASAN_DEMO_UNSAFE`. Він не входить у Ember або звичайний CTest.
Звичайна ціль `bounds_fixed` із того самого окремого файлу компілює перевірку
меж, без небезпечного запису. Жодну перевірку в робочій VM для досліду не вимикали.

Відтворення під Debug із ASan ON:

```powershell
& $cmake --build build --config Debug --target asan_oob
& $cmake "-DDEMO=$PWD/build/Debug/asan_oob.exe" "-DREPORT=$PWD/evidence/asan-report.txt" -P tests/capture_asan.cmake
.\build\Debug\bounds_fixed.exe
```

Скрипт зберігає stdout/stderr без редагування та код завершення в сусідній
`asan-report.txt.exit.txt`. Він успішний лише при ненульовому коді процесу
і справжньому повідомленні ASan. На виконаному запуску код процесу — **1**.
Нижче повний фактичний звіт; адреси та PID належать саме цьому запуску.

```text
=================================================================
==19020==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x0011faeffa20 at pc 0x7ff68bd0124b bp 0x0011faefe9e0 sp 0x0011faefe9e8
WRITE of size 1 at 0x0011faeffa20 thread T0
    #0 0x7ff68bd0124a in main C:\Users\aaaan\Downloads\Programming-Practice-Projects-main\Programming-Practice-Projects-main\courses\programming-fundamentals\starter\tests\asan_oob.cpp:12
    #1 0x7ff68bd04808 in invoke_main D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:78
    #2 0x7ff68bd04751 in __scrt_common_main_seh D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288
    #3 0x7ff68bd0460d in __scrt_common_main D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:330
    #4 0x7ff68bd0487d in mainCRTStartup D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_main.cpp:16
    #5 0x7ffd0fa1dda6 in BaseThreadInitThunk+0x16 (C:\WINDOWS\System32\KERNEL32.DLL+0x18002dda6)
    #6 0x7ffd1129ee4b in RtlUserThreadStart+0x2b (C:\WINDOWS\SYSTEM32\ntdll.dll+0x18007ee4b)

Address 0x0011faeffa20 is located in stack of thread T0 at offset 4128 in frame
    #0 0x7ff68bd0102f in main C:\Users\aaaan\Downloads\Programming-Practice-Projects-main\Programming-Practice-Projects-main\courses\programming-fundamentals\starter\tests\asan_oob.cpp:8

  This frame has 1 object(s):
    [32, 4128) 'data' <== Memory access at offset 4128 overflows this variable
HINT: this may be a false positive if your program uses some custom stack unwind mechanism, swapcontext or vfork
      (longjmp, SEH and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-buffer-overflow C:\Users\aaaan\Downloads\Programming-Practice-Projects-main\Programming-Practice-Projects-main\courses\programming-fundamentals\starter\tests\asan_oob.cpp:12 in main
Shadow bytes around the buggy address:
  0x0011faeff780: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeff800: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeff880: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeff900: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeff980: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x0011faeffa00: 00 00 00 00[f3]f3 f3 f3 00 00 00 00 00 00 00 00
  0x0011faeffa80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeffb00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeffb80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeffc00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0011faeffc80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==19020==ABORTING
```

Причина: `data` займає 4096 байтів (`[32, 4128)` від початку кадру стека),
а `data[4096]` записує за позицією 4128 — першим байтом після масиву.
`WRITE of size 1` пояснює розмір запису, `main ... asan_oob.cpp:12` — місце помилки.
Це не попередження компілятора і не вигаданий очікуваний текст.

Виправлення: перевірити `index >= MEM_SIZE` **до** звернення до `data[index]`.
У `bounds_fixed` той самий індекс безпечно відхиляється, код завершення 0:

```text
rejected index 4096: valid range 0..4095
```

`asan_bounds_fixed` проходить у CTest під ASan. У VM відповідний принцип
застосовано до PC, адрес даних і двобайтових операндів. Повний первинний файл:
[evidence/asan-report.txt](evidence/asan-report.txt).

## Результати перевірок

Початковий Git-стан: чисте дерево, HEAD `5931ed2f8ca98ea5cc83e0e121ccdb948c22a50d`.
Тег `lab-02` вказує на об'єкт `fa407a14d6a8b98f789d11661ba8cf19f5988b11`.
Перед змінами MSVC Debug з ASan зібрано; CTest **6/6 PASS**.
Початковий журнал `evidence/lab-01-02-before.log` збережений локально;
службові журнали не входять до Git.

Після реалізації: MSVC **19.51.36260.0**, x64 Debug, ASan ON, збірка успішна
без попереджень; CTest **15/15 PASS**. Окремі виконувані тести:
**2 645 440** перевірок Lab 01–02 та **541 001** перевірка Lab 03.
Старий тест тепер перевіряє колишню поведінку для решти опкодів; 15 нових
опкодів передано окремому тесту, тому його старий лічильник зменшився на 600.

Перевірено:

- усі 65536 значень get16/set16 на першій та останній допустимій парі;
- останній байт, адреси 4096, 65535 та SIZE_MAX, атомарність помилкового запису;
- усі нові опкоди, вісім початкових наборів прапорців, різні значення операндів;
- PC для довжин 1/2/3, успіх на кінці RAM, усі варіанти обрізаних операндів;
- H=0, 4095, 4096, 65535, 16-бітне загортання, захист непрямого доступу;
- HALT, повторний step/run, nullptr пам'яті, ліміт кроків і помилки даних;
- точні байти OUT/OUTN, AB, абсолютні LOAD/STORE, запис у код;
- старі сценарії Lab 01 і Lab 02, новий офіційний сценарій, траса й дампи;
- досліди C++ із вказівниками та виправлений вихід за межі під ASan.

Локальні журнали перевірок у `build/` та `evidence/` ігноруються Git.
Журнал повторного CTest доступний у `build/Testing/Temporary/LastTest.log`.
До репозиторію включено тільки постійні результати навчальних дослідів:
`evidence/asan-report.txt`, `evidence/asan-report.txt.exit.txt`,
`evidence/asan-fixed.txt` та `evidence/pointer-experiments.txt`.
Початковий MSBuild у пісочниці отримав E_ACCESSDENIED від FileTracker;
успішні збірки виконано з дозволеним розширеним доступом. Це обмеження
середовища, зміни коду для його обходу не застосовувалися.

## Відповідність Standard M1–M4

| Вимога | Підтвердження |
|---|---|
| M1: get16/set16, little-endian, межі | `src/memory.*`, `lab03_memory_cpu`, `lab-03-word` |
| M2: 20–2C, OUT/OUTN, H, step, run, regs | `src/cpu.*`, `src/main.cpp`, тести опкодів і PC |
| M3: код 000, дані 800, ABC і абсолютний доступ | `checks/lab-03-absolute.txt`, збережений дамп |
| M3: рівно сім інструкцій через H, AB та траса | `checks/lab-03-h.txt`, `checks/lab-03-trace.txt` |
| M4: вказівники C++, адреса хоста/гостя | досліди, цей README, журнал виводу |
| M4: справжній ASan та виправлення | ізольований `asan_oob`, повний звіт, `bounds_fixed` |
| Збереження Lab 01–02 | 6/6 до змін, їхні сценарії проходять після |
| Тег `lab-03` з умови | Анотований тег фінального коміту Lab 03; перевірка нижче |

Усі програмні вимоги Standard виконано. Здача оформлюється комітом
`Lab 03: Pointers and Memory` та анотованим тегом `lab-03`, який створюється
після успішного коміту. Попередні коміти Lab 01–02 та тег `lab-02`
зберігаються без змін. Публікація через push у цій підготовці не виконується.

Перевірка після оформлення (обидва хеші мають збігатися, тип тега — `tag`):

```powershell
git log -1 --oneline
git rev-parse HEAD
git rev-parse 'lab-03^{commit}'
git cat-file -t lab-03
git status
```

## П'ятихвилинний захист

1. **0:00–1:00.** Намалювати `p → x`, пояснити `&x`, `p`, `*p`, `&p` і nullptr.
   Показати: зміна `*p` змінює x; копія аргументу не змінює оригінал.
2. **1:00–2:00.** Пояснити розмір вказівника 8 і int 4 на цьому хості,
   крок int*/Byte*, void* та правильне повернення типу. Відділити Memory* від H/PC.
3. **2:00–3:00.** Показати `34 12` для 0x1234 та LOAD/STORE за абсолютними
   адресами. Пояснити довжини 1/2/3 і незмінні прапорці завантажень.
4. **3:00–4:00.** Запустити H-трасу: H=0x800 → 0x801, PC=0 → 3 → 4 → 5 → 6 → 7 → 8 → 9.
   Показати AB і дані ABC; пояснити аналогію `*p` і `++p`.
5. **4:00–5:00.** Відкрити справжній ASan: запис 1 байта, рядок 12, індекс 4096.
   Показати перевірку меж і безпечну відмову; завершити результатом CTest 15/15.
