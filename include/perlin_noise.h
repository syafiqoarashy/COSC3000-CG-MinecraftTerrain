#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H

#include <vector>
#include <numeric>
#include <random>
#include <algorithm>
#include <cmath>

class PerlinNoise {
public:
    PerlinNoise(unsigned int seed = std::default_random_engine::default_seed) {
        // Initialize the permutation vector with the reference values
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);

        // Shuffle using the given seed
        std::default_random_engine engine(seed);
        std::shuffle(p.begin(), p.end(), engine);

        // Duplicate the permutation vector
        p.insert(p.end(), p.begin(), p.end());
    }

    double noise(double x, double y) const {
        // Find the unit grid cell containing the point
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;

        // Get the relative x and y coordinates of the point within the cell
        x -= std::floor(x);
        y -= std::floor(y);

        // Compute the fade curves for x and y
        double u = fade(x);
        double v = fade(y);

        // Hash the coordinates of the 4 cube corners
        int aa = p[p[X] + Y];
        int ab = p[p[X] + Y + 1];
        int ba = p[p[X + 1] + Y];
        int bb = p[p[X + 1] + Y + 1];

        // Add the blended results from the 4 corners of the cube
        double res = lerp(v, lerp(u, grad(p[aa], x, y), grad(p[ba], x - 1, y)),
            lerp(u, grad(p[ab], x, y - 1), grad(p[bb], x - 1, y - 1)));
        return (res + 1.0) / 2.0; // Map the result to [0, 1]
    }

private:
    double fade(double t) const {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    double lerp(double t, double a, double b) const {
        return a + t * (b - a);
    }

    double grad(int hash, double x, double y) const {
        int h = hash & 3;
        double u = h < 2 ? x : y;
        double v = h < 2 ? y : x;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    std::vector<int> p;
};

#endif
