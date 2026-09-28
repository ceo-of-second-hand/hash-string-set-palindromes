"""Збірка та тестування лабораторної роботи (допоміжний інструмент, не частина рішення).

  python run_tests.py            # усе: ручні тести + випадкові 10^3..10^6 + таблиця часу
  python run_tests.py --quick    # без розміру 10^6
  python run_tests.py --sizes 1000 50000 --seeds 1 2 3

Кроки:
  1. компіляція lab / brute / gen (g++ -O2 -std=c++17);
  2. ручні тести tests/cases/*.in порівнюються з *.ans (і для lab, і для brute);
  3. випадкові тести: gen -> lab та brute, результати мають збігатися побайтово;
     кожен тест також запускається з двома різними основами хешу p (-s);
  4. таблиця часу за розмірами -> tests/out/timings.md.
Непройдений вхід зберігається у tests/out/ для відтворення.

Зв'язок однобічний: цей скрипт запускає .exe-програми, самі програми Python не
використовують і без нього працюють.
"""

import argparse    # розбір аргументів командного рядка (--quick, --sizes, --seeds)
import os
import re          # регулярні вирази — дістати число з "time_ms=..."
import shutil      # пошук g++ у PATH, копіювання невдалих входів
import subprocess  # запуск інших програм (g++, lab.exe, gen.exe, brute.exe)
import sys
import time
from pathlib import Path

# --- Шляхи до тек (відносно цього файлу) --------------------------------------
ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"                 # скомпільовані .exe
CASES = ROOT / "tests" / "cases"       # ручні тести: *.in (вхід) + *.ans (очікуване)
DATA = ROOT / "tests" / "data"         # тимчасові згенеровані входи
OUT = ROOT / "tests" / "out"           # таблиця часу та невдалі входи
EXE = ".exe" if os.name == "nt" else ""  # на Windows до імені програми додається .exe

# --- Профілі випадкових тестів --------------------------------------------------
# (назва, аргументи gen після "N seed"). {n} замінюється на N, {n3} — на N/3.
# Порядок аргументів gen: add del ask pool pal (див. gen.cpp).
PROFILES = [
    ("default", []),                                          # 50/20/30, пул N/3, 10 % паліндромів
    ("many_palindromes", ["50", "20", "30", "{n3}", "50"]),   # 50 % паліндромів
    ("queries_heavy", ["20", "0", "80", "{n3}", "10"]),       # переважно "?"
    ("delete_heavy", ["40", "40", "20", "{n3}", "10"]),       # багато "-"
    ("small_pool", ["50", "20", "30", "100", "10"]),          # 100 слів — постійні повтори
    ("big_set", ["70", "0", "30", "{n}", "10"]),              # велика множина, багато подвоєнь
]


def find_compiler():
    """Шукає g++: змінна середовища CXX -> PATH -> типові теки встановлення."""
    if os.environ.get("CXX"):
        return os.environ["CXX"]
    found = shutil.which("g++")
    if found:
        return found
    for cand in [r"C:\Program Files\CodeBlocks\MinGW\bin\g++.exe",
                 r"C:\msys64\ucrt64\bin\g++.exe", r"C:\msys64\mingw64\bin\g++.exe"]:
        if Path(cand).exists():
            return cand
    sys.exit("g++ not found: add it to PATH or set CXX")


def build():
    """Компілює main.cpp -> lab.exe, brute.cpp -> brute.exe, gen.cpp -> gen.exe.
    Та сама команда g++, що й при ручній збірці (плюс -Wall -Wextra — попередження)."""
    cxx = find_compiler()
    BUILD.mkdir(exist_ok=True)
    env = dict(os.environ)
    # MinGW потребує свої DLL у PATH
    env["PATH"] = str(Path(cxx).parent) + os.pathsep + env.get("PATH", "")
    for name in ["main", "brute", "gen"]:
        exe = BUILD / (("lab" if name == "main" else name) + EXE)
        cmd = [cxx, "-O2", "-std=c++17", "-Wall", "-Wextra", "-static",
               str(ROOT / f"{name}.cpp"), "-o", str(exe)]
        r = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if r.returncode != 0 or r.stderr.strip():
            print(r.stderr)            # показати помилки/попередження компілятора
        if r.returncode != 0:
            sys.exit(f"build failed: {name}.cpp")
    print(f"build ok ({cxx})")


# Ліміт часу на один запуск. Зависання (напр. зациклений ланцюг через баг)
# зараховується як провал, а не зупиняє весь прогін. Можна змінити: TEST_TIMEOUT=5.
TIMEOUT_S = int(os.environ.get("TEST_TIMEOUT", "60"))


