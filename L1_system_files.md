# SOP1 • Laboratorium 1: System plików i operacje w POSIX

> 📚 **Wybierz laboratorium:**  
> 👉 **Laboratorium 1: System plików, dirent, stat, nftw, getopt** *(bieżący dokument)*  
> 👉 [**Laboratorium 2: Niskopoziomowe I/O, mmap, FIFO, poll**](README.md)

> [!NOTE]
> **Przedmiot:** Systemy Operacyjne 1 (SOP1) • Politechnika Warszawska (Wydział MiNI)  
> **Zakres:** Laboratorium 1 • Operacje na plikach i katalogach, `dirent`, `stat`, `nftw`, `getopt`, uprawnienia oraz bezpieczne operacje na napisach.

---

## 📑 Spis Treści
1. [Gotowy Pakiet Nagłówków i Makr (Kopiuj-Wklej)](#01-gotowy-pakiet-nagłówków-i-makr-kopiuj-wklej)
2. [Parsowanie Argumentów: getopt i walidacja liczb (strtol)](#02-parsowanie-argumentów-getopt-i-walidacja-liczb-strtol)
3. [Wysokopoziomowe Operacje na Plikach (FILE*, fopen, fseek, fread)](#03-wysokopoziomowe-operacje-na-plikach-file-fopen-fseek-fread)
4. [Uprawnienia, umask i sprawdzanie dostępu (access, chmod)](#04-uprawnienia-umask-i-sprawdzanie-dostępu-access-chmod)
5. [Tworzenie i Usuwanie Katalogów oraz Plików (mkdir, unlink)](#05-tworzenie-i-usuwanie-katalogów-oraz-plików-mkdir-unlink)
6. [Linki i Metadane Plików: stat vs lstat, symlink, readlink](#06-linki-i-metadane-plików-stat-vs-lstat-symlink-readlink)
7. [Nawigacja i Przeglądanie Katalogów (opendir, readdir, getcwd, chdir)](#07-nawigacja-i-przeglądanie-katalogów-opendir-readdir-getcwd-chdir)
8. [Rekurencyjne Przechodzenie Drzewa Katalogów (nftw)](#08-rekurencyjne-przechodzenie-drzewa-katalogów-nftw)
9. [Bezpieczne Operacje na Napisach i Wejściu (getline, snprintf, strdup, pułapki)](#09-bezpieczne-operacje-na-napisach-i-wejściu-getline-snprintf-strdup-pułapki)
10. [Generator Liczb Losowych (srand, rand)](#10-generator-liczbl-losowych-srand-rand)

---

## 01. Gotowy Pakiet Nagłówków i Makr (Kopiuj-Wklej)

Poniższy blok zawiera wszystkie nagłówki i makra wymagane na Laboratorium 1. Możesz wkleić go na sam początek pliku `.c`, a zbędne linijki usunąć:

```c
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

/* Operacje na atrybutach plików i uprawnieniach */
#include <sys/types.h>
#include <sys/stat.h>

/* Przeglądanie katalogów */
#include <dirent.h>

/* Rekurencyjne przeszukiwanie drzewa katalogów */
#include <ftw.h>

/* Standardowe makro obsługi błędów systemowych (errno) */
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/* Funkcja usage informująca o poprawnym wywołaniu programu */
void usage(const char *pname) {
    fprintf(stderr, "USAGE: %s -n Name -p OCTAL -s SIZE\n", pname);
    exit(EXIT_FAILURE);
}

/* Opcjonalne pomocnicze stałe */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
```

> [!TIP]
> **Dlaczego `#define _XOPEN_SOURCE 500` i `#define _GNU_SOURCE` są na samej górze?**  
> Muszą znajdować się **przed** jakimkolwiek `#include`. Odblokowują one dostęp do funkcji standardu POSIX/XOPEN, takich jak `nftw()` czy `getline()`.

---

## 02. Parsowanie Argumentów: getopt i walidacja liczb (strtol)

Na SOP1 programy niemal zawsze przyjmują opcje w stylu `-n nazwa -p 0777 -s 1024`. Do ich parsowania służy funkcja `getopt()`.

### Sygnatura i zasada działania
```c
int getopt(int argc, char * const argv[], const char *optstring);
```
- Dwukropek `:` po literze oznacza, że **dana opcja wymaga argumentu** (wartość ląduje w zmiennej globalnej `optarg`).
- Brak dwukropka oznacza pojedynczą flagę (przełącznik typu bool).
- Gdy opcje się skończą, funkcja zwraca `-1`.

### Wzorzec z walidacją ósemkową (prawa dostępu) i dziesiętną:

```c
int c;
char *name = NULL;
mode_t perms = -1;
ssize_t size = -1;

while ((c = getopt(argc, argv, "p:n:s:")) != -1) {
    switch (c) {
        case 'p': {
            char *end;
            // Podstawa 8 (octal) dla uprawnień typu 0777, 0644!
            perms = strtol(optarg, &end, 8);
            // Sprawdzenie, czy użytkownik nie podał śmieci (np. "077abc")
            if (*end != '\0') {
                fprintf(stderr, "Błąd: Nieprawidłowe uprawnienia '%s'!\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        }
        case 'n':
            name = optarg;
            break;
        case 's': {
            char *end;
            // Podstawa 10 dla rozmiaru
            size = strtol(optarg, &end, 10);
            if (*end != '\0' || size <= 0) {
                fprintf(stderr, "Błąd: Nieprawidłowy rozmiar '%s'!\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        }
        default:
            fprintf(stderr, "Użycie: %s -p perms -n name -s size\n", argv[0]);
            exit(EXIT_FAILURE);
    }
}

// Sprawdzenie, czy wszystkie wymagane parametry zostały podane:
if (name == NULL || perms == (mode_t)-1 || size == -1) {
    fprintf(stderr, "Błąd: Brak wymaganych parametrów!\n");
    exit(EXIT_FAILURE);
}
```

### Jak dokładnie działa `strtol()`?

```c
#include <stdlib.h>
#include <errno.h>

long strtol(const char *str, char **endptr, int base);
```
- **`str`** — napis wejściowy do zamiany na liczbę.
- **`endptr`** — wskaźnik, do którego `strtol` wpisze **adres pierwszego znaku, na którym się zatrzymał** (pierwszego znaku niebędącego cyfrą w danej bazie). Jeśli nie potrzebujesz sprawdzania błędów, podaj `NULL`.
- **`base`** — system liczbowy:
  - `10` — dziesiętny (rozmiary, liczniki),
  - `8` — **ósemkowy** (prawa dostępu: zarówno `"0644"`, jak i `"644"`!),
  - `16` — szesnastkowy,
  - `0` — automatyczny (`0x...` → hex, `0...` → ósemkowy, reszta → dziesiętny).

#### Co zwraca `strtol(s, &end, 10)` w różnych przypadkach?

| Napis `s` | Zwrócone `val` | Na co wskazuje `end`? | Interpretacja |
| :--- | :--- | :--- | :--- |
| `"25"` | `25` | `'\0'` (koniec napisu) | ✅ Poprawna liczba (`*end == '\0'`) |
| `"07:Preheat"` | `7` | `':'` | ✅ Zatrzymał się dokładnie na separatorze |
| `"12abc"` | `12` | `'a'` | ❌ Śmieci za liczbą (`*end != '\0'`) |
| `"abc"` lub `""` | `0` | `s` (początek napisu) | ❌ Brak jakichkolwiek cyfr (`end == s`) |
| `"99999999999999999999"` | `LONG_MAX` | `'\0'` | ❌ Przepełnienie (`errno == ERANGE`) |

#### Wzorzec pełnej, kuloodpornej walidacji liczby:
```c
errno = 0;
char *end = NULL;
long val = strtol(optarg, &end, 10);

if (errno == ERANGE || end == optarg || *end != '\0') {
    fprintf(stderr, "Błąd: '%s' nie jest poprawną liczbą!\n", optarg);
    exit(EXIT_FAILURE);
}
```

> [!TIP]
> **Odwrotnie (liczba → napis):** W Linux/POSIX **nie istnieje funkcja `itoa()`**! Do zamiany liczby na napis zawsze używaj `snprintf()`:
> ```c
> char buf[32];
> snprintf(buf, sizeof(buf), "%04o", 0644); // -> "0644"
> ```

> [!WARNING]
> **Dlaczego na SOP1 nie wolno używać `atoi()`?**
> 1. `atoi("abc")` po cichu zwraca `0` — nie odróżnisz zera od błędnego napisu.
> 2. `atoi("12abc")` zwraca `12` — ignoruje śmieci na końcu.
> 3. Przepełnienie w `atoi()` to **Undefined Behavior** (zamiast `errno = ERANGE`).
> 4. `atoi("0644")` czyta liczbę jako **dziesiętne 644**, co całkowicie psuje bity uprawnień!

---

## 03. Wysokopoziomowe Operacje na Plikach (FILE*, fopen, fseek, fread)

W L1 pracujemy głównie na strumieniach standardowej biblioteki C (`FILE *`).

### Tabela Trybów fopen()

| Tryb | Opis | Plik istnieje | Plik NIE istnieje | Pozycja kursora |
| :--- | :--- | :--- | :--- | :--- |
| `"r"` | Tylko odczyt | Otwiera | Błąd (`NULL`) | Początek |
| `"w"` | Tylko zapis | **Czyści (0 bajtów)** | Tworzy nowy | Początek |
| `"a"` | Dopisywanie (Append) | Otwiera | Tworzy nowy | **Koniec** (każdy zapis na koniec) |
| `"r+"`| Odczyt i zapis | Otwiera | Błąd (`NULL`) | Początek |
| `"w+"`| Odczyt i zapis | **Czyści (0 bajtów)** | Tworzy nowy | Początek |
| `"a+"`| Odczyt i dopisywanie | Otwiera | Tworzy nowy | Czyta od początku / pisze na koniec |

### Wzorzec: Losowe wypełnianie pliku (Zadanie makeFile)

```c
void makeFile(const char *name, size_t size, mode_t perms, int percent) {
    FILE *fp;
    
    // Ustawienie umask, aby plik otrzymał dokładnie żądane uprawnienia
    umask(~perms & 0777);
    
    if ((fp = fopen(name, "w+")) == NULL)
        ERR("fopen");

    // Wypełnianie pliku w losowych pozycjach znakami 'A'-'Z'
    int count = (size * percent) / 100;
    for (int i = 0; i < count; i++) {
        long target_pos = rand() % size;
        if (fseek(fp, target_pos, SEEK_SET) != 0)
            ERR("fseek");
            
        char random_char = 'A' + (rand() % ('Z' - 'A' + 1));
        if (fprintf(fp, "%c", random_char) < 0)
            ERR("fprintf");
    }

    if (fclose(fp) != 0)
        ERR("fclose");
}
```

---

## 04. Uprawnienia, umask i sprawdzanie dostępu (access, chmod)

### 1. Sztuczka z umask: `umask(~perms & 0777)`
Funkcja `umask(mask)` maskuje (odbiera) bity uprawnień przy tworzeniu plików:
$$\text{Rzeczywiste uprawnienia} = \text{perms} \ \& \ (\sim\text{mask})$$
Dlatego, jeśli chcemy narzucić dokładne uprawnienia `perms`, ustawiamy:
```c
umask(~perms & 0777);
```

### 2. Zmiana uprawnień w locie: `chmod()`
```c
if (chmod("plik.txt", 0644) != 0)
    ERR("chmod");
```

### 3. Sprawdzanie uprawnień i istnienia: `access()`
```c
#include <unistd.h>
int access(const char *path, int amode);
```
Argument `amode` łączymy za pomocą bitowego OR (`|`):
- `F_OK` — czy plik w ogóle istnieje?
- `R_OK` — czy bieżący proces ma prawo do odczytu?
- `W_OK` — czy bieżący proces ma prawo do zapisu?
- `X_OK` — czy bieżący proces ma prawo do wykonania (execute)?

```c
// Przykład: Sprawdź czy plik istnieje i czy mamy prawo do czytania i pisania:
if (access("dane.bin", F_OK) == 0) {
    if (access("dane.bin", R_OK | W_OK) == 0) {
        printf("Plik istnieje i mamy pełny dostęp R/W!\n");
    }
}
```

---

## 05. Tworzenie i Usuwanie Katalogów oraz Plików (mkdir, unlink)

### Bezpieczne tworzenie katalogu (z ignorowaniem błędu, jeśli już istnieje):
```c
#include <sys/stat.h>
#include <sys/types.h>

if (mkdir(path, 0777) != 0) {
    if (errno != EEXIST) {
        ERR("mkdir");
    }
    // Jeśli errno == EEXIST — katalog już istniał, to nie jest błąd krytyczny!
}
```

### Bezpieczne usuwanie pliku:
```c
if (unlink(name) != 0) {
    if (errno != ENOENT) {
        ERR("unlink");
    }
    // Jeśli errno == ENOENT — pliku i tak nie było, możemy iść dalej
}
```

---

## 06. Linki i Metadane Plików: stat vs lstat, symlink, readlink

### `stat()` a `lstat()` — Podstawowa różnica

```c
struct stat sb;
stat(path, &sb);   // Podąża za symlinkiem i zwraca dane pliku DOCELOWEGO
lstat(path, &sb);  // Zwraca dane o SAMYM PLIKU LINKA (nie podąża za nim!)
```

### Sprawdzanie typu pliku przez makra w `sb.st_mode`:
- `S_ISREG(sb.st_mode)` — zwykły plik (regular file)
- `S_ISDIR(sb.st_mode)` — katalog
- `S_ISLNK(sb.st_mode)` — dowiązanie symboliczne (symlink)
- `S_ISFIFO(sb.st_mode)` — łącze nazwane (potok FIFO)

### Tworzenie linku symbolicznego: `symlink()`
```c
// symlink(target, linkpath);
symlink("../a/file.txt", "b/file.txt_bak");
```

### Odczytanie celu linku: `readlink()`
> [!CAUTION]
> **`readlink()` NIE dodaje znaku `'\0'` na końcu bufora!** Musisz dodać go ręcznie, bazując na liczbie zwróconych bajtów:

```c
char buf[PATH_MAX];
ssize_t len = readlink("my_symlink", buf, sizeof(buf) - 1);
if (len == -1)
    ERR("readlink");

buf[len] = '\0'; // Obowiązkowy krok!
printf("Symlink wskazuje na: %s\n", buf);
```

---

## 07. Nawigacja i Przeglądanie Katalogów (opendir, readdir, getcwd, chdir)

### Pobieranie bieżącej ścieżki (`getcwd`) oraz zmiana folderu (`chdir`):
```c
// getcwd(NULL, 0) dynamicznie alokuje pamięć (wymaga free!)
char *cwd = getcwd(NULL, 0);
if (cwd == NULL)
    ERR("getcwd");
printf("Bieżący katalog: %s\n", cwd);
free(cwd);

// Zmiana katalogu:
if (chdir("/tmp") != 0)
    ERR("chdir");
```

### Pętla przeglądania katalogu (`opendir` / `readdir` / `closedir`):

> [!WARNING]
> **Pułapka laboratoryjna z `readdir()` i `errno`:**  
> `readdir()` zwraca `NULL` zarówno gdy dotrze do końca katalogu, jak i w przypadku błędu!  
> Aby rozróżnić te dwa stany, **przed pętlą oraz na końcu każdej iteracji należy zresetować `errno = 0`**.

```c
DIR *dirp;
struct dirent *dp;
struct stat filestat;

if ((dirp = opendir(path)) == NULL)
    ERR("opendir");

errno = 0;
while ((dp = readdir(dirp)) != NULL) {
    // 1. Zawsze ignoruj wpisy "." oraz ".."!
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
        continue;

    // 2. UWAGA: lstat potrzebuje pełnej ścieżki lub uprzedniego chdir()!
    // Jeśli podasz samo dp->d_name, lstat będzie szukać w katalogu roboczym programu!
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dp->d_name);

    if (lstat(fullpath, &filestat) != 0)
        ERR("lstat");

    if (S_ISDIR(filestat.st_mode)) {
        printf("[DIR]  %s\n", dp->d_name);
    } else if (S_ISREG(filestat.st_mode)) {
        printf("[FILE] %s (%ld bajtów)\n", dp->d_name, (long)filestat.st_size);
    } else if (S_ISLNK(filestat.st_mode)) {
        printf("[LINK] %s\n", dp->d_name);
    }

    errno = 0; // Reset na koniec iteracji
}

// Jeśli po wyjściu z pętli errno != 0, oznacza to błąd odczytu katalogu!
if (errno != 0)
    ERR("readdir");

if (closedir(dirp) != 0)
    ERR("closedir");
```

---

## 08. Rekurencyjne Przechodzenie Drzewa Katalogów (nftw)

Gdy zadanie wymaga przeszukania całego drzewa folderów w głąb, ręczne pisanie rekurencji jest czasochłonne. POSIX oferuje gotową funkcję **`nftw()`** (*new file tree walk*).

```c
#define _XOPEN_SOURCE 500
#include <ftw.h>

// Funkcja zwrotna (callback) wywoływana dla każdego znalezionego pliku/folderu:
int process_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    /*
     * typeflag:
     *   FTW_F   - zwykły plik
     *   FTW_D   - katalog (odwiedzany przed dziećmi)
     *   FTW_DP  - katalog (odwiedzany po dzieciach)
     *   FTW_SL  - symlink (gdy użyto FTW_PHYS)
     *   FTW_DNR - katalog bez praw do odczytu
     *
     * ftwbuf->base  - indeks w fpath, gdzie zaczyna się sama nazwa pliku
     * ftwbuf->level - głębokość zagnieżdżenia w drzewie (0 = korzeń)
     */
    
    const char *filename = fpath + ftwbuf->base;
    
    if (typeflag == FTW_F) {
        printf("[Plik] Głębokość: %d, Nazwa: %s, Rozmiar: %ld\n", 
               ftwbuf->level, filename, (long)sb->st_size);
    } else if (typeflag == FTW_D) {
        printf("[Katalog] %s\n", fpath);
    }

    return 0; // Zwrócenie wartości innej niż 0 natychmiast zatrzymuje przeszukiwanie!
}

int main(void) {
    // Parametry:
    // 1. Ścieżka startowa
    // 2. Funkcja callback
    // 3. Maksymalna liczba jednocześnie otwartych deskryptorów (zazwyczaj 10-20)
    // 4. Flagi (FTW_PHYS = nie podążaj za symlinkami, jak lstat)
    if (nftw(".", process_file, 20, FTW_PHYS) != 0)
        ERR("nftw");

    return 0;
}
```

---

## 09. Bezpieczne Operacje na Napisach i Wejściu (getline, snprintf, strdup, pułapki)

### 1. `getline()` — Bezpieczne wczytywanie linii z obcięciem `\n`
```c
char *line = NULL;
size_t len = 0;
ssize_t read_bytes;

// getline automatycznie alokuje/rozszerza bufor przez malloc!
while ((read_bytes = getline(&line, &len, stdin)) > 0) {
    // Obcięcie znaku nowej linii '\n' jeśli występuje:
    if (line[read_bytes - 1] == '\n') {
        line[read_bytes - 1] = '\0';
    }

    printf("Wczytano: '%s'\n", line);
}

// Zawsze zwolnij zaalokowaną pamięć!
free(line);
```

### 2. `snprintf()` z przesunięciem (offset) do budowania ścieżek
```c
char path[PATH_MAX];
int offset = snprintf(path, sizeof(path), "%s/%s", "katalog", "podkatalog");

// Dopisywanie kolejnego segmentu przy użyciu offsetu:
if (offset > 0 && offset < sizeof(path)) {
    snprintf(path + offset, sizeof(path) - offset, "/%s", "plik.txt");
}
// Wynik: "katalog/podkatalog/plik.txt"
```

### 3. Klonowanie napisów: `strdup()`
```c
char *copy = strdup(original_str);
if (copy == NULL)
    ERR("strdup");

// ... użycie kopii ...

free(copy); // strdup używa malloc, więc pamięć musi zostać zwolniona!
```

### 4. Porównanie i pułapki funkcji wejścia

| Funkcja | Czy dodaje `\0`? | Czy ucina linię? | Pułapki / Uwagi |
| :--- | :--- | :--- | :--- |
| `fgets(buf, size, fp)` | **TAK** | Czyta całą linię z `\n` | Bezpieczna, zawsze gwarantuje `\0`. |
| `scanf("%s", buf)` | **TAK** | Zatrzymuje się na spacji | ❌ **Buffer overflow!** Używać z limitem: `scanf("%99s", buf)`. |
| `fscanf(fp, "%s", buf)`| **TAK** | Czyta tylko **jedno słowo** | Zatrzymuje się na spacji/tabulacji/enterze. |
| `strncpy(dst, src, n)` | ❌ **NIE ZAWSZE!** | Kopiuje do `n` znaków | 🚨 Jeśli `strlen(src) >= n`, `strncpy` **NIE dopisze `\0`**! |

---

## 10. Generator Liczb Losowych (srand, rand)

```c
#include <time.h>
#include <stdlib.h>

// 1. Inicjalizacja ziarna generatora (wywołać dokładnie RAZ w main!):
srand(time(NULL));

// 2. Losowa liczba całkowita z przedziału [min, max]:
int r = min + (rand() % (max - min + 1));

// 3. Losowy znak od 'A' do 'Z':
char c = 'A' + (rand() % ('Z' - 'A' + 1));
```
