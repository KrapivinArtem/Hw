// tests/test_sort.cpp
// Тесты КОРРЕКТНОСТИ mySort. Собираются в Debug с AddressSanitizer.
//
// Для каждого случая проверяем три вещи:
//   1) размер не изменился (ничего не потеряли и не добавили);
//   2) std::is_sorted — результат упорядочен по компаратору;
//   3) результат — ПЕРЕСТАНОВКА исходных данных (те же элементы с теми же кратностями).
// Только «отсортировано» недостаточно: функция, которая заполнит весь массив нулями,
// тоже даст «отсортированный» результат.
//
// Текст в консоль выводим по-английски (ASCII), чтобы не зависеть от кодировки консоли Windows.

#include "mySort.hpp"
#include "Point.hpp"

#include <algorithm>  // std::sort (ТОЛЬКО как эталон в тестах), std::is_sorted
#include <climits>    // INT_MIN, INT_MAX
#include <cstddef>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {  // анонимное пространство имён: имена видны только в этом файле

// Генератор случайных чисел с ФИКСИРОВАННЫМ зерном: тесты воспроизводимы —
// если тест упал, он упадёт так же и при следующем запуске, и можно отлаживать.
std::mt19937 g_rng(20261009);

int g_checks = 0;  // сколько проверок выполнено
int g_failed = 0;  // сколько из них провалено

// Одна проверка: считает и печатает только провалы.
void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::cout << "  [FAIL] " << what << '\n';
    }
}

// Печатает PASS, если с момента failedBefore не было ни одного провала.
void donePass(const std::string& name, int failedBefore) {
    if (g_failed == failedBefore) std::cout << "  [PASS] " << name << '\n';
}

// ---------------------------------------------------------------------------
// Универсальная проверка одного набора данных.
//   T         — тип элемента (задаём явно: checkCase<int>(...));
//   comp      — компаратор, по которому сортируем и проверяем is_sorted;
//   totalLess — ПОЛНЫЙ порядок, нужен только чтобы проверить «перестановку»:
//               сортируем им копии исходных и итоговых данных и сравниваем поэлементно.
//               Для Point comp (расстояние) не различает разные точки на одной окружности,
//               поэтому для сравнения multiset-ов нужен порядок по (x, y).
//   print     — печатать ли строку PASS (для массовых циклов отключаем).
// ---------------------------------------------------------------------------
template <typename T, typename Compare, typename TotalLess>
void checkCase(const std::string& name, std::vector<T> data, Compare comp, TotalLess totalLess,
               bool print = true) {
    const int failedBefore = g_failed;
    const std::vector<T> original = data;  // запоминаем исходные данные ДО сортировки

    mySort(data.begin(), data.end(), comp);  // проверяемая функция

    check(data.size() == original.size(), name + ": size unchanged");
    check(std::is_sorted(data.begin(), data.end(), comp), name + ": is_sorted");

    std::vector<T> a = original;
    std::vector<T> b = data;
    std::sort(a.begin(), a.end(), totalLess);
    std::sort(b.begin(), b.end(), totalLess);
    check(a == b, name + ": result is a permutation of the input");

    if (print) donePass(name, failedBefore);
}

