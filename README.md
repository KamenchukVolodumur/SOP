# Operacje niskopoziomowe na plikach (I/O w POSIX)

> [!NOTE]
> **Przedmiot:** Systemy Operacyjne 1 (SOP1) • Politechnika Warszawska (Wydział MiNI)
> **Zakres:** Laboratorium 2 • Niskopoziomowe I/O, `mmap`, `FIFO`, `poll`

Kompletny przewodnik: deskryptory plików, wywołania systemowe open, read, write, lseek, rodzina -at oraz typowe scenariusze laboratoryjne w języku C.

## 📑 Spis Treści

- [01. Operacje Niskopoziomowe vs Wysokopoziomowe](#01-operacje-niskopoziomowe-vs-wysokopoziomowe)
- [02. Funkcja open() i Przewodnik po Flagach](#02-funkcja-open-i-przewodnik-po-flagach)
- [03. Praktyczne Scenariusze Użycia open() w C](#03-praktyczne-scenariusze-użycia-open-w-c)
- [04. Odczyt i Zapis: read(), write() oraz Brak Buforowania](#04-odczyt-i-zapis-read-write-oraz-brak-buforowania)
- [05. Swobodne Poruszanie Kursorem: lseek()](#05-swobodne-poruszanie-kursorem-lseek)
- [06. Rodzina Funkcji z Końcówką -at (*at)](#06-rodzina-funkcji-z-końcówką--at-at)
- [07. Ściągawka: Kody Błędów (errno)](#07-ściągawka-kody-błędów-errno)
- [08. Wektorowe Wejście/Wyjście: readv() i writev() (Scatter / Gather I/O)](#08-wektorowe-wejściewyjście-readv-i-writev-scatter-gather-io)
- [09. Mapowanie Plików do Pamięci: mmap()](#09-mapowanie-plików-do-pamięci-mmap)
- [10. Tryb Blokujący vs Nieblokujący (O_NONBLOCK i fcntl)](#10-tryb-blokujący-vs-nieblokujący-o_nonblock-i-fcntl)
- [11. Multipleksowanie I/O: Odwieczny Problem i Rozwiązanie](#11-multipleksowanie-io-odwieczny-problem-i-rozwiązanie)
- [12. Praktyka Laboratoryjna: poll() i Przechwytywanie Zdarzeń (/dev/input)](#12-praktyka-laboratoryjna-poll-i-przechwytywanie-zdarzeń-devinput)
- [13. Łącza Nazwane (FIFO / Named Pipes): Idealny Poligon dla poll()](#13-łącza-nazwane-fifo-named-pipes-idealny-poligon-dla-poll)
- [14. Wzorzec: Jak Wypełniać pollfd i Odczytywać z Właściwego Źródła](#14-wzorzec-jak-wypełniać-pollfd-i-odczytywać-z-właściwego-źródła)

   

---

## 01. Operacje Niskopoziomowe vs Wysokopoziomowe

 

W języku C do obsługi plików mamy dwie warstwy operacji: standardową bibliotekę C (wysoki poziom) oraz wywołania systemowe jądra POSIX (niski poziom).

 

| Cecha | Wysokopoziomowe (Standard C) | Niskopoziomowe (Wywołania POSIX / System Calls) |
| --- | --- | --- |
| **Identyfikator pliku** | Wskaźnik `FILE *` (struktura ze stanem i buforem) | **Deskryptor pliku `int fd`** (liczba całkowita) |
| **Nagłówki** | `#include <stdio.h>` | `#include <fcntl.h>` oraz `#include <unistd.h>` |
| **Funkcje** | `fopen()`, `fread()`, `fwrite()`, `fclose()` | `open()`, `read()`, `write()`, `close()`, `lseek()` |
| **Buforowanie** | **Tak** (w przestrzeni użytkownika, redukuje liczbę syscalli) | **Brak buforowania!** Bezpośrednie zapytania do jądra OS |
| **Zastosowanie** | Zwykłe pliki tekstowe i binarne na dysku | **Wszystko:** pliki regularne, potoki (FIFO/pipe), sockety, urządzenia znakowe i blokowe (`/dev/*`) |

 

> [!NOTE]
> **💡 Co to jest Deskryptor Pliku (fd)?**
> Deskryptor to nieujemna liczba całkowita (`0, 1, 2, 3...`) będąca indeksem w tablicy otwartych plików danego procesu w jądrze. Domyślnie każdy proces startuje z trzema otwartymi deskryptorami:
> - `0` (`STDIN_FILENO`) — standardowe wejście
> - `1` (`STDOUT_FILENO`) — standardowe wyjście
> - `2` (`STDERR_FILENO`) — standardowe wyjście błędów

    

---

## 02. Funkcja open() i Przewodnik po Flagach

 

```c
int open(const char *path, int oflag, ... /* mode_t mode */);
int close(int fildes);
```

 

Parametr `oflag` konstruujemy za pomocą bitowego **OR** (`|`). 
 **Zasada żelazna:** Musimy wybrać dokładnie jeden tryb dostępu, a następnie możemy dodać dowolną liczbę flag modyfikujących!

 

### A. Tryby dostępu (Wybierz dokładnie jeden!)

 

| Flaga | Opis | Uwagi |
| --- | --- | --- |
| `O_RDONLY` | Tylko do odczytu (Read Only) | Próba wywołania `write()` zwróci błąd `[EBADF]`. |
| `O_WRONLY` | Tylko do zapisu (Write Only) | Próba wywołania `read()` zwróci błąd `[EBADF]`. |
| `O_RDWR` | Odczyt i zapis (Read & Write) | Umożliwia równoczesne czytanie i pisanie. *Nie stosować do FIFO!* |

 

### B. Flagi modyfikujące zachowanie (Łączone przez `|`)

  

#### 🔹 Tworzenie pliku `O_CREAT`

Jeśli plik nie istnieje, system go utworzy. **Wymaga podania 3. argumentu `mode`** (uprawnienia, np. `0644`), które zostaną zmodyfikowane przez `umask` procesu.
 

#### 🔹 Atomowe sprawdzenie `O_EXCL`

Używane **zawsze w parze z `O_CREAT`**. Jeśli plik już istnieje, `open()` zakończy się błędem `[EEXIST]`. Zapobiega wyścigom (race condition).
 

#### 🔹 Czyszczenie zawartości `O_TRUNC`

Jeśli plik istnieje i otwarto go do zapisu (`O_WRONLY` lub `O_RDWR`), jego długość jest natychmiast obcinana do **0 bajtów** (stara zawartość jest niszczona).
 

#### 🔹 Dopisywanie na koniec `O_APPEND`

Przed każdym wywołaniem `write()` wskaźnik pozycji w pliku jest automatycznie przestawiany na koniec. Idealne do logów.
 

#### 🔹 Tryb nieblokujący `O_NONBLOCK`

Wywołania `open`, `read`, `write` nie będą czekać. Zwracają natychmiast tyle bajtów, ile jest gotowych, lub błąd `[EAGAIN]`/`[EWOULDBLOCK]`.
 

#### 🔹 Bezpieczeństwo fork/exec `O_CLOEXEC`

Automatycznie zamyka ten deskryptor w procesie potomnym po wywołaniu funkcji z rodziny `exec()`.
     

---

## 03. Praktyczne Scenariusze Użycia open() w C

 

Oto gotowe szablony wywołań dla najczęstszych sytuacji laboratoryjnych i projektowych:

  

### 📖 1. Tylko odczyt istniejącego pliku (`O_RDONLY`)

Otwieramy plik do czytania. Jeśli plik nie istnieje — funkcja zwróci `-1` i ustawi `errno = ENOENT`.

```c
int fd = open("dane.txt", O_RDONLY);
if (fd == -1) {
    perror("Błąd otwarcia do odczytu");
    return 1;
}
// ... czytanie przez read(fd, ...) ...
close(fd);
```

  

### ✏️ 2. Zapis z nadpisaniem (Zastąpienie pliku / Wyczyszczenie) (`O_WRONLY | O_CREAT | O_TRUNC`)

Odpowiednik trybu `"w"` z `fopen` lub przekierowania `>` w bashu. Jeśli plik istnieje — czyści go do 0 bajtów. Jeśli nie istnieje — tworzy nowy.

```c
int fd = open("wynik.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd == -1) {
    perror("Nie można otworzyć/utworzyć pliku do zapisu");
    return 1;
}
// ... pisanie przez write(fd, ...) ...
close(fd);
```

 

> [!CAUTION]
> **🚨 Klasyczna pułapka laboratoryjna: brak O_TRUNC!**
> Co się stanie, jeśli otworzysz plik jako `open(path, O_WRONLY | O_CREAT, 0644)` **bez flagi O_TRUNC** dla istniejącego pliku?
> - Kursor ustawia się na **bajt 0 (początek pliku)**.
> - Stara zawartość pliku **NIE JEST KASOWANA**!
> - Funkcja `write()` zaczyna nadpisywać bajty od początku. Jeśli nowy tekst ma 20 znaków, a stary miał 100, pierwsze 20 znaków zostanie podmienionych, a pozostałe 80 zostanie nietknięte!
> - Jeśli kopiujesz plik sam w siebie lub o tej samej zawartości — na dysku nie zmieni się zupełnie nic!

  

### ➕ 3. Dopisywanie na koniec (Tryb Append / Logowanie) (`O_WRONLY | O_CREAT | O_APPEND`)

Odpowiednik trybu `"a"` z `fopen`. Nie niszczy dotychczasowych danych. Każdy kolejny `write()` ląduje na samym końcu pliku.

```c
int fd = open("app.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
if (fd == -1) {
    perror("Błąd otwarcia pliku logów");
    return 1;
}
// Każdy write dopisze treść na końcu pliku
close(fd);
```

  

### 🛡️ 4. Atomowe tworzenie NOWEGO pliku (Gwarancja unikalności) (`O_WRONLY | O_CREAT | O_EXCL`)

Chcemy stworzyć plik **tylko jeśli go jeszcze NIE MA**. Jeśli plik już istnieje, wywołanie natychmiast zawiedzie z `errno = EEXIST`. Niezastąpione przy plikach blokad (lockfiles).

```c
int fd = open("program.lock", O_WRONLY | O_CREAT | O_EXCL, 0600);
if (fd == -1) {
    if (errno == EEXIST) {
        fprintf(stderr, "Błąd: Plik już istnieje! Inna instancja programu działa.\n");
    } else {
        perror("Błąd tworzenia lockfile");
    }
    return 1;
}
close(fd);
```

  

### 🔄 5. Modyfikacja w miejscu (Odczyt i Zapis bez obcinania) (`O_RDWR`)

Otwieramy plik, aby czytać dane i w wybrane miejsca wpisywać poprawki (często łączone z `lseek`).

```c
int fd = open("baza.bin", O_RDWR);
if (fd == -1) {
    perror("Nie można otworzyć bazy w trybie RDWR");
    return 1;
}
// Możemy zarówno czytać (read), jak i pisać (write) na tym samym fd
close(fd);
```

    

---

## 04. Odczyt i Zapis: read(), write() oraz Brak Buforowania

 

```c
ssize_t read(int fildes, void *buf, size_t nbyte);
ssize_t write(int fildes, const void *buf, size_t nbyte);
```

 

> [!WARNING]
> **⚠️ Zjawisko Częściowego Odczytu/Zapisu (Partial Read/Write)**
> Ponieważ operacje niskopoziomowe **nie mają bufora**, `read` i `write` zwracają natychmiast tyle bajtów, ile aktualnie jest dostępnych w systemie:
> - Wartość zwrócona `> 0`: liczba bajtów, które *faktycznie* udało się przetworzyć (może być mniejsza niż `nbyte`!).
> - Wartość zwrócona `== 0` w `read()`: **Koniec pliku (EOF)**.
> - Wartość zwrócona `== -1`: Wystąpił błąd (kod w `errno`).

 

### Wzorzec 1: Bezpieczna pętla odczytu pliku do końca (Kopiowanie na STDOUT)

 

```c
#define BUF_SIZE 1024
char buffer[BUF_SIZE];
ssize_t bytes_read;

while ((bytes_read = read(fd, buffer, BUF_SIZE)) > 0) {
    // Uwaga: write również może zapisać mniej bajtów niż mu przekazano!
    ssize_t bytes_written = 0;
    while (bytes_written < bytes_read) {
        ssize_t res = write(STDOUT_FILENO, buffer + bytes_written, bytes_read - bytes_written);
        if (res == -1) {
            if (errno == EINTR) continue; // Przerwane przez sygnał -> ponów próbę
            perror("Błąd zapisu");
            return 1;
        }
        bytes_written += res;
    }
}

if (bytes_read == -1) {
    perror("Błąd odczytu");
}
```

    

---

## 05. Swobodne Poruszanie Kursorem: lseek()

 

```c
off_t lseek(int fildes, off_t offset, int whence);
```

 

Domyślnie odczyt i zapis przesuwają się sekwencyjnie. Funkcja `lseek()` pozwala przestawić kursor pozycji (w plikach regularnych) w dowolne miejsce bez konieczności czytania wszystkiego po kolei.

 

| Wartość `whence` | Punkt odniesienia | Przykład użycia |
| --- | --- | --- |
| `SEEK_SET` | Początek pliku (bajt 0) | `lseek(fd, 100, SEEK_SET);` — skok dokładnie na 100. bajt pliku. |
| `SEEK_CUR` | Aktualna pozycja kursora | `lseek(fd, 50, SEEK_CUR);` — przesuń kursor o 50 bajtów w przód od obecnego miejsca. |
| `SEEK_END` | Koniec pliku | `lseek(fd, -10, SEEK_END);` — cofnij kursor o 10 bajtów przed koniec pliku. |

 

### Laboratoyjne triki z lseek:

  

#### 🔹 Pobranie bieżącej pozycji `SEEK_CUR`

Ponieważ `lseek` zwraca nową pozycję, przesunięcie o 0 od pozycji bieżącej zwraca aktualny offset:

 

```c
off_t current_pos = lseek(fd, 0, SEEK_CUR);
```
 

#### 🔹 Sprawdzenie rozmiaru pliku `SEEK_END`

Skok na koniec pliku zwraca jego łączną wielkość w bajtach:

 

```c
off_t file_size = lseek(fd, 0, SEEK_END);
lseek(fd, 0, SEEK_SET); // Powrót na początek!
```
     

---

## 06. Rodzina Funkcji z Końcówką -at (*at)

 

```c
int openat(int dirfd, const char *path, int oflag, ...);
int fstatat(int dirfd, const char *path, struct stat *buf, int flag);
int unlinkat(int dirfd, const char *path, int flag);
```

 

Funkcje z końcówką `-at` rozwiązują 3 kluczowe problemy programisty systemowego:

 

1. **Brak konieczności sklejania długich ścieżek:** Otwieramy katalog raz (mamy jego `dirfd`), a potem otwieramy pliki wewnątrz podając jedynie ich krótkie nazwy.
2. **Wielowątkowość (Thread Safety):** Funkcja `chdir()` zmienia bieżący katalog dla całego procesu i psuje ścieżki w innych wątkach. Przy `*at` każdy wątek może mieć swój własny `dirfd` bez dotykania globalnego CWD!
3. **Ochrona przed wyścigami (TOCTOU):** Zabezpiecza przed podmianą katalogu (np. symlinkiem) w trakcie działania programu.

 

> [!TIP]
> **Magiczne Makro AT_FDCWD**
> Jeśli jako pierwszy parametr `dirfd` podasz `AT_FDCWD`, funkcja zinterpretuje ścieżkę względną od bieżącego katalogu roboczego (CWD).
> Oznacza to, że: `openat(AT_FDCWD, "plik.txt", O_RDONLY)` zachowuje się **dokładnie tak samo** jak `open("plik.txt", O_RDONLY)`.

    

---

## 07. Ściągawka: Kody Błędów (errno)

 

Wszystkie powyższe funkcje w razie awarii zwracają `-1` i ustawiają zmienną `errno` (z nagłówka `<errno.h>`):

 

| Kod błędu | Znaczenie | Typowa przyczyna na labach |
| --- | --- | --- |
| `ENOENT` | No such file or directory | Plik nie istnieje, a zapomniano podać `O_CREAT`, lub błędna ścieżka. |
| `EACCES` | Permission denied | Brak uprawnień do odczytu/zapisu lub brak prawa `+x` na katalogu nadrzędnym. |
| `EEXIST` | File exists | Podano `O_CREAT \| O_EXCL`, a plik już jest na dysku. |
| `EISDIR` | Is a directory | Próba otwarcia katalogu w trybie zapisu (`O_WRONLY` lub `O_RDWR`). |
| `EMFILE` | Too many open files (Process limit) | Proces wyczerpał limit otwartych deskryptorów (zapomniano wywoływać `close()` w pętli!). |
| `ENFILE` | Too many open files in system | Globalny limit całego systemu operacyjnego został osiągnięty. |
| `EINTR` | Interrupted function call | Wywołanie `read`/`write`/`open` zostało przerwane nadejściem sygnału. Zazwyczaj należy ponowić próbę. |

    

---

## 08. Wektorowe Wejście/Wyjście: readv() i writev() (Scatter / Gather I/O)

 

Wymagany nagłówek: `#include <sys/uio.h>`. Służy do odczytu lub zapisu danych z/do **wielu różnych buforów o różnych rozmiarach za jednym wywołaniem systemowym**.

 

```c
struct iovec {
    void   *iov_base; /* Wskaźnik na początek bufora w pamięci RAM */
    size_t  iov_len;  /* Dokładny rozmiar tego konkretnego bufora w bajtach */
};

ssize_t readv(int fildes, const struct iovec *iov, int iovcnt);
ssize_t writev(int fildes, const struct iovec *iov, int iovcnt);
```

 

> [!TIP]
> **💡 Czy każdy bufor ma swój własny rozmiar?**
> **TAK!** Każdy element w tablicy `iov` opisuje zupełnie niezależny bufor:
> - `iov[0]` może mieć 16 bajtów (np. nagłówek pakietu).
> - `iov[1]` może mieć 1024 bajty (np. treść wiadomości).
> - `iov[2]` może mieć 4 bajty (np. suma kontrolna / CRC).

 

### Jak to działa w praktyce?

 

- **writev (Gather write — "Zbieranie"):** Jądro zbiera dane z buforów `iov[0]`, potem `iov[1]`, aż do `iov[iovcnt-1]` i zapisuje je jeden po drugim do pliku w **jednej atomowej operacji**. Nie musisz alokować wielkiego bufora i robić `memcpy`!
- **readv (Scatter read — "Rozpraszanie"):** Jądro czyta dane z pliku i po kolei napełnia bufory: najpierw do pełna napełnia `iov[0]` (aż do `iov[0].iov_len`), potem nadmiar przelewa do `iov[1]` itd.

 

### Przykład w języku C (Zapis nagłówka i treści wiadomości):

 

```c
#include <sys/uio.h>
#include <fcntl.h>
#include <string.h>

char header[] = "HEADER:";          // Bufor 1 (rozmiar 7 bajtów)
char payload[] = "Tresc wiadomosci\n"; // Bufor 2 (rozmiar 17 bajtów)

struct iovec iov[2];

// Konfiguracja pierwszego pojemnika:
iov[0].iov_base = header;
iov[0].iov_len = strlen(header);

// Konfiguracja drugiego pojemnika o INNYM rozmiarze:
iov[1].iov_base = payload;
iov[1].iov_len = strlen(payload);

// Zapis obu buforów naraz za JEDNYM wywołaniem systemowym:
ssize_t written = writev(fd, iov, 2);
if (written == -1) {
    ERR("writev");
}
```

    

---

## 09. Mapowanie Plików do Pamięci: mmap()

 

Nagłówki: `#include <sys/mman.h>` oraz `#include <sys/stat.h>`.

**Koncepcja:** Zamiast bawić się w pętle z `read()`, `write()` i `lseek()`, prosimy jądro o **zrzutowanie pliku wprost do wirtualnej pamięci RAM procesu**. Otrzymujemy zwykły wskaźnik `char *buff` i pracujemy z plikiem dokładnie tak, jak ze zwykłą tablicą w C!

 

```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

 

### Rozłożenie parametrów na czynniki pierwsze:

 

| Parametr | Typowa wartość w SOP | Co oznacza? |
| --- | --- | --- |
| `addr` | `NULL` | Prosimy jądro, aby samo wybrało dogodny adres w pamięci RAM. |
| `length` | `sb.st_size` | Ile bajtów pliku mapujemy (zazwyczaj pobierane przez `fstat(fd, &sb)`). |
| `prot` | `PROT_READ \| PROT_WRITE` | Uprawnienia do pamięci: czytanie (`PROT_READ`), modyfikacja (`PROT_WRITE`), wykonywanie kodu (`PROT_EXEC`). |
| `flags` | `MAP_SHARED` | `MAP_SHARED`: zmiany w pamięci są **zapisywane fizycznie do pliku** na dysku. `MAP_PRIVATE`: tworzy kopię (Copy-on-Write) — zmiany są widoczne tylko dla nas i NIE wpływają na plik! |
| `fd` | `fd` | Deskryptor otwartego wcześniej pliku (np. przez `open("plik", O_RDWR)`). |
| `offset` | `0` | Od którego bajtu pliku zaczynamy mapowanie (musi być wielokrotnością rozmiaru strony pamięci, zazwyczaj 0). |

 

> [!WARNING]
> **⚠️ Pułapka laboratoryjna: Jak sprawdzać błąd mmap?**
> Większość funkcji zwracających wskaźnik przy błędzie zwraca `NULL`. **ALE NIE mmap!**
> W razie błędu `mmap` zwraca specjalną stałą: **`MAP_FAILED`** (czyli `(void *)-1`).
> ```c
> char *buff = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
> if (buff == MAP_FAILED) {  // ZAWSZE sprawdzaj MAP_FAILED, NIGDY NULL!
> ERR("mmap");
> }
> ```

 

### Trzy niezbędne kroki towarzyszące:

  

#### 🔹 1. Rozmiar: fstat() `st_size`

Aby wiedzieć, ile pamięci zmapować, pytamy system o rozmiar pliku przez `fstat(fd, &sb)`, po czym bierzemy `sb.st_size`.
 

#### 🔹 2. Zapis na dysk: msync() `MS_SYNC`

Zapis do tablicy `buff[i] = 'Z'` modyfikuje tylko pamięć RAM. Aby wymusić fizyczny zrzut zmian na dysk natychmiast, wołamy: `msync(buff, length, MS_SYNC);`.
 

#### 🔹 3. Sprzątanie: munmap() `munmap`

Po zakończeniu pracy zwalniamy zmapowany obszar: `munmap(buff, length);` i zamykamy deskryptor `close(fd);`.
     

---

## 10. Tryb Blokujący vs Nieblokujący (O_NONBLOCK i fcntl)

 

W systemach POSIX panuje zasada **„Wszystko jest plikiem”** — nie tylko pliki na dysku, ale też terminal (klawiatura/ekran), potoki (pipe/FIFO) oraz połączenia sieciowe (sockety).

  

#### 🔹 Tryb Blokujący (Domyślny) `Domyślny`

Gdy wołasz `read()` lub `getchar()` z klawiatury, a użytkownik nic nie wpisał — **program zasypia i wisi w nieskończoność**, czekając na klawisz Enter. Nie wykonuje wtedy żadnych innych instrukcji.
 

#### 🔹 Tryb Nieblokujący (O_NONBLOCK) `O_NONBLOCK`

Gdy wołasz `read()`, a danych akurat nie ma — **funkcja NIE czeka ani milisekundy!** Natychmiast zwraca `-1` i ustawia `errno = EAGAIN` (lub `EWOULDBLOCK`). Program działa dalej bez zatrzymywania.
  

### Jak włączyć O_NONBLOCK dla już otwartego deskryptora (np. STDIN)?

 

Deskryptor standardowego wejścia (`0` / `STDIN_FILENO`) otwiera system przed startem `main()`. Nie możemy go otworzyć przez `open()`, dlatego flagę zmieniamy funkcją `fcntl()`:

 

```c
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

// 1. Pobierz aktualne flagi deskryptora STDIN:
int flags = fcntl(STDIN_FILENO, F_GETFL);
if (flags == -1) ERR("fcntl F_GETFL");

// 2. Dodaj flagę O_NONBLOCK do istniejących flag:
if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == -1)
    ERR("fcntl F_SETFL");

// 3. Teraz próba czytania nie zatrzyma programu:
char c;
ssize_t res = read(STDIN_FILENO, &c, 1);
if (res == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        printf("Użytkownik nic nie nacisnął, program robi swoje dalej!\n");
    } else {
        ERR("read");
    }
} else if (res > 0) {
    printf("Wczytano znak: %c\n", c);
}
```

    

---

## 11. Multipleksowanie I/O: Odwieczny Problem i Rozwiązanie

 

Wyobraź sobie, że Twój program musi równocześnie nasłuchiwać **dwóch lub więcej źródeł danych**: np. klawiatury (`stdin`) oraz połączenia sieciowego (`socket`) lub potoku (`pipe`).

 

### Dwa złe podejścia (Pułapka):

 

| Podejście | Jak działa? | Dlaczego odpada na laboratoriach? |
| --- | --- | --- |
| **1. Tryb blokujący** | Wołasz `read(stdin)`, a potem `read(socket)`. | ❌ **Paraliż!** Program wisi na `stdin`. Jeśli z sieci przyjdą dane, program ich NIE zobaczy, dopóki użytkownik nie naciśnie Enter! |
| **2. Tryb nieblokujący + Busy Waiting** | Pętla `while(1)` bez przerwy sprawdza: „są dane na stdin? a na socket?”. | ❌ **100% obciążenia procesora!** Wentylatory wyją, CPU się pali. Na SOP za tzw. *busy waiting* (aktywne czekanie) stawiane jest 0 punktów! Dodanie `sleep()` z kolei powoduje lagi. |

 

> [!TIP]
> **💡 Rozwiązanie: Multipleksowanie I/O (IO Multiplexing)**
> Zamiast samemu w kółko pytać, **przekazujemy listę naszych deskryptorów jądru systemu operacyjnego** i mówimy:
> „Oto 5 moich deskryptorów. Ja idę spać (0% zużycia CPU). Obudź mnie dokładnie w tej mikrosekundzie, w której na KTÓRYMKOLWIEK z nich pojawią się dane do czytania!”

 

### Narzędzia Multipleksowania w POSIX i Linux:

  

#### 🔹 select() `POSIX (Stary)`

Pierwszy historyczny mechanizm. Ma sztywny limit do 1024 deskryptorów i niewygodne makra bitowe (`FD_SET`). Dziś rzadko zalecany.
 

#### 🔹 poll() `POSIX • GWIAZDA LABÓW!`

**Główny mechanizm na SOP1!** Brak sztucznych limitów, prosta i elegancka tablica struktur `struct pollfd`. Działa na każdym uniksowym systemie.
 

#### 🔹 epoll() `Tylko Linux`

Ultra-wydajne rozszerzenie jądra Linux dla serwerów obsługujących dziesiątki tysięcy połączeń (C10K problem). Złożone API, niewymagane na labach.
     

---

## 12. Praktyka Laboratoryjna: poll() i Przechwytywanie Zdarzeń (/dev/input)

 

Poniższy kod to podręcznikowy przykład użycia `poll()` do jednoczesnego nasłuchiwania **wszystkich urządzeń wejściowych w systemie Linux** (klawiatury, myszki, touchpada, przycisków zasilania).

 

### Anatomia struktury struct pollfd

 

```c
struct pollfd {
    int   fd;       /* Deskryptor pliku/urządzenia */
    short events;   /* O jakie zdarzenia pytamy jądro (np. POLLIN - gotowość do czytania) */
    short revents;  /* Zdarzenia ZWRÓCONE przez jądro po obudzeniu */
};
```

 

### Kluczowe mechanizmy w tym programie:

  

#### 🔹 1. Skanowanie /dev/input `opendir / readdir`

Funkcja `get_descriptors()` otwiera katalog `/dev/input` i szuka wszystkich plików o nazwach `event0`, `event1`... Każdy z nich reprezentuje fizyczny kontroler.
 

#### 🔹 2. Konfiguracja pollfd `POLLIN`

Dla każdego otwartego urządzenia ustawiamy: `fds[n].events = POLLIN;` (obudź, gdy użytkownik coś wciśnie/poruszy) oraz `revents = 0;`.
 

#### 🔹 3. Odłączenie urządzenia `fds[i].fd = -1`

Jeśli urządzenie zostanie odłączone (wyjęte USB), jądro zwraca flagę `POLLHUP`. **Trik POSIX:** ustawienie `fds[i].fd = -1` sprawia, że `poll()` automatycznie ignoruje ten wpis w przyszłości!
 

#### 🔹 4. Odczyt struct input_event `O_NONBLOCK + read`

Urządzenia są otwarte z `O_NONBLOCK`. Po obudzeniu przez `POLLIN` pętla `while(read(...) == sizeof(ev))` wysysa wszystkie zdarzenia z bufora do zera bez ryzyka zawieszenia.
     

---

## 13. Łącza Nazwane (FIFO / Named Pipes): Idealny Poligon dla poll()

 

Łącze nazwane (FIFO) to **wirtualna rura w pamięci RAM**, która w systemie plików udaje zwykły plik. Służy do komunikacji między procesami (IPC).

  

#### 🔹 Dlaczego FIFO? `First-In First-Out`

Dane wysłane jako pierwsze są odczytywane jako pierwsze. **Kluczowa cecha:** odczytanie danych z FIFO natychmiast je bezpowrotnie **kasuje** (dane nie zostają na dysku).
 

#### 🔹 Brak uprawnień roota! `Zwykły użytkownik`

W przeciwieństwie do `/dev/input` (które wymaga `sudo`), plik FIFO możesz utworzyć w swoim katalogu domowym i testować `poll()` bez żadnych ograniczeń.
  

### Testowanie w terminalu (Krok po kroku):

 

```c
# Terminal 1: Tworzymy FIFO i czekamy na dane
mkfifo moj_fifo
cat moj_fifo       # Program zawisa i czeka, aż ktoś coś napisze!

# Terminal 2: Wrzucamy wiadomość do rury
echo "Czesc z drugiego terminala!" > moj_fifo

# W Terminalu 1 tekst natychmiast się pojawi, a cat zakończy działanie.
rm moj_fifo        # Sprzątanie
```

 

### Użycie w języku C:

 

```c
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// 1. Tworzenie łącza (analogicznie do mkdir):
if (mkfifo("moj_pipe", 0666) == -1 && errno != EEXIST)
    ERR("mkfifo");

// 2. Otwieranie (zwykły open!):
int fd = open("moj_pipe", O_RDONLY | O_NONBLOCK);

// 3. Możemy teraz wrzucić fd do poll() i czekać na POLLIN!
// 4. Na koniec usuwamy plik ze ścieżki:
close(fd);
unlink("moj_pipe");
```

    

---

## 14. Wzorzec: Jak Wypełniać pollfd i Odczytywać z Właściwego Źródła

 

Oto uniwersalny, kompletny szablon laboratoryjny, który pokazuje krok po kroku: jak poprawnie skonfigurować tablicę `struct pollfd`, jak wywołać `poll()` oraz jak rozpoznać, **z którego dokładnie deskryptora (klawiatury czy rury) nadeszły dane**.

 

### Sygnatura i parametry funkcji poll()

 

> [!WARNING]
> **⚠️ Wymagany nagłówek i makro POSIX**
> Aby użyć `poll()` oraz struktury `struct pollfd`, na samym początku pliku **musisz** dołączyć:
> ```c
> #define _POSIX_C_SOURCE 200809L
> #include <poll.h>
> ```
> Brak tego nagłówka skutkuje błędem kompilacji: *„error: array type has incomplete element type ‘struct pollfd’”* (kompilator nie zna rozmiaru ani pól struktury).

 

```c
int poll(struct pollfd fds[], nfds_t nfds, int timeout);
```

 

| Argument | Typ | Znaczenie i wartości |
| --- | --- | --- |
| `fds[]` | `struct pollfd` | Tablica struktur opisująca deskryptory do monitorowania oraz oczekiwane zdarzenia. |
| `nfds` | `nfds_t` (unsigned) | Liczba elementów w tablicy `fds` (np. `2`). |
| `timeout` | `int` | Czas oczekiwania w **milisekundach (ms)**: • `-1`: **Czekaj w nieskończoność** (blokuj aż do wystąpienia zdarzenia). • `0`: **Sprawdź natychmiast** i wróć (nie blokuj wcale). • `> 0`: Czekaj maksymalnie tyle ms (np. `5000` = 5 sekund). |

 

> [!TIP]
> **Co zwraca funkcja poll()?**
> - `> 0`: **Sukces** — liczba deskryptorów, na których faktycznie zaszło zdarzenie (są gotowe do I/O).
> - `0`: **Upłynął timeout** — minął wyznaczony czas i nic się nie wydarzyło.
> - `-1`: **Błąd** — wywołanie nie powiodło się (np. przerwane przez sygnał `EINTR`), szczegóły w `errno`.

 

### 1. Trzy pola struktury struct pollfd

 

```c
struct pollfd fds[2];

// ŹRÓDŁO 0: Klawiatura (standardowe wejście)
fds[0].fd = STDIN_FILENO;  // 1. Który deskryptor? (0)
fds[0].events = POLLIN;     // 2. Na co czekamy? (Dane do czytania)
fds[0].revents = 0;         // 3. Wynik zerujemy przed wywołaniem

// ŹRÓDŁO 1: Rura FIFO (otwarta z O_NONBLOCK)
fds[1].fd = fifo_fd;       // 1. Deskryptor z open("moj_pipe", ...)
fds[1].events = POLLIN;     // 2. Czekamy na dane z drugiego procesu
fds[1].revents = 0;         // 3. Wynik zerujemy
```

 

### 2. Jak sprawdzić, SKĄD przyszły dane po obudzeniu poll()?

 

Po powrocie z `poll()` sprawdzamy pole **`revents`** każdego indeksu za pomocą bitowego **AND (`&`)**:

  

#### 🔹 fds[0]: Klawiatura `fds[0].revents & POLLIN`

Jeśli ten warunek jest prawdziwy, użytkownik nacisnął klawisz i Enter w terminalu. Czytamy z `STDIN_FILENO`.
 

#### 🔹 fds[1]: Rura FIFO `fds[1].revents & POLLIN`

Jeśli ten warunek jest prawdziwy, ktoś w innym terminalu wpisał coś do rury. Czytamy z `fifo_fd`.
  

### 3. Pełny, gotowy szablon pętli w języku C:

 

```c
#define BUF_SIZE 256
char buf[BUF_SIZE];

while (1) {
    // Czekamy na dane z klawiatury LUB rury (timeout 5000 ms = 5 sekund):
    int ret = poll(fds, 2, 5000);

    if (ret == -1) {
        ERR("poll");
    }

    if (ret == 0) {
        printf("[IDLE] Minelo 5 sekund bez zadnych danych...\n");
        continue;
    }

    // --- SPRAWDZAMY ŹRÓDŁO 0: KLAWIATURA (stdin) ---
    if (fds[0].revents & POLLIN) {
        ssize_t bytes = read(STDIN_FILENO, buf, BUF_SIZE - 1);
        if (bytes > 0) {
            buf[bytes] = '\0'; // Robimy poprawny string
            printf("Otrzymano z KLAWIATURY: %s", buf);

            if (strcmp(buf, "quit\n") == 0) {
                printf("Koniec programu!\n");
                break;
            }
        }
    }

    // --- SPRAWDZAMY ŹRÓDŁO 1: RURA (FIFO) ---
    if (fds[1].revents & POLLIN) {
        ssize_t bytes;
        // Pętla wyczyści wszystkie zakolejkowane wiadomości:
        while ((bytes = read(fds[1].fd, buf, BUF_SIZE - 1)) > 0) {
            buf[bytes] = '\0';
            printf("Otrzymano z RURY (FIFO): %s", buf);
        }
    }

    // --- OBSŁUGA ROZŁĄCZENIA KLIENTA RURY (POLLHUP) ---
    if (fds[1].revents & POLLHUP) {
        // Klient zamknął swój koniec rury. Przeotwieramy, by czekać na kolejnego:
        close(fds[1].fd);
        fds[1].fd = open("moj_pipe", O_RDONLY | O_NONBLOCK);
    }
}
```
