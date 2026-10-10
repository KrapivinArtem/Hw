#pragma once  // подключай этот файл не более одного раза


struct Point {
    double x, y;
};


inline bool operator==(const Point& a, const Point& b) {
    return a.x == b.x && a.y == b.y;
}

inline double distSquared(const Point& p) {
    return p.x * p.x + p.y * p.y;
}


struct ByDistance {
    bool operator()(const Point& a, const Point& b) const {
        return distSquared(a) < distSquared(b);
    }
};