// ---------------------------------------------------------------------------
// Набор обязательных случаев из задания — общий для int и Point.
//   gen() — возвращает случайный элемент типа T.
// ---------------------------------------------------------------------------
template <typename T, typename Gen, typename Compare, typename TotalLess>
void runCommonCases(const std::string& type, Gen gen, Compare comp, TotalLess totalLess) {
    // Лямбда: создаёт вектор из n случайных элементов.
    auto makeRandom = [&](std::size_t n) {
        std::vector<T> v;
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) v.push_back(gen());
        return v;
    };
    // Обратный компаратор: нужен, чтобы получить массив, отсортированный «наоборот».
    const auto reversedComp = [&](const T& a, const T& b) { return comp(b, a); };

    // 1. Пустой контейнер — mySort не должен упасть или обратиться к памяти.
    checkCase<T>(type + " / empty", std::vector<T>{}, comp, totalLess);

    // 2. Один элемент.
    checkCase<T>(type + " / one element", std::vector<T>{gen()}, comp, totalLess);

    // 3. Уже отсортированный массив (на нём срабатывает быстрый выход в mergeHalves).
    {
        std::vector<T> v = makeRandom(1000);
        std::sort(v.begin(), v.end(), comp);
        checkCase<T>(type + " / already sorted (n=1000)", v, comp, totalLess);
    }

    // 4. Массив в обратном порядке — типичный худший случай для наивного quicksort.
    {
        std::vector<T> v = makeRandom(1000);
        std::sort(v.begin(), v.end(), reversedComp);
        checkCase<T>(type + " / reverse sorted (n=1000)", v, comp, totalLess);
    }

    // 5. Все элементы равны (компаратор нигде не возвращает true).
    checkCase<T>(type + " / all equal (n=1000)", std::vector<T>(1000, gen()), comp, totalLess);

    // 6. Случайные данные разных размеров: маленький, средний, большой.
    const std::size_t sizes[] = {10, 1000, 100000};
    for (std::size_t n : sizes) {
        checkCase<T>(type + " / random n=" + std::to_string(n), makeRandom(n), comp, totalLess);
    }

    // 7. Много повторяющихся значений: всего 5 разных значений на 10000 элементов.
    {
        const std::vector<T> pool = makeRandom(5);
        std::uniform_int_distribution<std::size_t> pick(0, pool.size() - 1);
        std::vector<T> v;
        for (int i = 0; i < 10000; ++i) v.push_back(pool[pick(g_rng)]);
        checkCase<T>(type + " / many duplicates (5 distinct values, n=10000)", v, comp, totalLess);
    }

    // 8. ВСЕ размеры от 0 до 100. Здесь проверяются границы:
    //    порог сортировки вставками (16/17), чётные и нечётные размеры при делении пополам,
    //    первый уровень рекурсии и т.д. Ошибки «на единицу» (off-by-one) любят прятаться именно тут.
    {
        const int failedBefore = g_failed;
        for (std::size_t n = 0; n <= 100; ++n) {
            checkCase<T>(type + " / size " + std::to_string(n), makeRandom(n), comp, totalLess,
                         /*print=*/false);
        }
        donePass(type + " / every size 0..100", failedBefore);
    }
}

// ---------------------------------------------------------------------------
// Тесты для int
// ---------------------------------------------------------------------------
void testInt() {
    std::cout << "== int ==\n";
    const std::less<int> less{};

    std::uniform_int_distribution<int> dist(-1000000, 1000000);  // включает отрицательные
    auto gen = [&]() { return dist(g_rng); };

    runCommonCases<int>("int", gen, less, less);

    // Известный ответ, посчитанный вручную. Ловит ошибку, которую «is_sorted + перестановка»
    // пропустить не могут только при несогласованности самих проверок.
    {
        const int f0 = g_failed;
        std::vector<int> v = {5, -1, 3, 0, -7, 3};
        mySort(v.begin(), v.end());  // компаратор по умолчанию (std::less<int>)
        check(v == std::vector<int>({-7, -1, 0, 3, 3, 5}), "int / known answer (default comparator)");
        donePass("int / known answer, default comparator", f0);
    }

    // Только отрицательные числа.
    {
        std::uniform_int_distribution<int> neg(-1000, -1);
        std::vector<int> v;
        for (int i = 0; i < 1000; ++i) v.push_back(neg(g_rng));
        checkCase<int>("int / only negative values", v, less, less);
    }

    // Экстремальные значения. Если бы компаратор был написан как «a - b > 0», тут было бы
    // переполнение int. У нас сравнение «a < b», и тест это подтверждает.
    checkCase<int>("int / INT_MIN, INT_MAX, 0, -1, 1",
                   std::vector<int>{0, INT_MAX, INT_MIN, -1, 1, INT_MIN, INT_MAX, 0}, less, less);

    // Другой компаратор: std::greater даёт сортировку по убыванию. Проверяем, что Compare реально работает.
    {
        std::vector<int> v;
        for (int i = 0; i < 1000; ++i) v.push_back(dist(g_rng));
        checkCase<int>("int / std::greater (descending)", v, std::greater<int>{}, less);
    }

    // Обычный C-массив int[] (как в названии ДЗ): итераторами служат обычные указатели.
    // Здесь же ASan проверит выходы за границы массива на стеке.
    {
        const int f0 = g_failed;
        int arr[2000];
        for (int& x : arr) x = dist(g_rng);
        std::vector<int> expected(arr, arr + 2000);
        std::sort(expected.begin(), expected.end());  // эталон

        mySort(arr, arr + 2000);  // указатели вместо итераторов vector
        check(std::equal(expected.begin(), expected.end(), arr), "int[] / plain C array of 2000");
        donePass("int[] / plain C array of 2000", f0);
    }
}

