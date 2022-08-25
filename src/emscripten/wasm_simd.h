#include <stdint.h>
#include <wasm_simd128.h>

namespace emscripten_wasm_simd {

template<int n, int m, int n_stride>
void affine(const int8_t A[m][n_stride], const uint8_t x[n], const int32_t b[m], int32_t y[m]);

} // namespace emscripten_wasm_simd
