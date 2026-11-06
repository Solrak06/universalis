// Pruebas de los ambientes de estudio (Musica.cpp): se generan, no saturan, no estan en
// silencio y el bucle empalma sin salto. Lo compila y ejecuta compilar.bat / probar.bat.
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Musica.hpp"

int main() {
    int fallos = 0;
    for (int s = musica::LLUVIA; s < musica::MIA; ++s) {
        std::vector<short> m;
        DWORD t0 = GetTickCount();
        bool ok = musica::generar(s, m);
        DWORD ms = GetTickCount() - t0;
        double suma = 0;
        int pico = 0, salto = 0, saltoMedio = 0;
        for (std::size_t i = 0; i < m.size(); ++i) {
            suma += (double)m[i] * m[i];
            pico = std::max(pico, std::abs((int)m[i]));
        }
        // Salto en el empalme comparado con el salto tipico entre muestras seguidas
        std::size_t n = m.size() / 2;
        salto = std::abs(m[0] - m[(n - 1) * 2]);
        long long acumulado = 0;
        for (std::size_t i = 1; i < n; ++i) acumulado += std::abs(m[i * 2] - m[(i - 1) * 2]);
        saltoMedio = (int)(acumulado / (long long)(n - 1));
        double rms = std::sqrt(suma / std::max<std::size_t>(1, m.size()));
        bool bien = ok && m.size() > 100000 && pico <= 32767 && rms > 500 && salto <= saltoMedio * 8 + 400 && ms < 5000;
        std::printf("%-16ls %5lu ms  rms %6.0f  pico %5d  empalme %5d (normal %d)  %s\n", musica::nombre(s), ms, rms, pico,
                    salto, saltoMedio, bien ? "ok" : "FALLO");
        if (!bien) ++fallos;
    }
    // Campanillas del temporizador: suenan, no saturan y acaban en silencio (sin chasquido)
    for (int tipo = 0; tipo < 2; ++tipo) {
        std::vector<short> m;
        musica::generarCampana(tipo, m);
        int pico = 0;
        for (short x : m) pico = std::max(pico, std::abs((int)x));
        bool bien = m.size() > 20000 && pico > 8000 && pico <= 32767 && std::abs((int)m.back()) < 50;
        std::printf("Campanilla %d      pico %5d  final %d  %s\n", tipo, pico, (int)m.back(), bien ? "ok" : "FALLO");
        if (!bien) ++fallos;
    }
    std::printf("%d fallos\n", fallos);
    return fallos ? 1 : 0;
}