def run(exe, args, stdin_path=None):
    """Запускає програму, повертає (stdout bytes, stderr str, секунди).
    Якщо програма впала або зависла — stdout містить повідомлення про це,
    тож порівняння з еталоном дасть провал, а не зупинку всього прогону."""
    t = time.perf_counter()
    # Файл подається програмі як стандартний вхід (як "lab.exe < file").
    with open(stdin_path, "rb") if stdin_path else open(os.devnull, "rb") as f:
        try:
            r = subprocess.run([str(BUILD / (exe + EXE))] + args, stdin=f,
                               capture_output=True, timeout=TIMEOUT_S)
        except subprocess.TimeoutExpired:
            return f"<{exe}: timeout {TIMEOUT_S}s>".encode(), "", TIMEOUT_S
    dt = time.perf_counter() - t
    if r.returncode != 0:
        return f"<{exe}: exit code {r.returncode}>".encode(), "", dt
    # \r\n (Windows) -> \n, щоб порівнювати виводи чесно
    return r.stdout.replace(b"\r\n", b"\n"), r.stderr.decode(errors="replace"), dt


def internal_ms(stderr):
    """Дістає число з рядка 'time_ms=...' (його друкує lab/brute з ключем -t)."""
    m = re.search(r"time_ms=([\d.]+)", stderr)
    return float(m.group(1)) if m else float("nan")


def manual_tests():
    """Ручні тести: кожен tests/cases/X.in запускається в lab і brute,
    вивід має точно збігтися з X.ans (відповідь, порахована вручну)."""
    ok = True
    for case in sorted(CASES.glob("*.in")):
        expected = case.with_suffix(".ans").read_bytes().replace(b"\r\n", b"\n")
        for exe in ["lab", "brute"]:
            got, _, _ = run(exe, [], case)
            if got != expected:
                ok = False
                print(f"FAIL {case.name} [{exe}]\n  expected: {expected!r}\n  got:      {got!r}")
    print(f"manual tests: {'ok' if ok else 'FAILED'} ({len(list(CASES.glob('*.in')))} cases)")
    return ok


def random_tests(sizes, seeds):
    """Випадкові тести: розмір x профіль x зерно.
    Для кожного: gen -> файл; brute -> еталон; lab двічі (дві різні основи p)
    -> має збігтися з еталоном. Для першого зерна записуємо час."""
    DATA.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    ok = True
    timings = []  # (profile, n, lab_ms, brute_ms, answers)
    for n in sizes:
        for prof, extra in PROFILES:
            prof_ok = True
            for seed in seeds:
                # 1) згенерувати вхід
                args = [a.format(n=n, n3=max(1, n // 3)) for a in extra]
                inp = DATA / f"{prof}_{n}_{seed}.txt"
                data, _, _ = run("gen", [str(n), str(seed)] + args)
                inp.write_bytes(data)

                # 2) еталонна відповідь
                ref, err_b, _ = run("brute", ["-t"], inp)
                # 3) наша програма з двома різними основами хешу — результат
                #    не повинен залежати від p
                outs = []
                for hseed in ["12345", "987654321"]:
                    got, err_l, _ = run("lab", ["-t", "-s", hseed], inp)
                    outs.append(got)
                    if got != ref:
                        ok = prof_ok = False
                        keep = OUT / f"FAILED_{inp.name}"
                        shutil.copy(inp, keep)   # зберегти вхід для відтворення
                        print(f"FAIL {prof} n={n} seed={seed} hash_seed={hseed} -> {keep}")
                # 4) час — лише для першого зерна (щоб таблиця мала 1 рядок на профіль)
                if seed == seeds[0]:
                    timings.append((prof, n, internal_ms(err_l), internal_ms(err_b),
                                    ref.count(b"\n")))
                inp.unlink()  # великі файли не зберігаємо (невдалі вже скопійовані в tests/out)
            print(f"  n={n:>8} {prof:<17} {'ok' if prof_ok else 'FAILED'}")
    print(f"random tests: {'ok' if ok else 'FAILED'}")
    return ok, timings


def write_timings(timings):
    """Таблиця часу (Markdown) -> tests/out/timings.md і на екран."""
    lines = ["| profile | N | lab, ms | brute (std::unordered_set), ms | lab ms / N, ns |",
             "|---|---:|---:|---:|---:|"]
    for prof, n, lab, br, _ in timings:
        lines.append(f"| {prof} | {n} | {lab:.2f} | {br:.2f} | {lab * 1e6 / n:.1f} |")
    text = "\n".join(lines) + "\n"
    (OUT / "timings.md").write_text(text, encoding="utf-8")
    print("\n" + text)


def main():
    # Аргументи: --quick (без 10^6), --sizes (свої розміри), --seeds (свої зерна gen).
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="skip N = 10^6")
    ap.add_argument("--sizes", type=int, nargs="+")
    ap.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3])
    a = ap.parse_args()
    sizes = a.sizes or ([1000, 10000, 100000] if a.quick else [1000, 10000, 100000, 1000000])

    build()
    ok = manual_tests()
    rok, timings = random_tests(sizes, a.seeds)
    write_timings(timings)
    ok = ok and rok
    print("ALL TESTS PASSED" if ok else "SOME TESTS FAILED")
    sys.exit(0 if ok else 1)   # код виходу: 0 — усе добре, 1 — є провали


if __name__ == "__main__":
    main()