// ---------------------------------------------------------------------------
// Тесты для Point
// ---------------------------------------------------------------------------
void testPoint() {
    std::cout << "== Point ==\n";

    // Полный порядок по (x, y) — нужен только для проверки «перестановки».
    const auto totalLess = [](const Point& a, const Point& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    };

    std::uniform_real_distribution<double> coord(-1000.0, 1000.0);  // и положительные, и отрицательные
    auto gen = [&]() {
        const double x = coord(g_rng);  // вычисляем по очереди, чтобы порядок был однозначным
        const double y = coord(g_rng);
        return Point{x, y};
    };

    runCommonCases<Point>("Point", gen, ByDistance{}, totalLess);

    // Известный ответ: расстояния 0, 0.707.., 1.414.., 5, 10.
    {
        const int f0 = g_failed;
        std::vector<Point> v = {{3, 4}, {1, 1}, {0, 0}, {-6, 8}, {0.5, 0.5}};
        mySort(v.begin(), v.end(), ByDistance{});
        const std::vector<Point> expected = {{0, 0}, {0.5, 0.5}, {1, 1}, {3, 4}, {-6, 8}};
        check(v == expected, "Point / known answer");
        donePass("Point / known answer", f0);
    }

    // Разные точки на ОДНОЙ окружности радиуса 5: компаратор считает их равными,
    // порядок между ними не важен, но ни одна не должна потеряться.
    checkCase<Point>("Point / equal distances, different coordinates",
                     std::vector<Point>{{3, 4}, {4, 3}, {5, 0}, {0, 5}, {-3, -4}, {0, 0}, {-5, 0}, {4, -3}},
                     ByDistance{}, totalLess);

    // Все точки только в отрицательной четверти (x < 0, y < 0): расстояние всё равно неотрицательное.
    {
        std::uniform_real_distribution<double> neg(-1000.0, -1.0);
        std::vector<Point> v;
        for (int i = 0; i < 1000; ++i) {
            const double x = neg(g_rng);
            const double y = neg(g_rng);
            v.push_back(Point{x, y});
        }
        checkCase<Point>("Point / negative coordinates only", v, ByDistance{}, totalLess);
    }

    // Лямбда вместо готового функтора — тоже годится как Compare.
    {
        std::vector<Point> v;
        for (int i = 0; i < 1000; ++i) v.push_back(gen());
        const auto byDist = [](const Point& a, const Point& b) {
            return a.x * a.x + a.y * a.y < b.x * b.x + b.y * b.y;
        };
        checkCase<Point>("Point / lambda comparator", v, byDist, totalLess);
    }
}

// ---------------------------------------------------------------------------
// Бонус: устойчивость (stable). У merge sort она гарантирована; std::sort её НЕ гарантирует.
// Сортируем по key; при равных key исходный порядок (поле id) должен сохраниться.
// ---------------------------------------------------------------------------
struct Item {
    int key;
    int id;
};

void testStability() {
    std::cout << "== stability ==\n";
    const int f0 = g_failed;

    std::uniform_int_distribution<int> keyDist(0, 9);  // всего 10 ключей — много равных
    std::vector<Item> v;
    for (int i = 0; i < 5000; ++i) v.push_back(Item{keyDist(g_rng), i});

    mySort(v.begin(), v.end(), [](const Item& a, const Item& b) { return a.key < b.key; });

    bool ok = true;
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (v[i - 1].key > v[i].key) ok = false;                           // нарушен порядок ключей
        if (v[i - 1].key == v[i].key && v[i - 1].id > v[i].id) ok = false;  // нарушена устойчивость
    }
    check(ok, "stability: equal keys keep their original order");
    donePass("stable (equal keys keep original order)", f0);
}

}  // namespace

int main() {
    testInt();
    testPoint();
    testStability();

    std::cout << "\nchecks: " << g_checks << ", failed: " << g_failed << '\n';
    std::cout << (g_failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << '\n';
    return g_failed == 0 ? 0 : 1;  // ненулевой код возврата = «тесты упали» (для ctest/CI)
}
