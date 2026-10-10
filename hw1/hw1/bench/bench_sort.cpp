// bench/bench_sort.cpp
// Тесты ПРОИЗВОДИТЕЛЬНОСТИ: mySort против std::sort.
// Собирать ТОЛЬКО в Release (оптимизации включены) и БЕЗ санитайзеров — они искажают время в разы.
//
// Методика измерения:
//   * исходные данные генерируются один раз (фиксированное зерно), перед каждым прогоном копируются —
//     время копирования в замер НЕ входит;
//   * для маленьких n делаем много повторов, для больших — меньше (общая работа примерно одинакова);
//   * берём МЕДИАНУ времён повторов (устойчива к случайным «выбросам» из-за других процессов);
//   * после каждого прогона проверяем, что результат отсортирован: бенчмарк неправильной сортировки
//     бессмысленен (заодно компилятор не может выкинуть сортировку как «неиспользуемую»);
//   * время измеряем std::chrono::steady_clock — «монотонные» часы, которые не прыгают назад.

#include "mySort.hpp"
#include "Point.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Обёртки, чтобы одним и тем же кодом замерять обе сортировки.
struct MySortFn {
    template <typename It, typename Cmp>
    void operator()(It f, It l, Cmp c) const { mySort(f, l, c); }
};
struct StdSortFn {
    template <typename It, typename Cmp>
    void operator()(It f, It l, Cmp c) const { std::sort(f, l, c); }
};

// Сколько повторов делать для размера n: ~3 млн элементов суммарной работы, но не меньше 5.
int repsFor(std::size_t n) {
    return static_cast<int>(std::max<std::size_t>(5, 3000000 / n));
}

