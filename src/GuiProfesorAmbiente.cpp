// GuiProfesorAmbiente.cpp - Efectos visuales de la musica de fondo.
//
// Mientras suena un ambiente, el fondo del profesor lo acompaña con un efecto sutil: gotas de
// lluvia que caen, relampagos lejanos con la tormenta, olas en la parte de abajo, polvo que
// flota con el ruido marron, brasas que suben con la chimenea, luces suaves con el piano,
// ondas con las ondas alfa, hojas que caen en el bosque, luces calidas en la cafeteria y
// destellos de agua en el arroyo. Se dibujan detras del contenido, con muy poca opacidad.
//
// No guardan estado: cada particula sale de un numero pseudoaleatorio fijo (su indice) y de la
// hora, asi que el dibujo es el mismo en cada fotograma y no hay nada que actualizar.
// Se pueden quitar en Ajustes > Estudio > «Efectos visuales de la música».
#include "ProfesorUi.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Musica.hpp"

namespace profesor {
namespace ui {

namespace {

const float PI = 3.14159265f;

// Numero fijo en [0, 1) para la particula i y la propiedad k.
float azar(int i, int k) {
    unsigned x = (unsigned)i * 2654435761u ^ ((unsigned)k * 40503u + 0x9E3779B9u);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777216.0f;
}

Gdiplus::Color con(COLORREF c, int alfa) {
    alfa = alfa < 0 ? 0 : alfa > 255 ? 255 : alfa;
    return Gdiplus::Color((BYTE)alfa, GetRValue(c), GetGValue(c), GetBValue(c));
}

float ciclo(float x) { return x - std::floor(x); }

void lluvia(Gdiplus::Graphics& g, float W, float H, float t, float k, int n, float rapidez, COLORREF c, int alfa) {
    for (int i = 0; i < n; ++i) {
        float largo = (14 + 20 * azar(i, 3)) * k;
        float vel = (700 + 500 * azar(i, 2)) * k * rapidez;
        float y = std::fmod(t * vel + azar(i, 4) * (H + 2 * largo), H + 2 * largo) - largo;
        float x = azar(i, 1) * (W + 0.25f * H) - 0.2f * H + y * 0.18f;
        Gdiplus::Pen p(con(c, (int)(alfa * (0.5f + 0.5f * azar(i, 5)))), (0.9f + 0.6f * azar(i, 6)) * k);
        g.DrawLine(&p, x, y, x + largo * 0.18f, y + largo);
    }
}

void bokeh(Gdiplus::Graphics& g, float W, float H, float t, float k, int n, COLORREF c1, COLORREF c2, int alfa, float subida) {
    for (int i = 0; i < n; ++i) {
        float dur = 26 + 16 * azar(i, 2);
        float p = ciclo(t / dur + azar(i, 3));
        float r = (18 + 42 * azar(i, 4)) * k;
        float x = azar(i, 1) * W + std::sin(t * 0.25f + i) * 30 * k;
        float y = subida > 0 ? H * (1.1f - 1.25f * p) : azar(i, 6) * H + std::sin(t * 0.3f + i * 1.7f) * 20 * k;
        if (subida <= 0) x = ciclo(azar(i, 1) + t / (90 + 60 * azar(i, 5))) * (W + 2 * r) - r;
        float a = subida > 0 ? std::sin(PI * p) : 0.6f + 0.4f * std::sin(t * 0.5f + i);
        Gdiplus::SolidBrush b(con(azar(i, 7) < 0.5f ? c1 : c2, (int)(alfa * a)));
        g.FillEllipse(&b, x - r, y - r, 2 * r, 2 * r);
    }
}

}  // namespace

bool hayAmbiente(Estado* s) {
    return gui::estilo().efectosAmbiente && musica::sonando() != musica::NADA && !IsIconic(s->ventana);
}

void pintarAmbiente(Estado* s, HDC dc, RECT r) {
    if (!hayAmbiente(s)) return;
    const gui::Tema& tm = gui::tema();
    bool osc = tm.oscuro;
    float W = (float)(r.right - r.left), H = (float)(r.bottom - r.top), k = (float)s->factor;
    float t = (float)((GetTickCount64() % 86400000ULL) / 1000.0);
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetClip(Gdiplus::Rect(r.left, r.top, r.right - r.left, r.bottom - r.top));
    g.TranslateTransform((float)r.left, (float)r.top);
    const int m = 1 + (osc ? 0 : 1);  // en el tema claro, un poco mas marcado para que se vea

    switch (musica::sonando()) {
        case musica::LLUVIA:
            lluvia(g, W, H, t, k, 70, 1.0f, osc ? RGB(186, 214, 255) : RGB(70, 110, 170), 42 * m);
            break;
        case musica::TORMENTA: {
            lluvia(g, W, H, t, k, 150, 1.35f, osc ? RGB(176, 196, 255) : RGB(70, 90, 160), 48 * m);
            // Relampagos de vez en cuando: un destello doble que se apaga enseguida
            float c = std::fmod(t, 17.0f), luz = 0;
            if (c < 0.12f) luz = 1 - c / 0.12f;
            else if (c > 0.2f && c < 0.45f) luz = 0.7f * (1 - (c - 0.2f) / 0.25f);
            if (luz > 0) {
                Gdiplus::SolidBrush b(con(RGB(220, 228, 255), (int)(38 * luz * m)));
                g.FillRectangle(&b, 0.0f, 0.0f, W, H);
            }
            break;
        }
        case musica::OLAS: {
            // Tres capas de olas abajo que suben y bajan cada 8 s (como el sonido)
            COLORREF c = osc ? RGB(45, 212, 191) : RGB(20, 150, 140);
            float marea = std::sin(2 * PI * t / 8.0f);
            for (int capa = 0; capa < 3; ++capa) {
                float base = H * (0.80f + 0.05f * capa) - marea * (10 + 6 * capa) * k;
                std::vector<Gdiplus::PointF> pts;
                pts.push_back(Gdiplus::PointF(0, H));
                for (float x = 0; x <= W + 24 * k; x += 24 * k) {
                    float y = base + std::sin(x / (180 * k + capa * 60 * k) + t * (0.5f + 0.2f * capa) + capa) * (9 + 4 * capa) * k;
                    pts.push_back(Gdiplus::PointF(x, y));
                }
                pts.push_back(Gdiplus::PointF(W + 24 * k, H));
                Gdiplus::SolidBrush b(con(c, (10 + 4 * capa) * m));
                g.FillPolygon(&b, pts.data(), (INT)pts.size());
            }
            break;
        }
        case musica::MARRON:
            // Motas de polvo que flotan despacio y titilan
            for (int i = 0; i < 55; ++i) {
                float x = ciclo(azar(i, 1) + t / (70 + 50 * azar(i, 2))) * W + std::sin(t * 0.4f + i) * 12 * k;
                float y = ciclo(azar(i, 3) - t / (90 + 60 * azar(i, 4))) * H;
                float rr = (1.2f + 1.8f * azar(i, 5)) * k;
                float a = 0.5f + 0.5f * std::sin(t * (0.8f + azar(i, 6)) + i);
                Gdiplus::SolidBrush b(con(osc ? RGB(253, 200, 140) : RGB(150, 90, 30), (int)((30 + 40 * a) * m)));
                g.FillEllipse(&b, x - rr, y - rr, 2 * rr, 2 * rr);
            }
            break;
        case musica::CHIMENEA: {
            // Resplandor calido abajo que tiembla y brasas que suben
            float temblor = 0.75f + 0.15f * std::sin(t * 3.1f) + 0.1f * std::sin(t * 7.3f + 1);
            Gdiplus::LinearGradientBrush resplandor(Gdiplus::PointF(0, H * 0.62f), Gdiplus::PointF(0, H + 1),
                                                   con(RGB(251, 146, 60), 0), con(RGB(251, 120, 40), (int)(34 * temblor * m)));
            g.FillRectangle(&resplandor, 0.0f, H * 0.62f, W, H * 0.38f + 1);
            for (int i = 0; i < 50; ++i) {
                float vida = 4 + 4 * azar(i, 2);
                float p = ciclo(t / vida + azar(i, 3));
                float x = azar(i, 1) * W + std::sin(t * 1.3f + i) * 18 * k + (azar(i, 4) - 0.5f) * 60 * k * p;
                float y = H - p * H * (0.45f + 0.35f * azar(i, 5));
                float rr = (1.2f + 1.6f * azar(i, 6)) * k * (1 - 0.5f * p);
                float a = (1 - p) * (0.6f + 0.4f * std::sin(t * 9 + i));
                Gdiplus::SolidBrush b(con(azar(i, 7) < 0.5f ? RGB(253, 186, 116) : RGB(250, 204, 21), (int)(150 * a)));
                g.FillEllipse(&b, x - rr, y - rr, 2 * rr, 2 * rr);
            }
            break;
        }
        case musica::PIANO:
            bokeh(g, W, H, t, k, 16, RGB(167, 139, 250), RGB(129, 140, 248), 16 * m, 1);
            break;
        case musica::MIA:
            bokeh(g, W, H, t, k, 16, RGB(52, 211, 153), RGB(45, 212, 191), 15 * m, 1);
            break;
        case musica::CAFE:
            bokeh(g, W, H, t, k, 18, RGB(251, 191, 36), RGB(253, 230, 190), 13 * m, 0);
            break;
        case musica::ALFA: {
            // Ondas que salen despacio del centro
            float R = std::max(W, H) * 0.65f;
            for (int i = 0; i < 5; ++i) {
                float rr = std::fmod(t * 38 * k + i * R / 5, R);
                Gdiplus::Pen p(con(osc ? RGB(244, 114, 182) : RGB(190, 60, 130), (int)(30 * m * (1 - rr / R))), 2 * k);
                g.DrawEllipse(&p, W / 2 - rr, H / 2 - rr, 2 * rr, 2 * rr);
            }
            break;
        }
        case musica::BOSQUE:
            // Hojas que caen girando
            for (int i = 0; i < 24; ++i) {
                float dur = 12 + 9 * azar(i, 2);
                float p = ciclo(t / dur + azar(i, 3));
                float x = azar(i, 1) * W + std::sin(t * 0.8f + i) * 40 * k + p * 90 * k;
                float y = -20 * k + p * (H + 40 * k);
                const COLORREF hojas[] = {RGB(132, 204, 22), RGB(101, 163, 13), RGB(202, 138, 4), RGB(163, 230, 53)};
                Gdiplus::SolidBrush b(con(hojas[i % 4], (int)((55 + 35 * azar(i, 5)) * m)));
                g.TranslateTransform(x, y);
                g.RotateTransform(t * (40 + 80 * azar(i, 4)) + 360 * azar(i, 6));
                g.FillEllipse(&b, -6 * k, -2.6f * k, 12 * k, 5.2f * k);
                g.RotateTransform(-(t * (40 + 80 * azar(i, 4)) + 360 * azar(i, 6)));
                g.TranslateTransform(-x, -y);
            }
            break;
        case musica::ARROYO: {
            // Destellos de agua que corren hacia la derecha y burbujas que suben
            COLORREF c = osc ? RGB(125, 211, 252) : RGB(14, 116, 160);
            for (int i = 0; i < 55; ++i) {
                float largo = (18 + 34 * azar(i, 2)) * k;
                float x = ciclo(azar(i, 1) + t / (8 + 10 * azar(i, 3))) * (W + 2 * largo) - largo;
                float y = H * (0.45f + 0.55f * azar(i, 4)) + std::sin(t * 2 + i) * 3 * k;
                Gdiplus::Pen p(con(c, (int)((22 + 22 * azar(i, 5)) * m)), 1.3f * k);
                g.DrawLine(&p, x, y, x + largo, y);
            }
            for (int i = 0; i < 18; ++i) {
                float p = ciclo(t / (5 + 4 * azar(i, 12)) + azar(i, 13));
                float x = azar(i, 11) * W + std::sin(t * 2 + i) * 6 * k, y = H - p * H * 0.5f;
                float rr = (2 + 3 * azar(i, 14)) * k;
                Gdiplus::Pen pl(con(c, (int)(50 * (1 - p) * m)), 1.0f * k);
                g.DrawEllipse(&pl, x - rr, y - rr, 2 * rr, 2 * rr);
            }
            break;
        }
        default: break;
    }
}

}  // namespace ui
}  // namespace profesor
