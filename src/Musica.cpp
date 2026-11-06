// Musica.cpp - Sonido de fondo para estudiar (ver Musica.hpp).
//
// Los ambientes se sintetizan una vez (unas decimas de segundo) en un bucle de 48 s y se
// reproducen con waveOut en bucle infinito. Todo lo que suena cerca del final se escribe
// «dando la vuelta» (indice modulo la longitud) o se funde con el principio, asi que el
// empalme no se oye. «Mi musica» usa MCI, que abre mp3, wav, wma y m4a.
#include "Musica.hpp"

#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>

#include "Rutas.hpp"

namespace musica {

namespace {

const int FRECUENCIA = 22050;
const int SEGUNDOS = 48;
const int N = FRECUENCIA * SEGUNDOS;  // muestras por canal
const double PI = 3.14159265358979323846;
const UINT_PTR TIMER_FUNDIDO = 1;

struct Info {
    const wchar_t* nombre;
    const wchar_t* descripcion;
    COLORREF color;
};

const Info INFO[N_SONIDOS] = {
    {L"Silencio", L"Sin sonido", RGB(148, 163, 184)},
    {L"Lluvia", L"Lluvia constante con gotas", RGB(96, 165, 250)},
    {L"Olas del mar", L"Olas que van y vienen", RGB(45, 212, 191)},
    {L"Ruido marrón", L"Grave y uniforme: tapa los ruidos", RGB(217, 119, 6)},
    {L"Chimenea", L"Fuego que crepita", RGB(251, 146, 60)},
    {L"Piano tranquilo", L"Acordes lentos y suaves", RGB(167, 139, 250)},
    {L"Ondas alfa", L"Tono binaural para concentrarse (con cascos)", RGB(244, 114, 182)},
    {L"Tormenta", L"Lluvia fuerte y truenos lejanos", RGB(129, 140, 248)},
    {L"Bosque", L"Viento entre los árboles y pájaros", RGB(132, 204, 22)},
    {L"Cafetería", L"Murmullo de gente y tazas", RGB(214, 158, 96)},
    {L"Arroyo", L"Agua que corre entre piedras", RGB(56, 189, 248)},
    {L"Mi música", L"Tus canciones de la carpeta Música", RGB(52, 211, 153)},
};

// Estado de la reproduccion
HWAVEOUT g_salida = NULL;
WAVEHDR g_cabecera = WAVEHDR();
std::map<int, std::vector<short> > g_cache;  // ambientes ya sintetizados
int g_sonando = NADA;
int g_volumen = 60;
int g_ultimo = LLUVIA;
bool g_preferencias = false;
// Fundido de entrada
int g_fundido = 0;  // 0..100 del volumen elegido
HWND g_ventana = NULL;  // ventana oculta para los avisos de MCI y el temporizador del fundido
// Mi musica
std::vector<std::wstring> g_lista;
std::size_t g_pista = 0;
bool g_mciAbierto = false;

// ------------------------------------------------------------- sintesis

struct Azar {
    std::mt19937 g;
    std::uniform_real_distribution<double> u{-1.0, 1.0};
    explicit Azar(unsigned semilla) : g(semilla) {}
    double operator()() { return u(g); }
    double entre(double a, double b) { return a + (b - a) * (u(g) * 0.5 + 0.5); }
};

// Buffer estereo en coma flotante con escritura circular.
struct Lienzo {
    std::vector<double> i, d;
    Lienzo() : i(N, 0.0), d(N, 0.0) {}
    void sumar(long k, double l, double r) {
        k %= N;
        if (k < 0) k += N;
        i[(std::size_t)k] += l;
        d[(std::size_t)k] += r;
    }
};

// Ruido continuo: se generan N + F muestras y las F ultimas se funden con las primeras.
template <typename F>
void continuo(Lienzo& z, F muestra) {
    const int fundido = FRECUENCIA * 2;
    std::vector<double> l(N + fundido), r(N + fundido);
    for (int k = 0; k < N + fundido; ++k) muestra(k, l[(std::size_t)k], r[(std::size_t)k]);
    for (int k = 0; k < N; ++k) {
        double a = l[(std::size_t)k], b = r[(std::size_t)k];
        if (k < fundido) {
            double t = (double)k / fundido, c = std::sqrt(t), e = std::sqrt(1 - t);
            a = a * c + l[(std::size_t)(N + k)] * e;
            b = b * c + r[(std::size_t)(N + k)] * e;
        }
        z.i[(std::size_t)k] += a;
        z.d[(std::size_t)k] += b;
    }
}

// Ruido marron (paseo aleatorio con fuga) por canal.
struct Marron {
    double v = 0;
    double operator()(double blanco) {
        v = 0.997 * v + 0.045 * blanco;
        return v;
    }
};

struct PasoBajo {
    double v = 0, a;
    explicit PasoBajo(double a_) : a(a_) {}
    double operator()(double x) {
        v += a * (x - v);
        return v;
    }
};

void lluvia(Lienzo& z) {
    Azar az(11);
    PasoBajo l1(0.35), l2(0.012), r1(0.35), r2(0.012);
    continuo(z, [&](int k, double& l, double& r) {
        double mod = 0.85 + 0.15 * std::sin(2 * PI * k / (FRECUENCIA * 12.0));
        double a = l1(az()), b = r1(az());
        l = (a - l2(a)) * 0.9 * mod;
        r = (b - r2(b)) * 0.9 * mod;
    });
    // Gotas: chasquidos cortos y agudos repartidos por el estereo
    int gotas = SEGUNDOS * 30;
    for (int g = 0; g < gotas; ++g) {
        long t = (long)(az.entre(0, 1) * N);
        double amp = std::pow(az.entre(0.05, 1.0), 2.0) * 0.5, pan = az.entre(0.1, 0.9);
        int largo = (int)(FRECUENCIA * az.entre(0.004, 0.02));
        double prev = 0;
        for (int k = 0; k < largo; ++k) {
            double x = az(), hp = x - prev;
            prev = x;
            double e = amp * std::exp(-6.0 * k / largo);
            z.sumar(t + k, hp * e * (1 - pan), hp * e * pan);
        }
    }
}

void olas(Lienzo& z) {
    Azar az(23);
    Marron ml, mr;
    PasoBajo hl(0.25), hr(0.25);
    const double periodo = FRECUENCIA * 8.0;  // 6 olas exactas en el bucle
    continuo(z, [&](int k, double& l, double& r) {
        double f = 0.5 - 0.5 * std::cos(2 * PI * k / periodo);
        double f2 = 0.5 - 0.5 * std::cos(2 * PI * (k + FRECUENCIA * 0.6) / periodo);
        double el = 0.2 + 0.8 * f * f, er = 0.2 + 0.8 * f2 * f2;
        l = ml(az()) * 0.9 * el + hl(az()) * 0.35 * el * el;
        r = mr(az()) * 0.9 * er + hr(az()) * 0.35 * er * er;
    });
}

void marron(Lienzo& z) {
    Azar az(37);
    Marron ml, mr;
    continuo(z, [&](int, double& l, double& r) {
        l = ml(az());
        r = mr(az());
    });
}

void chimenea(Lienzo& z) {
    Azar az(41);
    Marron ml, mr;
    PasoBajo pl(0.08), pr(0.08);
    continuo(z, [&](int k, double& l, double& r) {
        double mod = 0.8 + 0.2 * std::sin(2 * PI * k / (FRECUENCIA * 6.0)) * std::sin(2 * PI * k / (FRECUENCIA * 16.0));
        l = pl(ml(az())) * 2.2 * mod;
        r = pr(mr(az())) * 2.2 * mod;
    });
    // Chasquidos: unos sueltos y otros en rafagas
    int n = SEGUNDOS * 7;
    for (int c = 0; c < n; ++c) {
        long t = (long)(az.entre(0, 1) * N);
        int rafaga = az.entre(0, 1) < 0.25 ? (int)az.entre(2, 6) : 1;
        double pan = az.entre(0.2, 0.8);
        for (int b = 0; b < rafaga; ++b) {
            double amp = std::pow(az.entre(0.1, 1.0), 1.5) * 0.7;
            int largo = (int)(FRECUENCIA * az.entre(0.001, 0.006));
            double prev = 0;
            for (int k = 0; k < largo; ++k) {
                double x = az(), hp = x - prev;
                prev = x;
                double e = amp * std::exp(-5.0 * k / largo);
                z.sumar(t + k, hp * e * (1 - pan), hp * e * pan);
            }
            t += (long)(FRECUENCIA * az.entre(0.01, 0.06));
        }
    }
}

double frecuenciaNota(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }

// Nota de piano: armonicos que se apagan antes cuanto mas agudos.
void notaPiano(Lienzo& z, double inicio, int midi, double fuerza, double pan, double duracion) {
    double f = frecuenciaNota(midi);
    long t0 = (long)(inicio * FRECUENCIA);
    int largo = (int)(duracion * FRECUENCIA);
    const double amp[] = {1.0, 0.42, 0.22, 0.12, 0.06, 0.03};
    // Osciladores recursivos (rotacion y caida multiplicativa): mucho mas rapido que sin/exp por muestra
    double re[6], im[6], cr[6], ci[6], caida[6];
    int armonicos = 0;
    for (int h = 0; h < 6; ++h) {
        double fh = f * (h + 1) * (1 + 0.0004 * h * h);
        if (fh > FRECUENCIA / 2.2) break;
        double w = 2 * PI * fh / FRECUENCIA;
        re[h] = 0;
        im[h] = amp[h];
        cr[h] = std::cos(w);
        ci[h] = std::sin(w);
        caida[h] = std::exp(-(0.9 + 0.7 * h) / FRECUENCIA);
        armonicos = h + 1;
    }
    int ataque = (int)(0.006 * FRECUENCIA);
    for (int k = 0; k < largo; ++k) {
        double v = 0;
        for (int h = 0; h < armonicos; ++h) {
            double nr = (re[h] * cr[h] - im[h] * ci[h]) * caida[h];
            im[h] = (re[h] * ci[h] + im[h] * cr[h]) * caida[h];
            re[h] = nr;
            v += re[h];
        }
        v *= fuerza * std::min(1.0, (double)k / ataque);
        z.sumar(t0 + k, v * (1 - pan) * 0.9, v * pan * 0.9);
    }
}

void piano(Lienzo& z) {
    Azar az(5);
    // Cmaj9 - Am9 - Fmaj7 - G6, tres vueltas de compases de 4 s
    const int acordes[4][4] = {{48, 55, 64, 71}, {45, 52, 60, 67}, {41, 48, 57, 64}, {43, 50, 59, 64}};
    const int escala[] = {72, 74, 76, 79, 81, 84, 86, 88};  // pentatonica de do, arriba
    for (int compas = 0; compas < SEGUNDOS / 4; ++compas) {
        const int* a = acordes[compas % 4];
        double t = compas * 4.0;
        notaPiano(z, t, a[0] - 12, 0.30, 0.45, 6.0);  // bajo
        for (int k = 1; k < 4; ++k) notaPiano(z, t + 0.12 * k + az.entre(0, 0.03), a[k], 0.13, 0.3 + 0.15 * k, 5.0);
        // Melodia: unas pocas notas en corcheas, cerca de la anterior
        int notas = (int)az.entre(2, 5.99), pos = (int)az.entre(0, 7.99);
        int grado = (int)az.entre(0, 7.99);
        for (int n = 0; n < notas; ++n) {
            double cuando = t + 0.5 * ((pos + n * 2 + (int)az.entre(0, 1.99)) % 8) + az.entre(0, 0.02);
            grado = std::max(0, std::min(7, grado + (int)std::lround(az.entre(-2, 2))));
            notaPiano(z, cuando, escala[grado], az.entre(0.12, 0.2), az.entre(0.3, 0.7), 3.5);
        }
    }
    // Reverberacion sencilla: dos ecos con realimentacion, dando dos vueltas al bucle
    const int d1 = (int)(0.113 * FRECUENCIA), d2 = (int)(0.171 * FRECUENCIA);
    for (int vuelta = 0; vuelta < 2; ++vuelta)
        for (int k = 0; k < N; ++k) {
            std::size_t a1 = (std::size_t)((k - d1 + N) % N), a2 = (std::size_t)((k - d2 + N) % N);
            z.i[(std::size_t)k] += 0.28 * z.d[a1] + 0.22 * z.i[a2];
            z.d[(std::size_t)k] += 0.28 * z.i[a1] + 0.22 * z.d[a2];
        }
    // Un colchon de ruido muy bajo para que no haya silencio total
    Azar ruido(9);
    PasoBajo pl(0.02), pr(0.02);
    continuo(z, [&](int, double& l, double& r) {
        l = pl(ruido()) * 0.05;
        r = pr(ruido()) * 0.05;
    });
}

void alfa(Lienzo& z) {
    // 200 Hz en un oido y 210 Hz en el otro: el cerebro «oye» 10 Hz (ritmo alfa)
    Azar az(3);
    Marron ml, mr;
    continuo(z, [&](int k, double& l, double& r) {
        double t = (double)k / FRECUENCIA;
        l = 0.22 * std::sin(2 * PI * 200 * t) + ml(az()) * 0.35;
        r = 0.22 * std::sin(2 * PI * 210 * t) + mr(az()) * 0.35;
    });
}

double rms(const Lienzo& z) {
    double s = 0;
    for (int k = 0; k < N; ++k) s += z.i[(std::size_t)k] * z.i[(std::size_t)k] + z.d[(std::size_t)k] * z.d[(std::size_t)k];
    return std::sqrt(s / (2.0 * N));
}

void tormenta(Lienzo& z) {
    lluvia(z);
    // Lluvia mas densa: un segundo manto, mas grave
    Azar az(71);
    PasoBajo l1(0.12), r1(0.12);
    continuo(z, [&](int k, double& l, double& r) {
        double mod = 0.8 + 0.2 * std::sin(2 * PI * k / (FRECUENCIA * 16.0));
        l = l1(az()) * 0.7 * mod;
        r = r1(az()) * 0.7 * mod;
    });
    double base = rms(z);
    // Truenos: un chasquido grave y un retumbar largo que se apaga. Se escriben dando la vuelta.
    Lienzo trueno;
    const double momentos[] = {6.5, 21.0, 37.5};
    for (int t = 0; t < 3; ++t) {
        Marron m;
        PasoBajo grave(0.018), grave2(0.03);
        double pan = 0.3 + 0.2 * t, dura = az.entre(5.0, 8.0);
        long t0 = (long)(momentos[t] * FRECUENCIA);
        int largo = (int)(dura * FRECUENCIA), ataque = (int)(0.25 * FRECUENCIA);
        double fuerza = az.entre(0.6, 1.0);
        for (int k = 0; k < largo; ++k) {
            double tt = (double)k / FRECUENCIA;
            double env = std::min(1.0, (double)k / ataque) * std::exp(-tt / (dura * 0.32));
            // Retumbos: la envolvente sube y baja varias veces mientras se apaga
            env *= 0.55 + 0.45 * std::fabs(std::sin(2 * PI * tt * 0.9 + t)) * std::fabs(std::sin(2 * PI * tt * 0.37));
            double v = grave2(grave(m(az()) * 3.0)) * env * fuerza;
            trueno.sumar(t0 + k, v * (1 - pan) * 2, v * pan * 2);
        }
    }
    double pico = 1e-9;
    for (int k = 0; k < N; ++k) pico = std::max(pico, std::fabs(trueno.i[(std::size_t)k]) + std::fabs(trueno.d[(std::size_t)k]));
    double g = base * 3.2 / pico;
    for (int k = 0; k < N; ++k) {
        z.i[(std::size_t)k] += trueno.i[(std::size_t)k] * g;
        z.d[(std::size_t)k] += trueno.d[(std::size_t)k] * g;
    }
}

// Canto de un pajaro: unas notas cortas con un barrido de tono.
void pajaro(Lienzo& z, Azar& az, double inicio, double distancia) {
    double f0 = az.entre(2600, 4200), pan = az.entre(0.1, 0.9);
    int notas = (int)az.entre(2, 7.99);
    double barrido = az.entre(-0.35, 0.45), hueco = az.entre(0.06, 0.16), dura = az.entre(0.05, 0.14);
    double amp = 0.22 / distancia;
    long t = (long)(inicio * FRECUENCIA);
    for (int n = 0; n < notas; ++n) {
        int largo = (int)(dura * FRECUENCIA);
        double fase = 0, f = f0 * (1 + az.entre(-0.06, 0.06));
        for (int k = 0; k < largo; ++k) {
            double x = (double)k / largo;
            double fi = f * (1 + barrido * x) * (1 + 0.03 * std::sin(2 * PI * 38 * k / FRECUENCIA));
            fase += 2 * PI * fi / FRECUENCIA;
            double env = std::sin(PI * x);
            double v = std::sin(fase) * env * env * amp;
            z.sumar(t + k, v * (1 - pan), v * pan);
        }
        t += (long)((dura + hueco) * FRECUENCIA);
    }
}

void bosque(Lienzo& z) {
    Azar az(83);
    Marron ml, mr;
    PasoBajo vl(0.05), vr(0.05), hl(0.5), hr(0.5), sl(0.08), sr(0.08);
    continuo(z, [&](int k, double& l, double& r) {
        double t = (double)k / FRECUENCIA;
        // Rachas de viento lentas y hojas que se mueven con ellas
        double racha = 0.55 + 0.45 * std::sin(2 * PI * t / 12.0) * std::sin(2 * PI * t / 7.0 + 1.0);
        double a = az(), b = az();
        double hojasL = (hl(a) - sl(a)) * 0.25 * racha * racha, hojasR = (hr(b) - sr(b)) * 0.25 * racha * racha;
        l = vl(ml(az())) * 1.6 * racha + hojasL;
        r = vr(mr(az())) * 1.6 * racha + hojasR;
    });
    // Unos cuantos pajaros, cerca y lejos
    for (double t = 0.5; t < SEGUNDOS - 1.5; t += az.entre(1.2, 3.8)) pajaro(z, az, t, az.entre(1.0, 3.5));
}

void cafeteria(Lienzo& z) {
    Azar az(97);
    // Murmullo: varias voces lejanas (tono con armonicos y soplo) que hablan a rafagas
    const int VOCES = 9;
    struct Voz {
        double f0, fase, silabas, frase, desfase, pan, fuerza;
        PasoBajo filtro{0.18}, filtro2{0.08}, env{0.004};
    };
    std::vector<Voz> voces((std::size_t)VOCES);
    for (Voz& v : voces) {
        v.f0 = az.entre(95, 230);
        v.fase = 0;
        v.silabas = az.entre(3.2, 5.5);
        v.frase = az.entre(4.0, 9.0);
        v.desfase = az.entre(0, 2 * PI);
        v.pan = az.entre(0.15, 0.85);
        v.fuerza = az.entre(0.5, 1.0);
    }
    Marron ml, mr;
    PasoBajo sala(0.04), salaR(0.04);
    continuo(z, [&](int k, double& l, double& r) {
        double t = (double)k / FRECUENCIA;
        l = sala(ml(az())) * 0.6;
        r = salaR(mr(az())) * 0.6;
        for (Voz& v : voces) {
            double frase = std::sin(2 * PI * t / v.frase + v.desfase);
            if (frase < -0.2) {  // en pausa: solo se apaga poco a poco
                v.env(0);
                continue;
            }
            double sil = 0.5 + 0.5 * std::sin(2 * PI * v.silabas * t + v.desfase * 3);
            double e = v.env(sil * sil * std::min(1.0, (frase + 0.2) * 2));
            double f = v.f0 * (1 + 0.08 * std::sin(2 * PI * 0.7 * t + v.desfase));
            v.fase += f / FRECUENCIA;
            v.fase -= std::floor(v.fase);
            double sierra = 2 * v.fase - 1;
            double x = v.filtro2(v.filtro(sierra * 0.7 + az() * 0.3)) * e * v.fuerza;
            l += x * (1 - v.pan) * 1.4;
            r += x * v.pan * 1.4;
        }
    });
    // Tazas y cucharillas, de vez en cuando
    for (double t = 1.0; t < SEGUNDOS - 1.0; t += az.entre(2.0, 6.0)) {
        double f = az.entre(2200, 3600), pan = az.entre(0.1, 0.9), amp = az.entre(0.04, 0.1);
        int golpes = az.entre(0, 1) < 0.4 ? 2 : 1;
        long t0 = (long)(t * FRECUENCIA);
        for (int g = 0; g < golpes; ++g) {
            int largo = (int)(0.35 * FRECUENCIA);
            for (int k = 0; k < largo; ++k) {
                double tt = (double)k / FRECUENCIA;
                double v = (std::sin(2 * PI * f * tt) + 0.5 * std::sin(2 * PI * f * 2.71 * tt)) * std::exp(-tt * 14) * amp;
                z.sumar(t0 + k, v * (1 - pan), v * pan);
            }
            t0 += (long)(az.entre(0.12, 0.25) * FRECUENCIA);
        }
    }
}

void arroyo(Lienzo& z) {
    Azar az(59);
    // Corriente: ruido filtrado con un temblor rapido e irregular
    PasoBajo l1(0.30), l2(0.04), r1(0.30), r2(0.04), tl(0.002), tr(0.002);
    continuo(z, [&](int, double& l, double& r) {
        double a = l1(az()), b = r1(az());
        double ml = 0.6 + 4.0 * std::fabs(tl(az())), mr = 0.6 + 4.0 * std::fabs(tr(az()));
        l = (a - l2(a)) * 0.55 * ml;
        r = (b - r2(b)) * 0.55 * mr;
    });
    // Burbujas: tonos cortos que suben, muchisimas y flojas
    int burbujas = SEGUNDOS * 45;
    for (int n = 0; n < burbujas; ++n) {
        long t = (long)(az.entre(0, 1) * N);
        double f = az.entre(450, 1600), pan = az.entre(0.05, 0.95);
        double amp = std::pow(az.entre(0.1, 1.0), 2.0) * 0.12, dura = az.entre(0.015, 0.05);
        int largo = (int)(dura * FRECUENCIA);
        double fase = 0;
        for (int k = 0; k < largo; ++k) {
            double x = (double)k / largo;
            fase += 2 * PI * f * (1 + 0.8 * x) / FRECUENCIA;
            double v = std::sin(fase) * std::exp(-4.0 * x) * std::min(1.0, k / 20.0) * amp;
            z.sumar(t + k, v * (1 - pan), v * pan);
        }
    }
}

void normalizar(const Lienzo& z, std::vector<short>& salida, double techo) {
    double pico = 1e-9;
    for (int k = 0; k < N; ++k) pico = std::max(pico, std::max(std::fabs(z.i[(std::size_t)k]), std::fabs(z.d[(std::size_t)k])));
    double g = techo * 32767.0 / pico;
    salida.resize((std::size_t)N * 2);
    for (int k = 0; k < N; ++k) {
        salida[(std::size_t)k * 2] = (short)std::lround(z.i[(std::size_t)k] * g);
        salida[(std::size_t)k * 2 + 1] = (short)std::lround(z.d[(std::size_t)k] * g);
    }
}

// --------------------------------------------------------- reproduccion

void cerrarSalida() {
    if (!g_salida) return;
    waveOutReset(g_salida);
    waveOutUnprepareHeader(g_salida, &g_cabecera, sizeof(g_cabecera));
    waveOutClose(g_salida);
    g_salida = NULL;
}

void cerrarMci() {
    if (!g_mciAbierto) return;
    mciSendStringW(L"close universalis", NULL, 0, NULL);
    g_mciAbierto = false;
}

void aplicarVolumen() {
    int v = g_volumen * g_fundido / 100;
    if (g_salida) {
        DWORD x = (DWORD)(0xFFFF * v / 100);
        waveOutSetVolume(g_salida, x | (x << 16));
    }
    if (g_mciAbierto) {
        wchar_t orden[96];
        swprintf(orden, 96, L"setaudio universalis volume to %d", v * 10);
        mciSendStringW(orden, NULL, 0, NULL);
    }
}

void empezarFundido() {
    g_fundido = 0;
    aplicarVolumen();
    if (g_ventana) SetTimer(g_ventana, TIMER_FUNDIDO, 40, NULL);
}

std::string rutaPreferencias() { return rutas::unir(rutas::directorioDelEjecutable(), "datos/musica.txt"); }

void cargarPreferencias() {
    if (g_preferencias) return;
    g_preferencias = true;
    std::ifstream f(rutaPreferencias().c_str());
    int u = 0, v = 0, version = 1;
    if (f >> u >> v) {
        if (!(f >> version)) version = 1;
        if (version < 2 && u == 7) u = MIA;  // en la version 1 «Mi musica» era el 7
        if (u > NADA && u < N_SONIDOS) g_ultimo = u;
        if (v >= 0 && v <= 100) g_volumen = v;
    }
}

void guardarPreferencias() {
    rutas::crearDirectorios(rutas::padre(rutaPreferencias()));
    std::ofstream f(rutaPreferencias().c_str(), std::ios::trunc);
    f << g_ultimo << " " << g_volumen << " 2\n";
}

void tocarPista();

LRESULT CALLBACK procOculta(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TIMER && wp == TIMER_FUNDIDO) {
        g_fundido = std::min(100, g_fundido + 4);
        aplicarVolumen();
        if (g_fundido >= 100) KillTimer(w, TIMER_FUNDIDO);
        return 0;
    }
    if (msg == MM_MCINOTIFY && wp == MCI_NOTIFY_SUCCESSFUL && g_sonando == MIA) {  // termino la cancion
        ++g_pista;
        tocarPista();
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

void prepararVentana() {
    if (g_ventana) return;
    WNDCLASSW c = WNDCLASSW();
    c.lpfnWndProc = procOculta;
    c.hInstance = GetModuleHandleW(NULL);
    c.lpszClassName = L"UniversalisMusica";
    RegisterClassW(&c);
    g_ventana = CreateWindowExW(0, L"UniversalisMusica", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, c.hInstance, NULL);
}

void leerLista() {
    g_lista.clear();
    std::wstring dir = gui::aW(carpeta());
    for (const wchar_t* patron : {L"*.mp3", L"*.wav", L"*.wma", L"*.m4a", L"*.aac", L"*.mid"}) {
        WIN32_FIND_DATAW d;
        HANDLE h = FindFirstFileW((dir + L"\\" + patron).c_str(), &d);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) g_lista.push_back(dir + L"\\" + d.cFileName);
        } while (FindNextFileW(h, &d));
        FindClose(h);
    }
    std::mt19937 g((unsigned)GetTickCount());
    std::shuffle(g_lista.begin(), g_lista.end(), g);
    g_pista = 0;
}

void tocarPista() {
    cerrarMci();
    if (g_lista.empty()) return;
    for (std::size_t intentos = 0; intentos < g_lista.size(); ++intentos, ++g_pista) {
        const std::wstring& f = g_lista[g_pista % g_lista.size()];
        std::wstring abrir = L"open \"" + f + L"\" type mpegvideo alias universalis";
        if (f.size() > 4 && _wcsicmp(f.c_str() + f.size() - 4, L".mid") == 0)
            abrir = L"open \"" + f + L"\" type sequencer alias universalis";
        if (mciSendStringW(abrir.c_str(), NULL, 0, NULL) != 0) continue;
        g_mciAbierto = true;
        aplicarVolumen();
        if (mciSendStringW(L"play universalis notify", NULL, 0, g_ventana) == 0) return;
        cerrarMci();
    }
}

}  // namespace

