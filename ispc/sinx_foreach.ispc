export void ispc_sinx_foreach(
    uniform int N,
    uniform int terms,
    uniform float* uniform x,
    uniform float* uniform result
) {
    foreach (i = 0 ... N) {
        float value = x[i];
        float numer = x[i] * x[i] * x[i];

        uniform int denom = 6;  // 3!
        uniform int sign = -1;

        for (uniform int j = 1; j <= terms; j++) {
            value += sign * numer / denom;

            numer *= x[i] * x[i];
            denom *= (2 * j + 2) * (2 * j + 3);
            sign *= -1;
        }

        result[i] = value;
    }
}