// Медиана вектора времён (миллисекунды).
double median(std::vector<double> t) {
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

// Замер одной сортировки на данных base. Возвращает медианное время в миллисекундах.
template <typename T, typename Compare, typename SortFn>
double measureMs(const std::vector<T>& base, Compare comp, SortFn sortFn) {
    const int reps = repsFor(base.size());
    std::vector<double> times;
    times.reserve(static_cast<std::size_t>(reps));
    std::vector<T> work;

    for (int r = 0; r < reps; ++r) {
        work = base;  // свежая копия неотсортированных данных (вне замера)

        const auto t0 = Clock::now();
        sortFn(work.begin(), work.end(), comp);
        const auto t1 = Clock::now();

        if (!std::is_sorted(work.begin(), work.end(), comp)) {
            std::cerr << "ERROR: result is not sorted, benchmark is meaningless\n";
            std::exit(1);
        }
        times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return median(times);
}

// ----------------------------- генераторы данных ---------------------------
std::vector<int> randomInts(std::size_t n, std::mt19937& rng) {
    std::uniform_int_distribution<int> d(-1000000000, 1000000000);
    std::vector<int> v(n);
    for (int& x : v) x = d(rng);
    return v;
}

std::vector<Point> randomPoints(std::size_t n, std::mt19937& rng) {
    std::uniform_real_distribution<double> d(-1000.0, 1000.0);
    std::vector<Point> v(n);
    for (Point& p : v) {
        p.x = d(rng);
        p.y = d(rng);
    }
    return v;
}

// ------------------------------- печать таблиц -----------------------------
std::ofstream g_csv;  // дублируем цифры в файл bench_results.csv (рядом с запуском)

void printRow(const std::string& type, std::size_t n, double stdMs, double myMs) {
    const double ratio = myMs / stdMs;
    std::cout << std::setw(8) << type << std::setw(10) << n << std::fixed << std::setprecision(3)
              << std::setw(14) << stdMs << std::setw(14) << myMs << std::setprecision(2)
              << std::setw(10) << ratio << '\n';
    g_csv << type << ',' << n << ',' << stdMs << ',' << myMs << ',' << ratio << '\n';
}

// Часть 1: случайные данные, размеры 10^3 .. 10^6.
template <typename T, typename MakeFn, typename Compare>
void benchRandom(const std::string& type, MakeFn make, Compare comp) {
    for (std::size_t n : {1000u, 10000u, 100000u, 1000000u}) {
        const std::vector<T> base = make(n);
        const double stdMs = measureMs(base, comp, StdSortFn{});
        const double myMs = measureMs(base, comp, MySortFn{});
        printRow(type, n, stdMs, myMs);
    }
}

// Часть 2: один размер, разные «формы» входа. Сравниваем время mySort с тем, что на случайных данных.
template <typename T, typename Compare>
void benchShapes(const std::string& type, const std::vector<T>& random, Compare comp) {
    const std::size_t n = random.size();

    std::vector<T> sorted = random;
    std::sort(sorted.begin(), sorted.end(), comp);

    std::vector<T> reversed = sorted;
    std::reverse(reversed.begin(), reversed.end());

    const std::vector<T> allEqual(n, random.front());

    const double myRandom = measureMs(random, comp, MySortFn{});

    struct Shape {
        const char* name;
        const std::vector<T>* data;
    };
    const Shape shapes[] = {{"random", &random},
                            {"sorted", &sorted},
                            {"reversed", &reversed},
                            {"all equal", &allEqual}};

    for (const Shape& s : shapes) {
        const bool isRandom = (s.data == &random);
        const double stdMs = measureMs(*s.data, comp, StdSortFn{});
        // Для строки "random" берём уже готовое измерение myRandom: это и есть базовая линия,
        // повторный замер того же самого дал бы только другой шум и отношение != 1.00.
        const double myMs = isRandom ? myRandom : measureMs(*s.data, comp, MySortFn{});
        const double vsRandom = myMs / myRandom;
        std::cout << std::setw(8) << type << std::setw(12) << s.name << std::fixed
                  << std::setprecision(3) << std::setw(14) << stdMs << std::setw(14) << myMs
                  << std::setprecision(2) << std::setw(18) << vsRandom;
        // Деградация = заметно ХУДШЕЕ время, чем на случайных данных (порог — в 2 раза).
        if (vsRandom > 2.0) std::cout << "   <-- DEGRADATION";
        std::cout << '\n';
        g_csv << type << "_shape," << s.name << ',' << n << ',' << stdMs << ',' << myMs << ','
              << vsRandom << '\n';
    }
}

}  // namespace

int main() {
    std::mt19937 rng(42);  // фиксированное зерно — одинаковые данные при каждом запуске
    g_csv.open("bench_results.csv");
    g_csv << "type,n,std_sort_ms,mySort_ms,ratio\n";

#ifdef NDEBUG
    std::cout << "Build: Release (NDEBUG defined)\n";
#else
    std::cout << "WARNING: NDEBUG is not defined - this is NOT a Release build, numbers are meaningless!\n";
#endif

    std::cout << "\n=== Part 1: random data, median time of several runs (ms) ===\n";
    std::cout << std::setw(8) << "type" << std::setw(10) << "n" << std::setw(14) << "std::sort"
              << std::setw(14) << "mySort" << std::setw(10) << "my/std" << '\n';

    benchRandom<int>("int", [&](std::size_t n) { return randomInts(n, rng); }, std::less<int>{});
    benchRandom<Point>("Point", [&](std::size_t n) { return randomPoints(n, rng); }, ByDistance{});

    const std::size_t kBig = 100000;  // 10^5, как в задании
    std::cout << "\n=== Part 2: input shapes at n = " << kBig << " (ms) ===\n";
    std::cout << std::setw(8) << "type" << std::setw(12) << "input" << std::setw(14) << "std::sort"
              << std::setw(14) << "mySort" << std::setw(18) << "mySort/mySort-rnd" << '\n';

    benchShapes<int>("int", randomInts(kBig, rng), std::less<int>{});
    benchShapes<Point>("Point", randomPoints(kBig, rng), ByDistance{});

    std::cout << "\nResults also saved to bench_results.csv\n";
    return 0;
}