const wchar_t* nombre(int s) { return INFO[s >= 0 && s < N_SONIDOS ? s : 0].nombre; }
const wchar_t* descripcion(int s) { return INFO[s >= 0 && s < N_SONIDOS ? s : 0].descripcion; }
COLORREF color(int s) { return INFO[s >= 0 && s < N_SONIDOS ? s : 0].color; }

bool generar(int sonido, std::vector<short>& muestras) {
    Lienzo z;
    switch (sonido) {
        case LLUVIA: lluvia(z); break;
        case OLAS: olas(z); break;
        case MARRON: marron(z); break;
        case CHIMENEA: chimenea(z); break;
        case PIANO: piano(z); break;
        case ALFA: alfa(z); break;
        case TORMENTA: tormenta(z); break;
        case BOSQUE: bosque(z); break;
        case CAFE: cafeteria(z); break;
        case ARROYO: arroyo(z); break;
        default: return false;
    }
    // Mas bien bajo: es un fondo, no tiene que tapar nada (el volumen del panel sube o baja desde aqui)
    normalizar(z, muestras, sonido == PIANO ? 0.36 : 0.28);
    return true;
}

void parar() {
    cerrarSalida();
    cerrarMci();
    g_sonando = NADA;
}

void tocar(int sonido) {
    cargarPreferencias();
    prepararVentana();
    parar();
    if (sonido <= NADA || sonido >= N_SONIDOS) return;
    g_ultimo = sonido;
    guardarPreferencias();
    if (sonido == MIA) {
        leerLista();
        g_sonando = MIA;
        empezarFundido();
        tocarPista();
        return;
    }
    std::vector<short>& m = g_cache[sonido];
    if (m.empty() && !generar(sonido, m)) return;
    WAVEFORMATEX fmt = WAVEFORMATEX();
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = FRECUENCIA;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = FRECUENCIA * 4;
    if (waveOutOpen(&g_salida, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        g_salida = NULL;
        return;
    }
    g_cabecera = WAVEHDR();
    g_cabecera.lpData = (LPSTR)m.data();
    g_cabecera.dwBufferLength = (DWORD)(m.size() * sizeof(short));
    g_cabecera.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
    g_cabecera.dwLoops = 0xFFFFFFFF;
    waveOutPrepareHeader(g_salida, &g_cabecera, sizeof(g_cabecera));
    g_sonando = sonido;
    empezarFundido();
    waveOutWrite(g_salida, &g_cabecera, sizeof(g_cabecera));
}

int sonando() { return g_sonando; }

void ponerVolumen(int v) {
    cargarPreferencias();
    g_volumen = std::max(0, std::min(100, v));
    if (g_fundido == 0 && !g_sonando) g_fundido = 100;
    aplicarVolumen();
    guardarPreferencias();
}

int volumen() {
    cargarPreferencias();
    return g_volumen;
}

int ultimo() {
    cargarPreferencias();
    return g_ultimo;
}

std::string carpeta() {
    std::string c = rutas::unir(rutas::directorioDelEjecutable(), "Musica");
    rutas::crearDirectorios(c);
    return c;
}

int canciones() {
    if (g_sonando != MIA) leerLista();
    return (int)g_lista.size();
}

std::wstring cancion() {
    if (g_sonando != MIA || g_lista.empty()) return L"";
    std::wstring f = g_lista[g_pista % g_lista.size()];
    std::size_t barra = f.find_last_of(L"\\/");
    if (barra != std::wstring::npos) f = f.substr(barra + 1);
    std::size_t punto = f.find_last_of(L'.');
    return punto == std::wstring::npos ? f : f.substr(0, punto);
}

void siguiente() {
    if (g_sonando != MIA) return;
    ++g_pista;
    tocarPista();
}

void generarCampana(int tipo, std::vector<short>& muestras) {
    // Campana: parciales inarmonicos que se apagan antes cuanto mas agudos
    const double parciales[] = {1.0, 2.0, 2.76, 4.07, 5.43};
    const double amplitudes[] = {1.0, 0.55, 0.35, 0.18, 0.08};
    const double caidas[] = {1.6, 2.4, 3.2, 4.5, 6.0};
    // 0: «ding-dong» que baja (a descansar); 1: tres notas que suben (a estudiar)
    std::vector<std::pair<double, double> > notas;  // (segundo, frecuencia)
    if (tipo == 0) notas = {{0.0, 1046.5}, {0.42, 783.99}};
    else notas = {{0.0, 783.99}, {0.22, 1046.5}, {0.44, 1318.5}};
    const int largo = (int)(2.8 * FRECUENCIA);
    std::vector<double> v((std::size_t)largo, 0.0);
    for (const auto& n : notas) {
        int t0 = (int)(n.first * FRECUENCIA);
        for (int h = 0; h < 5; ++h) {
            double f = n.second * parciales[h];
            if (f > FRECUENCIA / 2.2) continue;
            double w = 2 * PI * f / FRECUENCIA, re = 0, im = amplitudes[h], cr = std::cos(w), ci = std::sin(w);
            double caida = std::exp(-caidas[h] / FRECUENCIA);
            for (int k = 0; t0 + k < largo; ++k) {
                double nr = (re * cr - im * ci) * caida;
                im = (re * ci + im * cr) * caida;
                re = nr;
                v[(std::size_t)(t0 + k)] += re * std::min(1.0, k / 40.0);
            }
        }
    }
    double pico = 1e-9;
    for (double x : v) pico = std::max(pico, std::fabs(x));
    muestras.resize(v.size());
    for (std::size_t k = 0; k < v.size(); ++k) {
        double fin = std::min(1.0, (double)(v.size() - k) / (0.2 * FRECUENCIA));  // sin chasquido al acabar
        muestras[k] = (short)std::lround(v[k] / pico * 0.6 * 32767 * fin);
    }
}

void campana(int tipo) {
    static std::vector<char> wav[2];
    tipo = tipo ? 1 : 0;
    if (wav[tipo].empty()) {
        std::vector<short> m;
        generarCampana(tipo, m);
        uint32_t datos = (uint32_t)(m.size() * sizeof(short));
        auto u32 = [&](uint32_t x) { for (int i = 0; i < 4; ++i) wav[tipo].push_back((char)((x >> (8 * i)) & 0xFF)); };
        auto u16 = [&](uint16_t x) { for (int i = 0; i < 2; ++i) wav[tipo].push_back((char)((x >> (8 * i)) & 0xFF)); };
        auto txt = [&](const char* s) { wav[tipo].insert(wav[tipo].end(), s, s + 4); };
        txt("RIFF"); u32(36 + datos); txt("WAVE");
        txt("fmt "); u32(16); u16(1); u16(1); u32(FRECUENCIA); u32(FRECUENCIA * 2); u16(2); u16(16);
        txt("data"); u32(datos);
        const char* p = (const char*)m.data();
        wav[tipo].insert(wav[tipo].end(), p, p + datos);
    }
    PlaySoundW((LPCWSTR)wav[tipo].data(), NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

}  // namespace musica
