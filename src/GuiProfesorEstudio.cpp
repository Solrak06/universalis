// GuiProfesorEstudio.cpp - Herramientas de estudio de la ventana del profesor:
// inicio con el plan del dia, tarjetas de memoria, resumenes por tema, preguntas
// de desarrollo corregidas por la IA y analisis del progreso. Tambien el dibujo
// de iconos y de texto con formato (Markdown ligero) que usa toda la ventana.
//
// La logica vive en profesor/estudio.py; aqui solo se pinta y se piden cosas al
// motor. Las tareas de la IA son lentas (minutos en un portatil): su texto llega
// poco a poco en eventos "parcial" y se dibuja mientras se escribe.
#include "ProfesorUi.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace profesor {
namespace ui {

namespace {

const COLORREF VIOLETA = RGB(167, 139, 250);
const COLORREF TURQUESA = RGB(45, 212, 191);
const COLORREF NARANJA = RGB(251, 146, 60);
const COLORREF ROSA = RGB(244, 114, 182);
const int REPASO_DIAS[] = {0, 1, 3, 7, 16, 35};

std::wstring dias(int n) {
    if (n <= 0) return L"hoy";
    if (n == 1) return L"1 día";
    return std::to_wstring(n) + L" días";
}

// Segundos transcurridos de una tarea de la IA, como "1:05".
std::wstring cronometro(const TareaIA& t) {
    if (!t.inicio) return L"";
    return tiempo((int)((GetTickCount64() - t.inicio) / 1000));
}

void empezarTarea(TareaIA& t, int id) {
    t.id = id;
    t.parcial.clear();
    t.caracteres = 0;
    t.inicio = GetTickCount64();
}

void acabarTarea(TareaIA& t) { t = TareaIA(); }

// Cursor parpadeante al final de un texto que se esta escribiendo.
std::wstring conCursor(Estado* s, const std::wstring& t) { return t + ((s->animacion / 8) % 2 ? L" ▍" : L"  "); }

js::Valor temasElegidos(Estado* s) {
    js::Valor temas = js::Valor::array();
    const js::Valor& archivos = s->hola["archivos"];
    bool todos = true;
    for (std::size_t i = 0; i < archivos.tam() && i < s->temas.size(); ++i) {
        if (s->temas[i]) temas.agregar(archivos[i]["ruta"].comoCadena());
        else todos = false;
    }
    return todos ? js::Valor::array() : temas;
}


// Caja con el texto que la IA esta escribiendo (o un aviso mientras lee el apunte).
int cajaEscribiendo(Estado* s, HDC dc, int x, int y, int w, const TareaIA& t, const std::wstring& queHace,
                    const std::wstring& leyendo, int accionDetener) {
    const gui::Tema& tm = gui::tema();
    int pad = E(s, 22);
    int hTexto = t.parcial.empty() ? E(s, 22) : textoRico(s, dc, conCursor(s, t.parcial), 0, 0, w - 2 * pad, false);
    int alto = pad + E(s, 34) + hTexto + pad;
    RECT r = rect(x, y, w, alto);
    cristal(dc, r, E(s, 16), CLR_INVALID, mezcla(tm.borde, VIOLETA, 0.5));
    spinner(s, dc, x + pad + E(s, 10), y + pad + E(s, 10), E(s, 9));
    std::wstring crono = cronometro(t);
    texto(dc, s->f.negrita, tm.texto, crono.empty() ? queHace : queHace + L"  ·  " + crono,
          rect(x + pad + E(s, 30), y + pad, w - 2 * pad - E(s, 150), E(s, 22)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (accionDetener) boton(s, dc, rect(x + w - pad - E(s, 110), y + pad - E(s, 6), E(s, 110), E(s, 32)), L"Detener",
                             accionDetener, 0, PELIGRO);
    int ty = y + pad + E(s, 34);
    if (t.parcial.empty())
        texto(dc, s->f.texto, tm.textoSuave, leyendo, rect(x + pad, ty, w - 2 * pad, E(s, 22)),
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    else
        textoRico(s, dc, conCursor(s, t.parcial), x + pad, ty, w - 2 * pad);
    return alto;
}

}  // namespace

// Cabecera de pantalla: icono en un cuadrado de color, titulo y subtitulo.
int cabecera(Estado* s, HDC dc, int x, int y, int w, Icono i, COLORREF c, const std::wstring& titulo,
             const std::wstring& sub) {
    const gui::Tema& tm = gui::tema();
    int lado = E(s, 46);
    redondeado(dc, rect(x, y, lado, lado), E(s, 13), mezcla(tm.panel, c, tm.oscuro ? 0.22 : 0.16),
               mezcla(tm.panel, c, 0.45));
    glifo(s, dc, i, x + lado / 2, y + lado / 2, E(s, 24), c);
    int tx = x + lado + E(s, 16), tw = w - lado - E(s, 16);
    texto(dc, s->f.titulo, tm.texto, titulo, rect(tx, y - E(s, 4), tw, E(s, 34)), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    int h = altoTexto(dc, s->f.texto, sub, tw);
    texto(dc, s->f.texto, tm.textoSuave, sub, rect(tx, y + E(s, 30), tw, h));
    return std::max(lado, E(s, 30) + h) + E(s, 24);
}

// ================================================================= iconos

void glifo(Estado* s, HDC dc, Icono i, int cx, int cy, int tam, COLORREF c, const wchar_t* nombre) {
    if (!nombre) {
        switch (i) {
            case I_TEST: nombre = L"test"; break;
            case I_DUDAS: nombre = L"preguntar"; break;
            case I_BOMBILLA: nombre = L"explicar"; break;
            case I_CHISPA: nombre = L"ejemplo"; break;
            case I_TROFEO: nombre = L"trofeo"; break;
            case I_DIANA: nombre = L"objetivo"; break;
            case I_RELOJ: nombre = L"simulacro"; break;
            case I_LIBRO: nombre = L"lector"; break;
            case I_CALENDARIO: nombre = L"calendario"; break;
            case I_AMPLIAR: nombre = L"ampliar"; break;
            default: break;
        }
    }
    if (!nombre || !gui::hayDecoracion(nombre)) {
        icono(s, dc, i == I_AMPLIAR ? I_BUSCAR : i, cx, cy, tam, c);
        return;
    }
    int lado = tam * 108 / 100;  // los macizos llenan menos su caja que los lineales
    gui::decoracion(dc, nombre, rect(cx - lado / 2, cy - lado / 2, lado, lado), c, 255, gui::ICONO_NINGUNO);
}

void icono(Estado*, HDC dc, Icono i, int cx, int cy, int tam, COLORREF c) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float t = (float)tam, x = (float)cx, y = (float)cy;
    float grosor = std::max(1.4f, t / 11.5f);
    Gdiplus::Pen p(gc(c), grosor);
    p.SetLineJoin(Gdiplus::LineJoinRound);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    Gdiplus::SolidBrush relleno(gc(c));
    auto L = [&](float x1, float y1, float x2, float y2) { g.DrawLine(&p, x + x1 * t, y + y1 * t, x + x2 * t, y + y2 * t); };
    auto R = [&](float x1, float y1, float w, float h, float radio) {
        Gdiplus::GraphicsPath camino;
        RECT r = {(LONG)(x + x1 * t), (LONG)(y + y1 * t), (LONG)(x + (x1 + w) * t), (LONG)(y + (y1 + h) * t)};
        caminoRedondeado(camino, r, (int)(radio * t));
        g.DrawPath(&p, &camino);
    };
    auto C = [&](float ox, float oy, float r) { g.DrawEllipse(&p, x + (ox - r) * t, y + (oy - r) * t, 2 * r * t, 2 * r * t); };
    switch (i) {
        case I_INICIO:
            L(-0.44f, -0.02f, 0.0f, -0.40f);
            L(0.0f, -0.40f, 0.44f, -0.02f);
            L(-0.32f, -0.12f, -0.32f, 0.40f);
            L(0.32f, -0.12f, 0.32f, 0.40f);
            L(-0.32f, 0.40f, 0.32f, 0.40f);
            L(-0.08f, 0.40f, -0.08f, 0.14f);
            L(-0.08f, 0.14f, 0.08f, 0.14f);
            L(0.08f, 0.14f, 0.08f, 0.40f);
            break;
        case I_TEST:
            R(-0.34f, -0.40f, 0.68f, 0.82f, 0.12f);
            L(-0.16f, -0.12f, -0.10f, -0.06f);
            L(-0.10f, -0.06f, 0.0f, -0.20f);
            L(0.06f, -0.12f, 0.20f, -0.12f);
            L(-0.16f, 0.16f, 0.20f, 0.16f);
            L(-0.16f, 0.28f, 0.10f, 0.28f);
            break;
        case I_JUGAR: {
            Gdiplus::PointF pts[3] = {{x - 0.22f * t, y - 0.34f * t}, {x + 0.34f * t, y}, {x - 0.22f * t, y + 0.34f * t}};
            g.DrawPolygon(&p, pts, 3);
            break;
        }
        case I_TARJETAS:
            L(-0.18f, -0.42f, 0.36f, -0.42f);
            L(0.36f, -0.42f, 0.36f, 0.22f);
            R(-0.42f, -0.26f, 0.62f, 0.68f, 0.10f);
            break;
        case I_RESUMEN:
            L(-0.30f, -0.42f, 0.12f, -0.42f);
            L(0.12f, -0.42f, 0.32f, -0.22f);
            L(0.32f, -0.22f, 0.32f, 0.42f);
            L(0.32f, 0.42f, -0.30f, 0.42f);
            L(-0.30f, 0.42f, -0.30f, -0.42f);
            L(-0.14f, -0.06f, 0.16f, -0.06f);
            L(-0.14f, 0.10f, 0.16f, 0.10f);
            L(-0.14f, 0.26f, 0.06f, 0.26f);
            break;
        case I_DESARROLLO: {
            Gdiplus::PointF pts[5] = {{x - 0.40f * t, y + 0.40f * t}, {x - 0.30f * t, y + 0.10f * t},
                                      {x + 0.18f * t, y - 0.38f * t}, {x + 0.38f * t, y - 0.18f * t},
                                      {x - 0.10f * t, y + 0.30f * t}};
            g.DrawPolygon(&p, pts, 5);
            L(0.06f, -0.26f, 0.26f, -0.06f);
            break;
        }
        case I_DUDAS:
            R(-0.42f, -0.36f, 0.84f, 0.58f, 0.18f);
            L(-0.20f, 0.22f, -0.26f, 0.42f);
            L(-0.26f, 0.42f, 0.02f, 0.22f);
            g.FillEllipse(&relleno, x - 0.22f * t - grosor / 2, y - 0.07f * t - grosor / 2, grosor * 1.4f, grosor * 1.4f);
            g.FillEllipse(&relleno, x - grosor / 2, y - 0.07f * t - grosor / 2, grosor * 1.4f, grosor * 1.4f);
            g.FillEllipse(&relleno, x + 0.22f * t - grosor / 2, y - 0.07f * t - grosor / 2, grosor * 1.4f, grosor * 1.4f);
            break;
        case I_PROGRESO:
            L(-0.40f, 0.40f, 0.40f, 0.40f);
            L(-0.24f, 0.26f, -0.24f, 0.04f);
            L(0.0f, 0.26f, 0.0f, -0.16f);
            L(0.24f, 0.26f, 0.24f, -0.36f);
            break;
        case I_FALLOS: {
            Gdiplus::PointF pts[5] = {{x - 0.28f * t, y - 0.42f * t}, {x + 0.28f * t, y - 0.42f * t},
                                      {x + 0.28f * t, y + 0.42f * t}, {x, y + 0.18f * t}, {x - 0.28f * t, y + 0.42f * t}};
            g.DrawPolygon(&p, pts, 5);
            break;
        }
        case I_RESULTADOS:
            C(0.0f, -0.10f, 0.26f);
            L(-0.14f, 0.12f, -0.22f, 0.42f);
            L(-0.22f, 0.42f, -0.02f, 0.32f);
            L(0.14f, 0.12f, 0.22f, 0.42f);
            L(0.22f, 0.42f, 0.02f, 0.32f);
            break;
        case I_RELOJ:
            C(0.0f, 0.08f, 0.34f);
            L(0.0f, -0.26f, 0.0f, -0.40f);
            L(-0.12f, -0.42f, 0.12f, -0.42f);
            L(0.0f, 0.08f, 0.14f, -0.08f);
            L(0.30f, -0.24f, 0.38f, -0.32f);
            break;
        case I_BOMBILLA:
            g.DrawArc(&p, x - 0.30f * t, y - 0.44f * t, 0.60f * t, 0.60f * t, 135, 270);
            L(-0.21f, -0.02f, -0.12f, 0.18f);
            L(0.21f, -0.02f, 0.12f, 0.18f);
            L(-0.12f, 0.18f, 0.12f, 0.18f);
            L(-0.12f, 0.30f, 0.12f, 0.30f);
            L(-0.06f, 0.42f, 0.06f, 0.42f);
            break;
        case I_COPIAR:
            R(-0.16f, -0.40f, 0.56f, 0.62f, 0.10f);
            L(-0.40f, -0.16f, -0.40f, 0.38f);
            L(-0.40f, 0.38f, 0.12f, 0.38f);
            break;
        case I_CHISPA: {
            Gdiplus::GraphicsPath camino;
            Gdiplus::PointF pts[8];
            for (int k = 0; k < 8; ++k) {
                double ang = k * 3.14159265 / 4 - 3.14159265 / 2;
                float r = (k % 2 == 0) ? 0.46f : 0.13f;
                pts[k] = Gdiplus::PointF(x + (float)std::cos(ang) * r * t, y + (float)std::sin(ang) * r * t);
            }
            camino.AddPolygon(pts, 8);
            g.FillPath(&relleno, &camino);
            break;
        }
        case I_LIBRO:
            L(0.0f, -0.28f, 0.0f, 0.38f);
            L(0.0f, -0.28f, -0.42f, -0.36f);
            L(-0.42f, -0.36f, -0.42f, 0.30f);
            L(-0.42f, 0.30f, 0.0f, 0.38f);
            L(0.0f, -0.28f, 0.42f, -0.36f);
            L(0.42f, -0.36f, 0.42f, 0.30f);
            L(0.42f, 0.30f, 0.0f, 0.38f);
            break;
        case I_FLECHA:
            L(-0.36f, 0.0f, 0.34f, 0.0f);
            L(0.10f, -0.24f, 0.34f, 0.0f);
            L(0.10f, 0.24f, 0.34f, 0.0f);
            break;
        case I_REPETIR:
            g.DrawArc(&p, x - 0.36f * t, y - 0.36f * t, 0.72f * t, 0.72f * t, -60, 300);
            L(0.18f, -0.31f, 0.36f, -0.36f);
            L(0.18f, -0.31f, 0.24f, -0.12f);
            break;
        case I_CARPETA:
            L(-0.42f, -0.30f, -0.12f, -0.30f);
            L(-0.12f, -0.30f, 0.0f, -0.18f);
            L(0.0f, -0.18f, 0.42f, -0.18f);
            L(0.42f, -0.18f, 0.42f, 0.34f);
            L(0.42f, 0.34f, -0.42f, 0.34f);
            L(-0.42f, 0.34f, -0.42f, -0.30f);
            break;
        case I_CHECK:
            L(-0.34f, 0.02f, -0.10f, 0.28f);
            L(-0.10f, 0.28f, 0.36f, -0.26f);
            break;
        case I_DIANA:
            C(0.0f, 0.0f, 0.40f);
            C(0.0f, 0.0f, 0.22f);
            g.FillEllipse(&relleno, x - 0.07f * t, y - 0.07f * t, 0.14f * t, 0.14f * t);
            break;
        case I_AMPLIAR:  // sin su dibujo, la lupa
        case I_BUSCAR:
            C(-0.08f, -0.08f, 0.28f);
            L(0.13f, 0.13f, 0.40f, 0.40f);
            break;
        case I_MAPA:
            C(0.0f, 0.0f, 0.12f);
            C(-0.32f, -0.30f, 0.09f);
            C(0.32f, -0.30f, 0.09f);
            C(-0.32f, 0.32f, 0.09f);
            C(0.32f, 0.32f, 0.09f);
            L(-0.09f, -0.08f, -0.25f, -0.23f);
            L(0.09f, -0.08f, 0.25f, -0.23f);
            L(-0.09f, 0.08f, -0.25f, 0.25f);
            L(0.09f, 0.08f, 0.25f, 0.25f);
            break;
        case I_TROFEO:
            g.DrawArc(&p, x - 0.26f * t, y - 0.62f * t, 0.52f * t, 0.70f * t, 0, 180);
            L(-0.26f, -0.27f, -0.26f, -0.40f);
            L(0.26f, -0.27f, 0.26f, -0.40f);
            L(-0.26f, -0.40f, 0.26f, -0.40f);
            g.DrawArc(&p, x - 0.44f * t, y - 0.38f * t, 0.24f * t, 0.24f * t, 90, 180);
            g.DrawArc(&p, x + 0.20f * t, y - 0.38f * t, 0.24f * t, 0.24f * t, 270, 180);
            L(0.0f, 0.08f, 0.0f, 0.26f);
            L(-0.20f, 0.40f, 0.20f, 0.40f);
            L(-0.12f, 0.26f, 0.12f, 0.26f);
            break;
        case I_EXPORTAR:
            L(-0.38f, 0.10f, -0.38f, 0.40f);
            L(-0.38f, 0.40f, 0.38f, 0.40f);
            L(0.38f, 0.40f, 0.38f, 0.10f);
            L(0.0f, 0.18f, 0.0f, -0.40f);
            L(-0.18f, -0.22f, 0.0f, -0.40f);
            L(0.18f, -0.22f, 0.0f, -0.40f);
            break;
        case I_CALENDARIO:
            R(-0.40f, -0.32f, 0.80f, 0.74f, 0.10f);
            L(-0.40f, -0.08f, 0.40f, -0.08f);
            L(-0.18f, -0.44f, -0.18f, -0.22f);
            L(0.18f, -0.44f, 0.18f, -0.22f);
            g.FillEllipse(&relleno, x - 0.20f * t, y + 0.08f * t, 0.12f * t, 0.12f * t);
            break;
        case I_FUEGO: {
            Gdiplus::GraphicsPath camino;
            camino.AddBezier(x, y + 0.42f * t, x - 0.40f * t, y + 0.36f * t, x - 0.34f * t, y - 0.05f * t, x - 0.10f * t, y - 0.20f * t);
            camino.AddBezier(x - 0.10f * t, y - 0.20f * t, x - 0.02f * t, y - 0.30f * t, x + 0.02f * t, y - 0.38f * t, x + 0.02f * t, y - 0.46f * t);
            camino.AddBezier(x + 0.02f * t, y - 0.46f * t, x + 0.36f * t, y - 0.24f * t, x + 0.44f * t, y + 0.30f * t, x, y + 0.42f * t);
            camino.CloseFigure();
            g.DrawPath(&p, &camino);
            break;
        }
        case I_PAUSA:
            L(-0.14f, -0.30f, -0.14f, 0.30f);
            L(0.14f, -0.30f, 0.14f, 0.30f);
            break;
    }
}

void botonIcono(Estado* s, HDC dc, RECT r, Icono i, const std::wstring& t, int accion, int dato, EstiloBoton estilo,
                bool activo) {
    const gui::Tema& tm = gui::tema();
    boton(s, dc, r, L"", accion, dato, estilo, activo);
    int lado = E(s, 16), hueco = E(s, 8);
    int tw = anchoTexto(dc, s->f.negrita, t);
    int total = lado + hueco + tw;
    int x = std::max((int)r.left + E(s, 12), (int)(r.left + r.right - total) / 2);
    COLORREF tinta = estilo == PRIMARIO ? RGB(255, 255, 255) : estilo == PELIGRO ? tm.mal
                   : (estilo == FANTASMA && !encima(s, accion, dato)) ? tm.textoSuave : tm.texto;
    if (!activo) tinta = mezcla(tinta, tm.fondo, 0.5);
    icono(s, dc, i, x + lado / 2, (r.top + r.bottom) / 2, lado, tinta);
    texto(dc, s->f.negrita, tinta, t, rect(x + lado + hueco, r.top, r.right - x - lado - hueco - E(s, 8), r.bottom - r.top),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void retratoEn(Estado* s, HDC dc, RECT r, int radio) {
    if (!s->retrato || s->retrato->GetLastStatus() != Gdiplus::Ok || s->retrato->GetHeight() == 0) return;
    int alto = r.bottom - r.top;
    int ancho = (int)(alto * (double)s->retrato->GetWidth() / s->retrato->GetHeight());
    if (!s->retratoCache || s->retratoAncho != ancho || s->retratoAlto != alto) {
        delete s->retratoCache;
        s->retratoCache = new Gdiplus::Bitmap(std::max(1, ancho), std::max(1, alto), PixelFormat32bppPARGB);
        Gdiplus::Graphics gb(s->retratoCache);
        gb.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        gb.DrawImage(s->retrato, 0, 0, ancho, alto);
        s->retratoAncho = ancho;
        s->retratoAlto = alto;
    }
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath camino;
    caminoRedondeado(camino, r, radio);
    g.SetClip(&camino);
    g.DrawImage(s->retratoCache, (int)(r.left + r.right - ancho) / 2, (int)r.top, ancho, alto);
}

// ======================================================= texto con formato

namespace {

struct Pieza {
    std::wstring t;
    bool negrita;
    bool cita;
};

// Trocea una linea en palabras con su estilo: **negrita** y citas [1].
std::vector<Pieza> piezas(const std::wstring& linea) {
    std::vector<Pieza> salida;
    bool negrita = false;
    std::wstring actual;
    auto cerrar = [&](bool cita = false) {
        if (!actual.empty()) salida.push_back({actual, negrita, cita});
        actual.clear();
    };
    for (std::size_t i = 0; i < linea.size(); ++i) {
        wchar_t c = linea[i];
        if (c == L'*' && i + 1 < linea.size() && linea[i + 1] == L'*') {
            cerrar();
            negrita = !negrita;
            ++i;
            continue;
        }
        if (c == L'[' && i + 2 < linea.size() && iswdigit(linea[i + 1])) {
            std::size_t fin = linea.find(L']', i);
            bool soloCifras = fin != std::wstring::npos;
            for (std::size_t k = i + 1; soloCifras && k < fin; ++k) soloCifras = iswdigit(linea[k]) != 0;
            if (soloCifras && fin - i <= 4) {  // [1], [12]: cita; [0,1] es un intervalo
                cerrar();
                actual = linea.substr(i, fin - i + 1);
                cerrar(true);
                i = fin;
                continue;
            }
        }
        actual += c;
        if (c == L' ') cerrar();  // cada palabra con su espacio detras
    }
    cerrar();
    return salida;
}

// Parrafo con ajuste de linea manual (las palabras pueden tener estilos distintos).
int parrafo(Estado* s, HDC dc, const std::wstring& t, int x, int y, int w, bool dibujar, COLORREF tinta, int altoLinea) {
    const gui::Tema& tm = gui::tema();
    std::vector<Pieza> ps = piezas(t);
    int cx = x, cy = y;
    bool algo = false;
    for (const Pieza& p : ps) {
        HFONT f = p.negrita ? s->f.negrita : s->f.texto;
        std::wstring sinEspacio = p.t;
        while (!sinEspacio.empty() && sinEspacio.back() == L' ') sinEspacio.pop_back();
        int ancho = anchoTexto(dc, f, sinEspacio);
        if (cx > x && cx + ancho > x + w) {
            cx = x;
            cy += altoLinea;
            if (sinEspacio.empty()) continue;
        }
        if (dibujar) {
            SelectObject(dc, f);
            SetTextColor(dc, p.cita ? tm.acento : p.negrita ? tm.texto : tinta);
            SetBkMode(dc, TRANSPARENT);
            if (ancho > w) {
                RECT r = rect(cx, cy, x + w - cx, altoLinea);
                DrawTextW(dc, sinEspacio.c_str(), (int)sinEspacio.size(), &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            } else {
                TextOutW(dc, cx, cy, p.t.c_str(), (int)p.t.size());
            }
        }
        cx += anchoTexto(dc, f, p.t);
        algo = true;
    }
    return algo ? cy - y + altoLinea : 0;
}

}  // namespace

int textoRico(Estado* s, HDC dc, const std::wstring& md, int x, int y, int w, bool dibujar, COLORREF tinta) {
    const gui::Tema& tm = gui::tema();
    if (tinta == CLR_INVALID) tinta = tm.texto;
    SelectObject(dc, s->f.texto);
    TEXTMETRICW m;
    GetTextMetricsW(dc, &m);
    int altoLinea = m.tmHeight + E(s, 4);
    int y0 = y;
    bool primera = true, hueco = false;
    std::size_t inicio = 0;
    while (inicio <= md.size()) {
        std::size_t fin = md.find(L'\n', inicio);
        if (fin == std::wstring::npos) fin = md.size();
        std::wstring linea = md.substr(inicio, fin - inicio);
        inicio = fin + 1;
        while (!linea.empty() && (linea.back() == L' ' || linea.back() == L'\r')) linea.pop_back();
        if (linea.empty()) {
            if (!primera && !hueco) y += E(s, 8);
            hueco = true;
            if (fin == md.size()) break;
            continue;
        }
        hueco = false;
        if (linea.compare(0, 3, L"## ") == 0) {
            if (!primera) y += E(s, 10);
            std::wstring t = linea.substr(3);
            int h = altoTexto(dc, s->f.subtitulo, t, w);
            if (dibujar) texto(dc, s->f.subtitulo, tm.texto, t, rect(x, y, w, h));
            y += h + E(s, 6);
            primera = false;
            if (fin == md.size()) break;
            continue;
        }
        int sangria = 0;
        std::wstring cuerpo = linea;
        if (linea.compare(0, 4, L"  - ") == 0) {
            sangria = E(s, 40);
            cuerpo = linea.substr(4);
            if (dibujar) circulo(dc, x + E(s, 27), y + altoLinea / 2 - E(s, 1), E(s, 3), tm.panel, mezcla(tinta, tm.panel, 0.3), 1.2f);
        } else if (linea.compare(0, 2, L"- ") == 0) {
            sangria = E(s, 20);
            cuerpo = linea.substr(2);
            if (dibujar) circulo(dc, x + E(s, 7), y + altoLinea / 2 - E(s, 1), E(s, 3), tm.acento);
        } else {
            std::size_t k = 0;
            while (k < linea.size() && k < 3 && iswdigit(linea[k])) ++k;
            if (k > 0 && k + 1 < linea.size() && linea[k] == L'.' && linea[k + 1] == L' ') {
                sangria = E(s, 26);
                cuerpo = linea.substr(k + 2);
                if (dibujar)
                    texto(dc, s->f.negrita, tm.acento, linea.substr(0, k + 1), rect(x, y, sangria, altoLinea),
                          DT_LEFT | DT_SINGLELINE);
            }
        }
        y += parrafo(s, dc, cuerpo, x + sangria, y, w - sangria, dibujar, tinta, altoLinea) + E(s, 4);
        primera = false;
        if (fin == md.size()) break;
    }
    return y - y0;
}

std::wstring sinFormato(const std::wstring& md) {
    std::wstring salida;
    std::size_t inicio = 0;
    while (inicio <= md.size()) {
        std::size_t fin = md.find(L'\n', inicio);
        if (fin == std::wstring::npos) fin = md.size();
        std::wstring linea = md.substr(inicio, fin - inicio);
        if (linea.compare(0, 3, L"## ") == 0) linea = linea.substr(3);
        else if (linea.compare(0, 2, L"- ") == 0) linea = L"• " + linea.substr(2);
        else if (linea.compare(0, 4, L"  - ") == 0) linea = L"    ◦ " + linea.substr(4);
        std::wstring limpia;
        for (std::size_t i = 0; i < linea.size(); ++i) {
            if (linea[i] == L'*' && i + 1 < linea.size() && linea[i + 1] == L'*') {
                ++i;
                continue;
            }
            limpia += linea[i];
        }
        salida += limpia;
        if (fin == md.size()) break;
        salida += L"\r\n";
        inicio = fin + 1;
    }
    return salida;
}

bool copiar(HWND ventana, const std::wstring& t) {
    if (!OpenClipboard(ventana)) return false;
    EmptyClipboard();
    HGLOBAL memoria = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
    bool ok = false;
    if (memoria) {
        void* destino = GlobalLock(memoria);
        if (destino) {
            memcpy(destino, t.c_str(), (t.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(memoria);
            ok = SetClipboardData(CF_UNICODETEXT, memoria) != NULL;
        }
        if (!ok) GlobalFree(memoria);
    }
    CloseClipboard();
    return ok;
}

// ================================================================== inicio

namespace {

std::wstring saludo() {
    time_t ahora = time(NULL);
    struct tm local;
    localtime_s(&local, &ahora);
    if (local.tm_hour < 6) return L"Estudiando a deshoras";
    if (local.tm_hour < 14) return L"Buenos días";
    if (local.tm_hour < 21) return L"Buenas tardes";
    return L"Buenas noches";
}

Icono iconoDeTarea(const std::string& tipo) {
    if (tipo == "tarjetas") return I_TARJETAS;
    if (tipo == "repaso") return I_REPETIR;
    if (tipo == "tema") return I_DIANA;
    if (tipo == "resumen") return I_RESUMEN;
    if (tipo == "simulacro") return I_RELOJ;
    if (tipo == "desarrollo") return I_DESARROLLO;
    return I_TEST;
}

COLORREF colorDeTarea(const std::string& tipo) {
    const gui::Tema& tm = gui::tema();
    if (tipo == "tarjetas") return NARANJA;
    if (tipo == "repaso") return tm.medio;
    if (tipo == "tema") return tm.mal;
    if (tipo == "resumen") return TURQUESA;
    if (tipo == "simulacro") return ROSA;
    if (tipo == "desarrollo") return VIOLETA;
    return tm.acento;
}

}  // namespace

int pintarHub(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& ini = s->inicio;
    const js::Valor& res = s->hola["resumen"];
    const js::Valor& examen = ini["examen"];

    // --- Portada: el profesor, el proximo examen y la cuenta atras
    int altoHero = E(s, 236);
    RECT hero = rect(x, y, w, altoHero);
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath camino;
        caminoRedondeado(camino, hero, E(s, 22));
        Gdiplus::LinearGradientBrush fondo(Gdiplus::PointF((float)hero.left, (float)hero.top),
                                           Gdiplus::PointF((float)hero.right, (float)hero.bottom),
                                           gc(mezcla(tm.panel, tm.acento, tm.oscuro ? 0.34 : 0.22)),
                                           gc(mezcla(tm.panel, VIOLETA, tm.oscuro ? 0.20 : 0.14)));
        g.FillPath(&fondo, &camino);
        Gdiplus::Pen borde(gc(mezcla(tm.borde, tm.acento, 0.45)), 1.0f);
        g.DrawPath(&borde, &camino);
    }
    int anchoRetrato = std::min(E(s, 210), w / 4);
    retratoEn(s, dc, rect(x + E(s, 8), y + E(s, 8), anchoRetrato, altoHero - E(s, 8)), E(s, 18));
    int anillo = E(s, 64);
    bool conAnillo = w >= E(s, 760);
    int tx = x + anchoRetrato + E(s, 28);
    int tw = w - (tx - x) - (conAnillo ? 2 * anillo + E(s, 70) : E(s, 24));
    int ty = y + E(s, 34);
    texto(dc, s->f.texto, tm.textoSuave, saludo() + L"  ·  " + W(s->asignatura.nombre), rect(tx, ty, tw, E(s, 22)),
          DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    ty += E(s, 28);
    std::wstring titular, detalle;
    int diasExamen = examen.esNulo() ? -1 : examen["dias"].comoEntero();
    COLORREF urgencia = diasExamen < 0 ? tm.ok : diasExamen <= 3 ? tm.mal : diasExamen <= 10 ? tm.medio : tm.ok;
    if (diasExamen >= 0) {
        std::wstring nombre = W(examen["nombre"]);
        titular = diasExamen == 0 ? nombre + L" es hoy" : diasExamen == 1 ? nombre + L" es mañana"
                                                                          : nombre + L" es en " + dias(diasExamen);
        wchar_t b[160];
        swprintf(b, 160, L"%d %% de la nota  ·  %ls", (int)std::lround(examen["peso"].comoNumero()),
                 diasExamen <= 3 ? L"es el momento de los simulacros y de repasar fallos."
                                 : L"un poco cada día rinde más que un atracón al final.");
        detalle = b;
    } else {
        titular = L"Sin pruebas a la vista";
        detalle = L"Buen momento para adelantar temario y afianzar lo que ya sabes.";
    }
    texto(dc, s->f.titulo, tm.texto, titular, rect(tx, ty, tw, E(s, 36)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    ty += E(s, 42);
    int hd = std::min(altoTexto(dc, s->f.texto, detalle, tw), E(s, 44));
    texto(dc, s->f.texto, tm.textoSuave, detalle, rect(tx, ty, tw, hd), DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    ty = y + altoHero - E(s, 64);
    // Cifras rapidas
    {
        int prec = res["precision"].comoEntero(-1), racha = res["racha"].comoEntero();
        const js::Valor& pred = ini["prediccion"];
        const js::Valor& nivel = ini["nivel"];
        struct Dato {
            std::wstring valor, nombre;
            COLORREF color;
        };
        std::vector<Dato> datos = {
            {prec < 0 ? L"—" : std::to_wstring(prec) + L" %", L"aciertos",
             prec < 0 ? tm.textoSuave : prec >= 70 ? tm.ok : prec >= 50 ? tm.medio : tm.mal},
            {std::to_wstring(racha), racha == 1 ? L"día de racha" : L"días de racha", racha ? tm.ok : tm.textoSuave}};
        if (!pred.esNulo())
            datos.push_back({numero(pred["nota"].comoNumero()), L"nota estimada", gui::colorDeNota(pred["nota"].comoNumero())});
        else
            datos.push_back({std::to_wstring(res["tests"].comoEntero()), L"tests hechos", tm.texto});
        if (!nivel.esNulo()) datos.push_back({std::to_wstring(nivel["nivel"].comoEntero()), L"nivel", RGB(250, 204, 21)});
        // Preguntas de IA ya preparadas en segundo plano: el siguiente test empieza con ellas
        int listas = s->hola["resumen"]["ia_nuevas"].comoEntero();
        if (listas > 0)
            datos.push_back({std::to_wstring(listas), listas == 1 ? L"pregunta de IA lista" : L"preguntas de IA listas",
                             RGB(167, 139, 250)});
        int px = tx;
        for (const Dato& d : datos) {
            int vw = anchoTexto(dc, s->f.subtitulo, d.valor), nw = anchoTexto(dc, s->f.peque, d.nombre);
            int cw = vw + nw + E(s, 34);
            if (px + cw > tx + tw) break;
            RECT r = rect(px, ty, cw, E(s, 38));
            redondeado(dc, r, E(s, 12), mezcla(tm.panel, tm.fondo, 0.35), mezcla(tm.borde, tm.fondo, 0.3));
            texto(dc, s->f.subtitulo, d.color, d.valor, rect(px + E(s, 12), ty, vw + E(s, 4), E(s, 38)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            texto(dc, s->f.peque, tm.textoSuave, d.nombre, rect(px + E(s, 20) + vw, ty, nw + E(s, 6), E(s, 38)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if (std::wstring(d.nombre) == L"nivel") {
                // Progreso hasta el siguiente nivel, como una linea fina bajo la cifra
                int base = nivel["xp_nivel"].comoEntero(), sig = nivel["xp_siguiente"].comoEntero();
                double fr = (double)(nivel["xp"].comoEntero() - base) / std::max(1, sig - base);
                barra(dc, rect(px + E(s, 12), ty + E(s, 31), cw - E(s, 24), E(s, 3)), fr, RGB(250, 204, 21));
            }
            if (std::wstring(d.nombre) == L"nota estimada") zona(s, rect(px, ty, cw, E(s, 38)), A_PESTANA, V_PROGRESO);
            if (d.nombre.find(L"de IA lista") != std::wstring::npos) zona(s, rect(px, ty, cw, E(s, 38)), A_PESTANA, V_INICIO);
            px += cw + E(s, 10);
        }
    }
    if (conAnillo) {
        int cx = x + w - anillo - E(s, 40), cy = y + altoHero / 2;
        arco(dc, cx, cy, anillo, (float)E(s, 10), 0, 360, mezcla(tm.panel, tm.fondo, 0.4));
        std::wstring numeroGrande, pie;
        if (diasExamen >= 0) {
            double fr = 1.0 - std::min(1.0, diasExamen / 30.0);
            arco(dc, cx, cy, anillo, (float)E(s, 10), -90, (float)(360.0 * std::max(0.04, fr)), urgencia);
            numeroGrande = std::to_wstring(diasExamen);
            pie = diasExamen == 1 ? L"día" : L"días";
        } else {
            int prec = res["precision"].comoEntero(-1);
            if (prec > 0) arco(dc, cx, cy, anillo, (float)E(s, 10), -90, (float)(3.6 * prec), urgencia);
            numeroGrande = prec < 0 ? L"—" : std::to_wstring(prec) + L"\u00A0%";
            pie = L"aciertos";
        }
        texto(dc, s->f.grande, tm.texto, numeroGrande, rect(cx - anillo, cy - E(s, 34), 2 * anillo, E(s, 50)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.peque, tm.textoSuave, pie, rect(cx - anillo, cy + E(s, 16), 2 * anillo, E(s, 20)),
              DT_CENTER | DT_SINGLELINE);
    }
    y = hero.bottom + E(s, 22);

    // --- Test a medias
    const js::Valor& pend = s->hola["pendiente"];
    if (!pend.esNulo()) {
        RECT r = rect(x, y, w, E(s, 64));
        redondeado(dc, r, E(s, 14), mezcla(tm.panel, tm.acento, 0.12), mezcla(tm.borde, tm.acento, 0.6));
        icono(s, dc, I_JUGAR, x + E(s, 30), y + E(s, 32), E(s, 20), tm.acento);
        wchar_t b[160];
        swprintf(b, 160, L"Tienes un test a medias: %d de %d respondidas", pend["respondidas"].comoEntero(),
                 pend["n"].comoEntero());
        texto(dc, s->f.negrita, tm.texto, b, rect(x + E(s, 56), y, w - E(s, 240), E(s, 64)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        boton(s, dc, rect(r.right - E(s, 160), y + E(s, 13), E(s, 144), E(s, 38)), L"Continuar", A_CONTINUAR, 0, PRIMARIO);
        y += E(s, 64) + E(s, 20);
    }

    // --- Documento del temario a medio leer
    {
        const js::Valor& docs = s->temario["temas"];
        std::string ultimo = s->temario["ultimo"].comoCadena();
        for (std::size_t i = 0; i < docs.tam() && !ultimo.empty(); ++i) {
            const js::Valor& d = docs[i];
            double pos = d["posicion"].comoNumero();
            if (d["ruta"].comoCadena() != ultimo || d["leido"].comoBool() || pos <= 0.02) continue;
            const COLORREF azul = RGB(96, 165, 250);
            RECT r = rect(x, y, w, E(s, 64));
            redondeado(dc, r, E(s, 14), mezcla(tm.panel, azul, 0.10), mezcla(tm.borde, azul, 0.5));
            // Anillo con lo que llevas
            int cx = x + E(s, 32), cy = y + E(s, 32);
            arco(dc, cx, cy, E(s, 15), (float)E(s, 3), 0, 360, mezcla(tm.panel, azul, 0.25));
            arco(dc, cx, cy, E(s, 15), (float)E(s, 3), -90, (float)(360.0 * pos), azul);
            icono(s, dc, I_LIBRO, cx, cy, E(s, 14), azul);
            texto(dc, s->f.negrita, tm.texto, L"Seguir leyendo «" + W(d["tema"]) + L"»", rect(x + E(s, 62), y + E(s, 12), w - E(s, 250), E(s, 22)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            texto(dc, s->f.peque, tm.textoSuave,
                  L"Vas por el " + std::to_wstring((int)(pos * 100 + 0.5)) + L" %  ·  " + W(d["grupo"]),
                  rect(x + E(s, 62), y + E(s, 34), w - E(s, 250), E(s, 20)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            boton(s, dc, rect(r.right - E(s, 160), y + E(s, 13), E(s, 144), E(s, 38)), L"Seguir", A_L_SEGUIR, 0, SECUNDARIO);
            y += E(s, 64) + E(s, 20);
            break;
        }
    }

    // --- Plan de hoy
    const js::Valor& plan = ini["plan"];
    if (plan.tam() > 0) {
        int hechas = 0;
        for (std::size_t i = 0; i < plan.tam(); ++i) hechas += plan[i]["hecha"].comoBool() ? 1 : 0;
        int filaAlto = E(s, 66);
        int alto = E(s, 76) + (int)plan.tam() * filaAlto + E(s, 10);
        RECT caja = rect(x, y, w, alto);
        cristal(dc, caja, E(s, 18));
        int pad = E(s, 22);
        texto(dc, s->f.subtitulo, tm.texto, L"Tu plan para hoy", rect(x + pad, y + E(s, 18), w / 2, E(s, 26)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.peque, tm.textoSuave, L"Según tu calendario, tus tarjetas, tus fallos y tus temas flojos.",
              rect(x + pad, y + E(s, 44), w - 2 * pad - E(s, 140), E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        // Progreso del plan
        {
            int cx = x + w - pad - E(s, 22), cy = y + E(s, 38), radio = E(s, 18);
            arco(dc, cx, cy, radio, (float)E(s, 5), 0, 360, tm.panelAlt);
            if (hechas) arco(dc, cx, cy, radio, (float)E(s, 5), -90, 360.0f * hechas / (float)plan.tam(), tm.ok);
            texto(dc, s->f.mini, tm.texto, std::to_wstring(hechas) + L"/" + std::to_wstring(plan.tam()),
                  rect(cx - radio, cy - radio, 2 * radio, 2 * radio), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (hechas == (int)plan.tam())
                texto(dc, s->f.negrita, tm.ok, L"¡Plan completado!", rect(cx - radio - E(s, 170), cy - E(s, 11), E(s, 160), E(s, 22)),
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        int fy = y + E(s, 76);
        for (std::size_t i = 0; i < plan.tam(); ++i) {
            const js::Valor& t = plan[i];
            bool hecha = t["hecha"].comoBool();
            std::string tipo = t["tipo"].comoCadena();
            COLORREF c = colorDeTarea(tipo);
            RECT fila = rect(x + E(s, 10), fy, w - E(s, 20), filaAlto - E(s, 6));
            bool h = encima(s, A_PLAN, (int)i);
            if (h) redondeado(dc, fila, E(s, 12), tm.panelAlt);
            int cx = fila.left + E(s, 26), cy = fila.top + (fila.bottom - fila.top) / 2;
            if (hecha) marca(dc, cx, cy, E(s, 12), true);
            else circulo(dc, cx, cy, E(s, 11), tm.panel, mezcla(tm.borde, c, 0.6), 2.0f);
            int ix = cx + E(s, 30);
            redondeado(dc, rect(ix, cy - E(s, 18), E(s, 36), E(s, 36)), E(s, 10), mezcla(tm.panel, c, tm.oscuro ? 0.22 : 0.15));
            glifo(s, dc, iconoDeTarea(tipo), ix + E(s, 18), cy, E(s, 18), c);
            int lx = ix + E(s, 50), lw = fila.right - lx - E(s, 150);
            texto(dc, s->f.negrita, hecha ? tm.textoSuave : tm.texto, W(t["titulo"]), rect(lx, cy - E(s, 21), lw, E(s, 22)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            texto(dc, s->f.peque, tm.textoSuave, W(t["detalle"]), rect(lx, cy + E(s, 1), lw, E(s, 20)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            std::wstring accion = hecha ? L"Hecho" : L"Empezar";
            RECT b = rect(fila.right - E(s, 130), cy - E(s, 17), E(s, 118), E(s, 34));
            if (hecha) {
                texto(dc, s->f.negrita, tm.ok, L"✓ Hecho", b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                redondeado(dc, b, E(s, 10), h ? tm.acento : mezcla(tm.panel, tm.acento, 0.16),
                           h ? tm.acento : mezcla(tm.borde, tm.acento, 0.5));
                texto(dc, s->f.negrita, h ? RGB(255, 255, 255) : tm.texto, L"Empezar  →", b,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            zona(s, fila, A_PLAN, (int)i);
            fy += filaAlto;
        }
        y = caja.bottom + E(s, 26);
    }

    // --- Camino al examen
    if (int h = pintarCalendario(s, dc, x, y, w)) y += h + E(s, 10);
    const js::Valor& pred = ini["prediccion"];
    if (!pred.esNulo()) {
        RECT r = rect(x, y, w, E(s, 70));
        COLORREF c = gui::colorDeNota(pred["nota"].comoNumero());
        redondeado(dc, r, E(s, 14), mezcla(tm.panel, c, tm.oscuro ? 0.10 : 0.07), mezcla(tm.borde, c, 0.5));
        texto(dc, s->f.titulo, c, numero(pred["nota"].comoNumero()), rect(x + E(s, 18), y, E(s, 70), E(s, 70)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.negrita, tm.texto, L"Nota estimada si el examen fuera hoy",
              rect(x + E(s, 100), y + E(s, 12), w - E(s, 120), E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.peque, tm.textoSuave,
              W(pred["consejo"]) + L"  ·  basada en tus aciertos de cada tema (" + std::to_wstring(pred["cubierto"].comoEntero()) +
                  L" % del temario practicado)",
              rect(x + E(s, 100), y + E(s, 38), w - E(s, 120), E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y = r.bottom + E(s, 26);
    }

    // --- Herramientas
    tituloSeccion(s, dc, x, y, w, L"Herramientas de estudio", L"Todo sale de los apuntes de tu carpeta Temario.");
    const js::Valor& her = ini["herramientas"];
    int pendientes = res["tarjetas_pendientes"].comoEntero(), nuevas = res["tarjetas_nuevas"].comoEntero();
    int prec = res["precision"].comoEntero(-1);
    wchar_t b1[96], b2[96], b3[96], b4[96], b5[96];
    swprintf(b1, 96, L"%ls  ·  %ls aciertos", plural(res["tests"].comoEntero(), L"test", L"tests").c_str(),
             prec < 0 ? L"—" : (std::to_wstring(prec) + L" %").c_str());
    if (pendientes) swprintf(b2, 96, L"%d para hoy  ·  %d nuevas", pendientes, nuevas);
    else swprintf(b2, 96, L"Al día  ·  %d nuevas", nuevas);
    swprintf(b3, 96, L"%d de %d temas resumidos", her["resumenes"].comoEntero(), her["temas"].comoEntero());
    if (her["desarrollos"].comoEntero())
        swprintf(b4, 96, her["desarrollos"].comoEntero() == 1 ? L"%d respondida  ·  media %ls" : L"%d respondidas  ·  media %ls", her["desarrollos"].comoEntero(),
                 numero(her["nota_desarrollo"].comoNumero()).c_str());
    else swprintf(b4, 96, L"Aún no has respondido ninguna");
    swprintf(b5, 96, L"20 preguntas  ·  1 min cada una");
    std::wstring descConcentracion = L"Bloques de " + std::to_wstring(gui::estilo().pomodoro) +
                                     L" minutos con descanso guiado: suman tiempo y experiencia.";
    struct Herramienta {
        Vista v;
        int accion;
        Icono i;
        const wchar_t* nombre;
        const wchar_t* desc;
        std::wstring dato;
        COLORREF color;
    };
    std::vector<Herramienta> herramientas = {
        {V_INICIO, A_PESTANA, I_TEST, L"Test", L"Preguntas tipo test de tus apuntes, al instante o redactadas por la IA.", b1, tm.acento},
        {V_TARJETAS, A_PESTANA, I_TARJETAS, L"Tarjetas", L"Repetición espaciada de los conceptos clave, con trucos de la IA.", b2, NARANJA},
        {V_RESUMENES, A_PESTANA, I_RESUMEN, L"Resúmenes", L"Ideas clave, conceptos y preguntas de examen de cada tema.", b3, TURQUESA},
        {V_DESARROLLO, A_PESTANA, I_DESARROLLO, L"Desarrollo", L"Responde con tus palabras y la IA te corrige como en el examen.", b4, VIOLETA},
        {V_CHAT, A_PESTANA, I_DUDAS, L"Dudas", L"Pregunta lo que no entiendas: te lo explica con tus apuntes.",
         s->chat.empty() ? std::wstring(L"Conversación nueva") : plural((int)(s->chat.size() + 1) / 2, L"pregunta hecha", L"preguntas hechas"), tm.acento},
        {V_HUB, A_SIMULACRO, I_RELOJ, L"Simulacro de examen", L"Cronometrado, con corrección al final y los fallos restando.", b5, ROSA},
        {V_BUSCAR, A_PESTANA, I_BUSCAR, L"Buscar y glosario", L"Encuentra cualquier concepto en tus apuntes, con su página.",
         plural((int)s->glosario["glosario"].tam(), L"concepto", L"conceptos") + L" en el glosario", tm.acento},
        {V_HUB, A_R_VISTA, I_MAPA, L"Mapas mentales", L"Cada tema de un vistazo: sus ideas principales y sus detalles.",
         L"Para memorizar la estructura", TURQUESA},
        {V_HUB, A_POMODORO, I_FUEGO, L"Concentración",
         descConcentracion.c_str(),
         ini["nivel"].esNulo() ? std::wstring(L"Empieza uno ahora") :
         std::to_wstring(minutosHoy(s)) +
             (gui::estilo().objetivoDiario > 0 ? L" de " + std::to_wstring(gui::estilo().objetivoDiario) + L" min hoy" : L" min hoy") +
             L"  ·  nivel " + std::to_wstring(ini["nivel"]["nivel"].comoEntero()), NARANJA},
    };
    // Las dudas las responde la IA: sin ella no se ofrecen
    if (!iaDisponible(s) && !s->chatEnCurso)
        herramientas.erase(std::remove_if(herramientas.begin(), herramientas.end(), [](const Herramienta& h) { return h.v == V_CHAT; }),
                           herramientas.end());
    int columnas = w >= E(s, 820) ? 3 : 2;
    int hueco = E(s, 14);
    int tw2 = (w - (columnas - 1) * hueco) / columnas, th = E(s, 148);
    const int nHerramientas = (int)herramientas.size();
    for (int k = 0; k < nHerramientas; ++k) {
        const Herramienta& hm = herramientas[k];
        RECT r = rect(x + (k % columnas) * (tw2 + hueco), y + (k / columnas) * (th + hueco), tw2, th);
        int dato = hm.accion == A_PESTANA ? (int)hm.v : hm.accion == A_R_VISTA ? 101 : 0;
        bool h = encima(s, hm.accion, dato);
        cristal(dc, r, E(s, 16), h ? tm.panelAlt : CLR_INVALID, h ? mezcla(tm.borde, hm.color, 0.7) : CLR_INVALID, h ? 2 : 1);
        int pad = E(s, 18);
        redondeado(dc, rect(r.left + pad, r.top + pad, E(s, 40), E(s, 40)), E(s, 11), mezcla(tm.panel, hm.color, tm.oscuro ? 0.24 : 0.16));
        glifo(s, dc, hm.i, r.left + pad + E(s, 20), r.top + pad + E(s, 20), E(s, 21), hm.color);
        texto(dc, s->f.subtitulo, tm.texto, hm.nombre, rect(r.left + pad + E(s, 54), r.top + pad, tw2 - 2 * pad - E(s, 54), E(s, 40)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.peque, tm.textoSuave, hm.desc, rect(r.left + pad, r.top + pad + E(s, 50), tw2 - 2 * pad, E(s, 38)),
              DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        texto(dc, s->f.negrita, mezcla(hm.color, tm.texto, 0.25), hm.dato, rect(r.left + pad, r.bottom - pad - E(s, 20), tw2 - 2 * pad, E(s, 20)),
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        zona(s, r, hm.accion, dato);
    }
    y += ((nHerramientas + columnas - 1) / columnas) * (th + hueco) + E(s, 10);
    return y - a.y;
}

// ================================================================ tarjetas

namespace {

const js::Valor& tarjetaActual(Estado* s) { return s->tarjetas[(std::size_t)s->tarjeta]; }

bool enSesionDeTarjetas(Estado* s) {
    return !s->tarjetasFin && s->tarjetas.tam() > 0 && s->tarjeta < (int)s->tarjetas.tam();
}

// Fraccion del giro (0 = sin girar, 1 = girada) y si ya se ve la cara de atras.
double giro(Estado* s) {
    ULONGLONG pasado = GetTickCount64() - s->volteo;
    if (!s->volteada) return 0.0;
    return std::min(1.0, pasado / 280.0);
}

std::wstring nombreTipo(const std::string& tipo) {
    if (tipo == "concepto") return L"Concepto";
    if (tipo == "completar") return L"Completa la frase";
    return L"Pregunta";
}

void empezarTarjetas(Estado* s, bool extra) {
    js::Valor p = js::Valor::objeto();
    p["n"] = s->nTarjetas;
    p["temas"] = temasElegidos(s);
    p["extra"] = extra;
    s->trucos.clear();
    enviar(s, "tarjetas", p, true, L"Barajando tus tarjetas…");
}

void calificarTarjeta(Estado* s, int nota) {
    if (!enSesionDeTarjetas(s) || !s->volteada) return;
    js::Valor p = js::Valor::objeto();
    p["tarjeta"] = tarjetaActual(s)["id"].comoCadena();
    p["nota"] = nota;
    enviar(s, "tarjeta", p);
    if (nota == 0) ++s->tarjetasOtra;
    else ++s->tarjetasBien;
    s->volteada = false;
    s->scroll = 0;
    if (s->tarjeta + 1 >= (int)s->tarjetas.tam()) {
        s->tarjetasFin = true;
        s->mensaje = s->tarjetasOtra == 0 ? L"¡Repaso perfecto! Vuelve mañana para las siguientes."
                                          : L"Buen repaso. Las que marcaste «Otra vez» siguen pendientes para hoy.";
        enviar(s, "inicio");
    } else {
        ++s->tarjeta;
        if (nota == 0) s->mensaje = L"No pasa nada: te la volveré a enseñar hoy mismo.";
        else if (nota == 2) s->mensaje = L"¡Fácil! La espaciaré más.";
        else s->mensaje = L"Bien. Volverá dentro de unos días.";
    }
    InvalidateRect(s->ventana, NULL, FALSE);
}

void voltear(Estado* s) {
    if (!enSesionDeTarjetas(s)) return;
    s->volteada = !s->volteada;
    s->volteo = GetTickCount64();
    SetTimer(s->ventana, TIMER_ANIMACION, 15, NULL);
}

int intervalo(const js::Valor& t, int nota) {
    int caja = t["caja"].comoEntero();
    int nueva = std::min(caja + nota, 5);
    return REPASO_DIAS[nueva];
}

}  // namespace

int pintarTarjetas(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& res = s->hola["resumen"];

    if (s->tarjetasFin) {
        RECT r = rect(x, y, w, E(s, 300));
        cristal(dc, r, E(s, 20));
        marca(dc, x + w / 2, y + E(s, 66), E(s, 30), true);
        texto(dc, s->f.titulo, tm.texto, L"¡Repaso terminado!", rect(x, y + E(s, 112), w, E(s, 36)), DT_CENTER | DT_SINGLELINE);
        wchar_t b[160];
        swprintf(b, 160, L"%ls  ·  %d bien  ·  %d para repetir",
                 plural(s->tarjetasBien + s->tarjetasOtra, L"tarjeta", L"tarjetas").c_str(), s->tarjetasBien, s->tarjetasOtra);
        texto(dc, s->f.texto, tm.textoSuave, b, rect(x, y + E(s, 154), w, E(s, 24)), DT_CENTER | DT_SINGLELINE);
        int bw = E(s, 230), bh = E(s, 44), bx = x + w / 2 - bw - E(s, 6), by = y + E(s, 206);
        if (s->tarjetasOtra > 0) {
            botonIcono(s, dc, rect(bx, by, bw, bh), I_REPETIR, L"Repetir las falladas", A_T_EMPEZAR, 0, PRIMARIO);
            botonIcono(s, dc, rect(bx + bw + E(s, 12), by, bw, bh), I_INICIO, L"Volver al inicio", A_PESTANA, V_HUB);
        } else {
            botonIcono(s, dc, rect(bx, by, bw, bh), I_TARJETAS, L"Seguir con más", A_T_EMPEZAR, 0, PRIMARIO);
            botonIcono(s, dc, rect(bx + bw + E(s, 12), by, bw, bh), I_INICIO, L"Volver al inicio", A_PESTANA, V_HUB);
        }
        return r.bottom - a.y + E(s, 20);
    }

    if (!enSesionDeTarjetas(s)) {
        y += cabecera(s, dc, x, y, w, I_TARJETAS, NARANJA, L"Tarjetas de memoria",
                      L"Lee la cara de delante, intenta recordar la respuesta y gira la tarjeta. Según lo bien que la "
                      L"sepas volverá en días o semanas: así se fija en la memoria a largo plazo.");
        int hueco = E(s, 12), cw = (w - 2 * hueco) / 3, ch = E(s, 84);
        int pendientes = res["tarjetas_pendientes"].comoEntero(), nuevas = res["tarjetas_nuevas"].comoEntero();
        cifra(s, dc, rect(x, y, cw, ch), std::to_wstring(pendientes), L"Toca repasar hoy", pendientes ? NARANJA : tm.ok, I_REPETIR);
        cifra(s, dc, rect(x + cw + hueco, y, cw, ch), std::to_wstring(nuevas), L"Nuevas por aprender", tm.texto, I_CHISPA);
        cifra(s, dc, rect(x + 2 * (cw + hueco), y, cw, ch), std::to_wstring(res["tarjetas_total"].comoEntero()),
              L"Tarjetas en tu mazo", tm.textoSuave, I_TARJETAS);
        y += ch + E(s, 22);
        RECT caja = rect(x, y, w, E(s, 150));
        cristal(dc, caja, E(s, 16));
        int pad = E(s, 22);
        texto(dc, s->f.subtitulo, tm.texto, L"Sesión de repaso", rect(x + pad, y + E(s, 18), w, E(s, 26)), DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.textoSuave, L"Tarjetas", rect(x + pad, y + E(s, 58), E(s, 100), E(s, 34)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int px = x + pad + E(s, 100);
        for (int n : {10, 20, 30, 50}) px += chip(s, dc, px, y + E(s, 58), std::to_wstring(n), s->nTarjetas == n, A_T_CANTIDAD, n) + E(s, 8);
        int seleccionados = 0;
        for (bool b : s->temas) seleccionados += b ? 1 : 0;
        std::wstring temas = seleccionados == (int)s->temas.size() ? L"De todos los temas" :
                             L"De " + plural(seleccionados, L"tema elegido", L"temas elegidos") + L" en «Nuevo test»";
        texto(dc, s->f.peque, tm.textoSuave, temas + L"  ·  primero las que toca repasar, luego las nuevas.",
              rect(x + pad, y + E(s, 106), w - 2 * pad, E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y = caja.bottom + E(s, 22);
        // Tu mazo por cajas: cada acierto sube la tarjeta una caja y vuelve más tarde; un fallo la baja a la primera
        const js::Valor& cajas = res["tarjetas_cajas"];
        if (cajas.tam() == 6 && res["tarjetas_total"].comoEntero() > 0) {
            int alto = E(s, 196);
            RECT panel = rect(x, y, w, alto);
            cristal(dc, panel, E(s, 16));
            texto(dc, s->f.subtitulo, tm.texto, L"Tu mazo por cajas", rect(x + pad, y + E(s, 18), w, E(s, 26)),
                  DT_LEFT | DT_SINGLELINE);
            int dominadas = res["tarjetas_dominadas"].comoEntero();
            texto(dc, s->f.peque, tm.textoSuave,
                  L"Cada acierto sube la tarjeta una caja y vuelve más tarde; si la fallas, vuelve a la primera.  ·  " +
                      plural(dominadas, L"tarjeta dominada", L"tarjetas dominadas") + L" (una semana o más)",
                  rect(x + pad, y + E(s, 46), w - 2 * pad, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            const wchar_t* cuando[] = {L"Nuevas", L"Hoy", L"1 día", L"3 días", L"1 semana", L"16 días", L"5 semanas"};
            int valores[7] = {nuevas};
            int maximo = std::max(1, nuevas);
            for (int k = 0; k < 6; ++k) {
                valores[k + 1] = cajas[(std::size_t)k].comoEntero();
                maximo = std::max(maximo, valores[k + 1]);
            }
            int hueco = E(s, 10), bw = (w - 2 * pad - 6 * hueco) / 7, by = y + E(s, 80), bh = E(s, 96);
            for (int k = 0; k < 7; ++k) {
                int bx = x + pad + k * (bw + hueco);
                // De naranja (recién empezadas) a verde (dominadas); las nuevas, en gris
                COLORREF c = k == 0 ? tm.textoSuave : mezcla(NARANJA, tm.ok, (k - 1) / 5.0);
                RECT r = rect(bx, by, bw, bh);
                redondeado(dc, r, E(s, 12), mezcla(tm.panel, c, tm.oscuro ? 0.10 : 0.07), mezcla(tm.borde, c, 0.35));
                // Barra de nivel abajo, proporcional a cuántas hay
                int lleno = valores[k] ? std::max(E(s, 4), (bw - E(s, 24)) * valores[k] / maximo) : 0;
                redondeado(dc, rect(bx + E(s, 12), by + bh - E(s, 16), bw - E(s, 24), E(s, 5)), E(s, 3), mezcla(tm.panel, c, 0.18));
                if (lleno) redondeado(dc, rect(bx + E(s, 12), by + bh - E(s, 16), lleno, E(s, 5)), E(s, 3), c);
                texto(dc, s->f.subtitulo, valores[k] ? c : tm.textoSuave, std::to_wstring(valores[k]),
                      rect(bx, by + E(s, 12), bw, E(s, 34)), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                texto(dc, s->f.mini, tm.textoSuave, cuando[k], rect(bx, by + E(s, 48), bw, E(s, 18)),
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            y = panel.bottom + E(s, 22);
        }
        if (pendientes == 0 && nuevas == 0 && res["tarjetas_total"].comoEntero() > 0) {
            RECT aviso = rect(x, y, w, E(s, 64));
            redondeado(dc, aviso, E(s, 14), mezcla(tm.panel, tm.ok, 0.10), mezcla(tm.borde, tm.ok, 0.5));
            texto(dc, s->f.negrita, tm.texto, L"Estás al día: no te toca repasar ninguna tarjeta.",
                  rect(x + E(s, 18), y, w - E(s, 260), E(s, 64)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            boton(s, dc, rect(aviso.right - E(s, 236), y + E(s, 12), E(s, 220), E(s, 40)), L"Repaso extra", A_T_EXTRA);
            y += E(s, 84);
        }
        return y - a.y;
    }

    // --- Sesion en curso
    const js::Valor& t = tarjetaActual(s);
    int n = (int)s->tarjetas.tam();
    wchar_t cab[64];
    swprintf(cab, 64, L"Tarjeta %d de %d", s->tarjeta + 1, n);
    texto(dc, s->f.titulo, tm.texto, cab, rect(x, y, w, E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    {
        int rx = x + w;
        std::wstring otra = std::to_wstring(s->tarjetasOtra), bien = std::to_wstring(s->tarjetasBien);
        int ow = anchoTexto(dc, s->f.negrita, otra), bw2 = anchoTexto(dc, s->f.negrita, bien);
        texto(dc, s->f.negrita, tm.texto, otra, rect(rx - ow, y, ow, E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        icono(s, dc, I_REPETIR, rx - ow - E(s, 14), y + E(s, 17), E(s, 16), tm.mal);
        rx -= ow + E(s, 36);
        texto(dc, s->f.negrita, tm.texto, bien, rect(rx - bw2, y, bw2, E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        marca(dc, rx - bw2 - E(s, 14), y + E(s, 17), E(s, 9), true);
    }
    y += E(s, 42);
    barra(dc, rect(x, y, w, E(s, 8)), (double)s->tarjeta / std::max(1, n), NARANJA);
    y += E(s, 28);

    int cw = std::min(w, E(s, 860));
    int cx0 = x + (w - cw) / 2;
    int pad = E(s, 34);
    double fr = giro(s);
    bool atras = s->volteada && fr >= 0.5;
    std::wstring frente = W(t["frente"]), reverso = W(t["reverso"]), detalle = W(t["detalle"]);
    std::wstring cita = W(t["cita"]);
    Gdiplus::Image* im = atras ? imagen(s, W(t["imagen"]["archivo"])) : NULL;
    int anchoImg = (im && cw >= E(s, 700)) ? cw * 36 / 100 : 0;
    int iw = cw - 2 * pad - (anchoImg ? anchoImg + E(s, 20) : 0);
    int alto;
    if (!atras) {
        alto = std::max(E(s, 300), altoTexto(dc, s->f.tarjeta, frente, cw - 2 * pad) + E(s, 150));
    } else {
        int h = E(s, 40) + altoTexto(dc, s->f.texto, frente, iw) + E(s, 26) + altoTexto(dc, s->f.tarjeta, reverso, iw);
        if (!detalle.empty()) h += E(s, 14) + altoTexto(dc, s->f.texto, detalle, iw);
        if (!cita.empty()) h += E(s, 18) + altoTexto(dc, s->f.peque, L"«" + cita + L"»", iw - E(s, 16));
        h += E(s, 30);
        alto = std::max(E(s, 300), h + 2 * pad);
        if (anchoImg) alto = std::max(alto, anchoImg * 3 / 4 + 2 * pad);
    }
    // Giro: la tarjeta se estrecha hasta el canto y vuelve a abrirse por la otra cara.
    double escala = std::fabs(std::cos(fr * 3.14159265));
    if (!s->volteada) escala = 1.0;
    int anchoVisible = std::max(E(s, 6), (int)(cw * escala));
    RECT carta = rect(cx0 + (cw - anchoVisible) / 2, y, anchoVisible, alto);
    COLORREF base = atras ? mezcla(tm.panel, NARANJA, tm.oscuro ? 0.07 : 0.05) : tm.panel;
    cristal(dc, carta, E(s, 22), base, mezcla(tm.borde, NARANJA, atras ? 0.55 : 0.3), 2);
    bool animando = s->volteada && fr < 1.0;
    if (!animando) {
        int px = cx0 + pad, py = y + E(s, 24);
        px += etiqueta(s, dc, px, py, nombreTipo(t["tipo"].comoCadena()), NARANJA) + E(s, 8);
        px += etiqueta(s, dc, px, py, W(t["tema"]), tm.acento) + E(s, 8);
        if (t["nueva"].comoBool()) etiqueta(s, dc, px, py, L"Nueva", tm.ok);
        if (!atras) {
            int h = altoTexto(dc, s->f.tarjeta, frente, cw - 2 * pad);
            texto(dc, s->f.tarjeta, tm.texto, frente, rect(cx0 + pad, y + (alto - h) / 2, cw - 2 * pad, h), DT_CENTER | DT_WORDBREAK);
            texto(dc, s->f.peque, tm.textoSuave, L"Piensa la respuesta y pulsa para girar la tarjeta  ·  Espacio",
                  rect(cx0, y + alto - E(s, 40), cw, E(s, 20)), DT_CENTER | DT_SINGLELINE);
            zona(s, carta, A_T_VOLTEAR);
        } else {
            int ty = y + E(s, 64);
            int h = altoTexto(dc, s->f.texto, frente, iw);
            texto(dc, s->f.texto, tm.textoSuave, frente, rect(cx0 + pad, ty, iw, h));
            ty += h + E(s, 12);
            redondeado(dc, rect(cx0 + pad, ty, iw, 1), 0, mezcla(tm.borde, NARANJA, 0.4));
            ty += E(s, 14);
            h = altoTexto(dc, s->f.tarjeta, reverso, iw);
            texto(dc, s->f.tarjeta, tm.texto, reverso, rect(cx0 + pad, ty, iw, h));
            ty += h;
            if (!detalle.empty()) {
                ty += E(s, 14);
                h = altoTexto(dc, s->f.texto, detalle, iw);
                texto(dc, s->f.texto, tm.texto, detalle, rect(cx0 + pad, ty, iw, h));
                ty += h;
            }
            if (!cita.empty()) {
                ty += E(s, 18);
                std::wstring c = L"«" + cita + L"»";
                h = altoTexto(dc, s->f.peque, c, iw - E(s, 16));
                redondeado(dc, rect(cx0 + pad, ty, E(s, 3), h), 1, mezcla(tm.panel, NARANJA, 0.6));
                texto(dc, s->f.peque, tm.textoSuave, c, rect(cx0 + pad + E(s, 14), ty, iw - E(s, 16), h));
                ty += h;
            }
            texto(dc, s->f.mini, tm.textoSuave, L"Fuente: " + W(t["tema"]) + L"  ·  " + W(t["ubicacion"]),
                  rect(cx0 + pad, y + alto - E(s, 36), iw, E(s, 18)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (anchoImg) {
                RECT ri = rect(cx0 + cw - pad - anchoImg, y + E(s, 64), anchoImg, alto - E(s, 64) - pad);
                redondeado(dc, ri, E(s, 12), RGB(255, 255, 255));
                RECT dentro = ri;
                InflateRect(&dentro, -E(s, 6), -E(s, 6));
                imagenAjustada(im, dc, dentro);
                zona(s, ri, A_T_ZOOM);
            }
        }
    }
    y += alto + E(s, 20);

    if (atras && !animando) {
        // Botones de calificacion con el intervalo que tendria la tarjeta
        int bw = std::min(E(s, 250), (cw - 2 * E(s, 12)) / 3), bh = E(s, 58);
        int bx = cx0 + (cw - 3 * bw - 2 * E(s, 12)) / 2;
        struct Nota {
            int nota;
            const wchar_t* nombre;
            const wchar_t* tecla;
            COLORREF color;
        } notas[3] = {{0, L"Otra vez", L"1", tm.mal}, {1, L"Bien", L"2", tm.acento}, {2, L"Fácil", L"3", tm.ok}};
        for (int k = 0; k < 3; ++k) {
            RECT r = rect(bx + k * (bw + E(s, 12)), y, bw, bh);
            bool h = encima(s, A_T_NOTA, notas[k].nota);
            COLORREF c = notas[k].color;
            redondeado(dc, r, E(s, 14), h ? mezcla(tm.panel, c, 0.30) : mezcla(tm.panel, c, 0.13), mezcla(tm.borde, c, h ? 0.9 : 0.55),
                       h ? 2 : 1);
            texto(dc, s->f.negrita, tm.texto, notas[k].nombre, rect(r.left, r.top + E(s, 8), bw, E(s, 22)), DT_CENTER | DT_SINGLELINE);
            int d = notas[k].nota == 0 ? 0 : intervalo(t, notas[k].nota);
            std::wstring cuando = notas[k].nota == 0 ? L"hoy otra vez" : L"en " + dias(d);
            texto(dc, s->f.peque, mezcla(c, tm.texto, 0.3), cuando + L"  ·  " + notas[k].tecla,
                  rect(r.left, r.top + E(s, 31), bw, E(s, 20)), DT_CENTER | DT_SINGLELINE);
            zona(s, r, A_T_NOTA, notas[k].nota);
        }
        y += bh + E(s, 18);
        // Ayudas: truco de la IA, duda y apunte
        std::string id = t["id"].comoCadena();
        auto it = s->trucos.find(id);
        bool escribiendo = s->truco.activa() && s->trucoTarjeta == id;
        if (escribiendo || it != s->trucos.end()) {
            std::wstring tt = escribiendo ? (s->truco.parcial.empty() ? L"" : conCursor(s, s->truco.parcial)) : it->second;
            int ipad = E(s, 18);
            int ht = tt.empty() ? E(s, 22) : textoRico(s, dc, tt, 0, 0, cw - 2 * ipad - E(s, 40), false);
            RECT caja = rect(cx0, y, cw, ht + 2 * ipad);
            redondeado(dc, caja, E(s, 14), mezcla(tm.panel, tm.medio, tm.oscuro ? 0.10 : 0.08), mezcla(tm.borde, tm.medio, 0.5));
            icono(s, dc, I_BOMBILLA, cx0 + ipad + E(s, 10), y + ipad + E(s, 10), E(s, 20), tm.medio);
            if (tt.empty()) {
                spinner(s, dc, cx0 + ipad + E(s, 50), y + ipad + E(s, 11), E(s, 8));
                texto(dc, s->f.texto, tm.textoSuave, L"Pensando un truco para recordarla…  " + cronometro(s->truco),
                      rect(cx0 + ipad + E(s, 66), y + ipad, cw, E(s, 22)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            } else {
                textoRico(s, dc, tt, cx0 + ipad + E(s, 40), y + ipad, cw - 2 * ipad - E(s, 40));
            }
            y = caja.bottom + E(s, 14);
        }
        int aw = E(s, 210), ah = E(s, 38), ax = cx0 + (cw - 3 * aw - 2 * E(s, 10)) / 2;
        bool ia = iaDisponible(s);
        if (it == s->trucos.end() && !escribiendo)
            botonIcono(s, dc, rect(ax, y, aw, ah), I_BOMBILLA, L"Truco para recordarla", A_T_TRUCO, 0, FANTASMA, ia && !s->truco.activa());
        else
            botonIcono(s, dc, rect(ax, y, aw, ah), I_REPETIR, L"Otro truco", A_T_TRUCO, 1, FANTASMA, ia && !s->truco.activa());
        if (!ia) ax += (aw + E(s, 10)) / 2;  // sin IA no hay a quien preguntar: dos botones, centrados
        if (ia) botonIcono(s, dc, rect(ax + aw + E(s, 10), y, aw, ah), I_DUDAS, L"Preguntar al profesor", A_T_DUDA, 0, FANTASMA);
        botonIcono(s, dc, rect(ax + (ia ? 2 : 1) * (aw + E(s, 10)), y, aw, ah), I_LIBRO, L"Abrir el apunte", A_T_ABRIR, 0, FANTASMA);
        y += ah + E(s, 10);
    } else if (!s->volteada) {
        boton(s, dc, rect(cx0 + cw / 2 - E(s, 160), y, E(s, 320), E(s, 48)), L"Mostrar la respuesta", A_T_VOLTEAR, 0, PRIMARIO);
        y += E(s, 58);
    }
    return y - a.y;
}

void pieTarjetas(Estado* s, HDC dc, RECT pie) {
    const gui::Tema& tm = gui::tema();
    int alto = E(s, 44), y = pie.top + (pie.bottom - pie.top - alto) / 2;
    if (enSesionDeTarjetas(s)) {
        boton(s, dc, rect(pie.left, y, E(s, 180), alto), L"Terminar repaso", A_T_TERMINAR, 0, FANTASMA);
        texto(dc, s->f.peque, tm.textoSuave, L"Espacio: girar  ·  1 Otra vez  ·  2 Bien  ·  3 Fácil",
              rect(pie.left + E(s, 200), y, pie.right - pie.left - E(s, 200), alto), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else if (!s->tarjetasFin) {
        int pendientes = s->hola["resumen"]["tarjetas_pendientes"].comoEntero();
        std::wstring t = pendientes ? L"Repasar ahora (" + std::to_wstring(std::min(pendientes, s->nTarjetas)) + L")  →"
                                    : L"Aprender nuevas  →";
        boton(s, dc, rect(pie.right - E(s, 260), y, E(s, 260), alto), t, A_T_EMPEZAR, 0, PRIMARIO);
    }
}

// ================================================================ resumenes

namespace {

int indiceResumen(Estado* s, const std::string& ruta) {
    const js::Valor& temas = s->resumenes["temas"];
    for (std::size_t i = 0; i < temas.tam(); ++i)
        if (temas[i]["ruta"].comoCadena() == ruta) return (int)i;
    return -1;
}

void ponerEstadoResumen(Estado* s, const std::string& ruta, const std::string& estado) {
    js::Valor temas = js::Valor::array();
    const js::Valor& viejos = s->resumenes["temas"];
    for (std::size_t i = 0; i < viejos.tam(); ++i) {
        js::Valor t = viejos[i];
        if (t["ruta"].comoCadena() == ruta) t["estado"] = estado;
        temas.agregar(t);
    }
    s->resumenes["temas"] = temas;
}

void pedirResumen(Estado* s, const std::string& ruta, bool rehacer, bool conIa) {
    if (s->tareaResumen.activa()) return;
    js::Valor p = js::Valor::objeto();
    p["ruta"] = ruta;
    p["rehacer"] = rehacer;
    p["ia"] = conIa;
    s->resumenRuta = ruta;
    int i = indiceResumen(s, ruta);
    std::string estado = i >= 0 ? s->resumenes["temas"][(std::size_t)i]["estado"].comoCadena() : "no";
    bool generara = conIa && iaDisponible(s) && (rehacer || estado != "ia");
    if (generara) {
        int id = enviar(s, "resumen", p);
        empezarTarea(s->tareaResumen, id);
        s->resumen = js::Valor();
        s->mensaje = L"Estoy leyendo el apunte para resumirlo. La IA tarda un par de minutos: puedes seguir con otra "
                     L"cosa y volver luego.";
        SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    } else {
        enviar(s, "resumen", p, true, L"Preparando el resumen…");
    }
}

}  // namespace

int pintarResumenes(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    y += cabecera(s, dc, x, y, w, I_RESUMEN, TURQUESA, L"Resúmenes por tema",
                  L"Las ideas clave, los conceptos con su definición y las preguntas que podrían caer en el examen. "
                  L"Los escribe la IA a partir de cada apunte y quedan guardados.");
    const js::Valor& temas = s->resumenes["temas"];
    int wl = std::max(E(s, 230), std::min(E(s, 330), w * 30 / 100));
    int xr = x + wl + E(s, 20), wr = w - wl - E(s, 20);

    // --- Lista de temas
    int ly = y;
    {
        bool trabajando = !s->resumenesFondo.esNulo();
        int hechosIa = 0;
        for (std::size_t i = 0; i < temas.tam(); ++i) hechosIa += temas[i]["estado"].comoCadena() == "ia" ? 1 : 0;
        if (trabajando) {
            RECT r = rect(x, ly, wl, E(s, 74));
            redondeado(dc, r, E(s, 14), mezcla(tm.panel, VIOLETA, 0.12), mezcla(tm.borde, VIOLETA, 0.5));
            spinner(s, dc, x + E(s, 22), ly + E(s, 22), E(s, 8));
            wchar_t b[96];
            swprintf(b, 96, L"Resumiendo %d de %d", s->resumenesFondo["hechos"].comoEntero() + 1,
                     s->resumenesFondo["total"].comoEntero());
            texto(dc, s->f.negrita, tm.texto, b, rect(x + E(s, 40), ly + E(s, 10), wl - E(s, 130), E(s, 24)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            texto(dc, s->f.peque, tm.textoSuave, W(s->resumenesFondo["tema"]), rect(x + E(s, 16), ly + E(s, 42), wl - E(s, 32), E(s, 20)),
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            boton(s, dc, rect(r.right - E(s, 86), ly + E(s, 8), E(s, 76), E(s, 30)), L"Parar", A_R_DETENER, 0, FANTASMA);
            ly += E(s, 86);
        } else if (iaDisponible(s) && hechosIa < (int)temas.tam()) {
            botonIcono(s, dc, rect(x, ly, wl, E(s, 42)), I_CHISPA, L"Resumir todos con IA", A_R_TODOS, 0, SECUNDARIO,
                       !s->tareaResumen.activa());
            ly += E(s, 54);
        }
        int filaAlto = E(s, 50), subAlto = E(s, 60);
        std::vector<FilaTema> filas = filasPorTema(temas);
        int altoFilas = 0;
        for (const FilaTema& f : filas) altoFilas += f.indice < 0 ? subAlto : filaAlto;
        RECT lista = rect(x, ly, wl, std::max(filaAlto, altoFilas) + E(s, 10));
        cristal(dc, lista, E(s, 14));
        if (temas.tam() == 0)
            texto(dc, s->f.texto, tm.textoSuave, L"Tu Temario está vacío.", rect(x + E(s, 16), ly, wl, filaAlto),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int fy = ly + E(s, 5);
        for (const FilaTema& f : filas) {
            if (f.indice < 0) {
                int hechos = 0;
                for (std::size_t j = 0; j < temas.tam(); ++j)
                    if (W(temas[j]["grupo"]) == f.grupo && temas[j]["estado"].comoCadena() == "ia") ++hechos;
                wchar_t info[48];
                swprintf(info, 48, L"%d/%d", hechos, f.miembros);
                subtituloTema(s, dc, rect(x + E(s, 14), fy + E(s, 6), wl - E(s, 28), subAlto - E(s, 8)), f.grupo, info,
                              TURQUESA);
                fy += subAlto;
                continue;
            }
            std::size_t i = (std::size_t)f.indice;
            const js::Valor& t = temas[i];
            RECT r = rect(x + E(s, 5), fy, wl - E(s, 10), filaAlto);
            fy += filaAlto;
            bool elegido = t["ruta"].comoCadena() == s->resumenRuta;
            bool h = encima(s, A_R_TEMA, (int)i);
            if (elegido) redondeado(dc, r, E(s, 10), mezcla(tm.panel, TURQUESA, tm.oscuro ? 0.20 : 0.14));
            else if (h) redondeado(dc, r, E(s, 10), tm.panelAlt);
            std::string estado = t["estado"].comoCadena();
            int cx = r.left + E(s, 20), cy = r.top + filaAlto / 2;
            bool escribiendo = (s->tareaResumen.activa() && t["ruta"].comoCadena() == s->resumenRuta) ||
                               (!s->resumenesFondo.esNulo() && s->resumenesFondo["ruta"].comoCadena() == t["ruta"].comoCadena());
            if (escribiendo) spinner(s, dc, cx, cy, E(s, 7));
            else if (estado == "ia") icono(s, dc, I_CHISPA, cx, cy, E(s, 18), VIOLETA);
            else if (estado == "auto") circulo(dc, cx, cy, E(s, 6), mezcla(tm.panel, TURQUESA, 0.7));
            else if (estado == "antiguo") circulo(dc, cx, cy, E(s, 6), tm.medio);
            else circulo(dc, cx, cy, E(s, 6), tm.panel, mezcla(tm.borde, tm.textoSuave, 0.4), 1.5f);
            texto(dc, elegido ? s->f.negrita : s->f.texto, tm.texto, W(t["tema"]),
                  rect(cx + E(s, 18), r.top + E(s, 6), r.right - cx - E(s, 26), E(s, 22)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            std::wstring info = estado == "ia" ? L"Resumen de la IA" : estado == "auto" ? L"Resumen automático"
                              : estado == "antiguo" ? L"El apunte ha cambiado" : L"Sin resumir";
            if (!t["precision"].esNulo()) info += L"  ·  " + std::to_wstring(t["precision"].comoEntero()) + L" % aciertos";
            texto(dc, s->f.mini, tm.textoSuave, info, rect(cx + E(s, 18), r.top + E(s, 27), r.right - cx - E(s, 26), E(s, 18)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            zona(s, r, A_R_TEMA, (int)i);
        }
        ly = lista.bottom;
    }

    // --- Resumen elegido
    int ry = y;
    int i = indiceResumen(s, s->resumenRuta);
    bool fondoAqui = !s->resumenesFondo.esNulo() && s->resumenesFondo["ruta"].comoCadena() == s->resumenRuta &&
                     !s->tareaResumen.activa();
    if (i < 0) {
        RECT r = rect(xr, ry, wr, E(s, 220));
        cristal(dc, r, E(s, 18));
        icono(s, dc, I_LIBRO, xr + wr / 2, ry + E(s, 70), E(s, 44), mezcla(tm.textoSuave, TURQUESA, 0.4));
        texto(dc, s->f.subtitulo, tm.texto, L"Elige un tema de la lista", rect(xr, ry + E(s, 112), wr, E(s, 28)), DT_CENTER | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.textoSuave, L"Los marcados con una chispa ya tienen su resumen de la IA.",
              rect(xr, ry + E(s, 146), wr, E(s, 24)), DT_CENTER | DT_SINGLELINE);
        ry = r.bottom;
    } else {
        const js::Valor& t = temas[(std::size_t)i];
        std::string estado = t["estado"].comoCadena();
        if (!t["grupo"].comoCadena().empty()) {  // el tema al que pertenece este apartado
            texto(dc, s->f.peque, TURQUESA, W(t["grupo"]), rect(xr, ry, wr, E(s, 20)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            ry += E(s, 22);
        }
        texto(dc, s->f.titulo, tm.texto, W(t["tema"]), rect(xr, ry, wr, E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        ry += E(s, 38);
        bool cargado = s->resumen["ruta"].comoCadena() == s->resumenRuta && !s->tareaResumen.activa();
        std::wstring sub;
        if (s->tareaResumen.activa() || fondoAqui) sub = L"La IA está escribiendo este resumen…";
        else if (cargado && s->resumen["origen"].comoCadena() == "ia") sub = L"Resumen de la IA (" + W(s->resumen["modelo"]) + L") a partir del apunte";
        else if (cargado) sub = L"Resumen automático sin IA: frases clave y definiciones sacadas del apunte";
        else sub = plural(t["paginas"].comoEntero(), L"página", L"páginas") + L"  ·  aún sin resumir";
        texto(dc, s->f.peque, tm.textoSuave, sub, rect(xr, ry, wr, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        ry += E(s, 30);
        {
            int px = xr;
            px += chip(s, dc, px, ry, L"Resumen", !s->verMapa, A_R_VISTA, 0) + E(s, 8);
            chip(s, dc, px, ry, L"Mapa mental", s->verMapa, A_R_VISTA, 1);
            ry += E(s, 48);
        }
        if (s->verMapa) {
            bool cargadoMapa = s->mapa["ruta"].comoCadena() == s->resumenRuta && !s->tareaMapa.activa();
            int bh = E(s, 36), bw = std::min(E(s, 210), (wr - 2 * E(s, 8)) / 3);
            bool ia = iaDisponible(s);
            if (cargadoMapa && s->mapa["origen"].comoCadena() == "ia")
                botonIcono(s, dc, rect(xr, ry, bw, bh), I_REPETIR, L"Rehacer mapa", A_R_MAPA, 1, SECUNDARIO, ia);
            else
                botonIcono(s, dc, rect(xr, ry, bw, bh), I_CHISPA, L"Mapa con IA", A_R_MAPA, 0, PRIMARIO, ia && !s->tareaMapa.activa());
            botonIcono(s, dc, rect(xr + bw + E(s, 8), ry, bw, bh), I_TEST, L"Test del tema", A_R_TEST, 0, SECUNDARIO);
            botonIcono(s, dc, rect(xr + 2 * (bw + E(s, 8)), ry, bw, bh), I_LIBRO, L"Abrir apunte", A_R_ABRIR, 0, FANTASMA);
            ry += bh + E(s, 18);
            if (cargadoMapa && s->mapa["origen"].comoCadena() != "ia") {
                texto(dc, s->f.peque, tm.textoSuave,
                      ia ? L"Mapa automático con los conceptos del apunte. «Mapa con IA» hace uno mucho mejor (tarda un minuto o dos)."
                         : L"Mapa automático con los conceptos del apunte.",
                      rect(xr, ry, wr, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                ry += E(s, 30);
            }
            ry += pintarMapa(s, dc, xr, ry, wr);
            return std::max(ly, ry) - a.y + E(s, 10);
        }
        // Acciones
        {
            int bh = E(s, 36), bx = xr;
            bool ia = iaDisponible(s);
            bool ocupada = s->tareaResumen.activa() || fondoAqui;
            int bw = std::min(E(s, 190), (wr - 3 * E(s, 8)) / 4);
            if (cargado && s->resumen["origen"].comoCadena() == "ia")
                botonIcono(s, dc, rect(bx, ry, bw, bh), I_REPETIR, L"Rehacer", A_R_GENERAR, 1, SECUNDARIO, ia && !ocupada);
            else
                botonIcono(s, dc, rect(bx, ry, bw, bh), I_CHISPA, L"Resumir con IA", A_R_GENERAR, 0, PRIMARIO, ia && !ocupada);
            bx += bw + E(s, 8);
            botonIcono(s, dc, rect(bx, ry, bw, bh), I_TEST, L"Test del tema", A_R_TEST, 0, SECUNDARIO);
            bx += bw + E(s, 8);
            botonIcono(s, dc, rect(bx, ry, bw, bh), I_LIBRO, L"Abrir apunte", A_R_ABRIR, 0, FANTASMA);
            bx += bw + E(s, 8);
            botonIcono(s, dc, rect(bx, ry, bw, bh), I_COPIAR, L"Copiar", A_R_COPIAR, 0, FANTASMA, cargado);
            ry += bh + E(s, 18);
        }
        if (s->tareaResumen.activa() || fondoAqui) {
            TareaIA t2 = s->tareaResumen;
            if (fondoAqui) {
                t2.parcial = W(s->resumenesFondo["parcial"]);
                t2.inicio = 0;
            }
            ry += cajaEscribiendo(s, dc, xr, ry, wr, t2, L"La IA está resumiendo",
                                  L"Leyendo el apunte… (en este equipo tarda en torno a un minuto antes de empezar a escribir)",
                                  s->tareaResumen.activa() ? A_R_CANCELAR : A_R_DETENER);
        } else if (cargado) {
            int pad = E(s, 26);
            std::wstring md = W(s->resumen["texto"]);
            int h = textoRico(s, dc, md, 0, 0, wr - 2 * pad, false);
            RECT r = rect(xr, ry, wr, h + 2 * pad);
            cristal(dc, r, E(s, 18));
            redondeado(dc, rect(xr, ry + E(s, 20), E(s, 4), h + 2 * pad - E(s, 40)), E(s, 2),
                       s->resumen["origen"].comoCadena() == "ia" ? VIOLETA : TURQUESA);
            textoRico(s, dc, md, xr + pad, ry + pad, wr - 2 * pad);
            ry = r.bottom;
            if (s->resumen["origen"].comoCadena() != "ia" && iaDisponible(s)) {
                ry += E(s, 14);
                texto(dc, s->f.peque, tm.textoSuave, L"Consejo: «Resumir con IA» escribe un resumen mucho mejor, con tus conceptos explicados.",
                      rect(xr, ry, wr, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                ry += E(s, 20);
            }
        } else {
            RECT r = rect(xr, ry, wr, E(s, 170));
            cristal(dc, r, E(s, 18));
            texto(dc, s->f.subtitulo, tm.texto, estado == "antiguo" ? L"El apunte ha cambiado desde el último resumen" : L"Este tema aún no tiene resumen",
                  rect(xr + E(s, 24), ry + E(s, 24), wr - E(s, 48), E(s, 26)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            std::wstring explica = iaDisponible(s)
                ? L"Pulsa «Resumir con IA»: lee el apunte y escribe sus ideas clave, conceptos y preguntas de examen. "
                  L"Tarda un par de minutos y queda guardado. Si tienes prisa, el resumen automático es instantáneo."
                : L"La IA (" + servidorIa(s) + L") no está disponible ahora, pero puedo hacerte un resumen automático con "
                  L"las frases más importantes y las definiciones del apunte.";
            int h = altoTexto(dc, s->f.texto, explica, wr - E(s, 48));
            texto(dc, s->f.texto, tm.textoSuave, explica, rect(xr + E(s, 24), ry + E(s, 60), wr - E(s, 48), h));
            boton(s, dc, rect(xr + E(s, 24), r.bottom - E(s, 56), E(s, 270), E(s, 38)), L"Resumen automático (sin IA)",
                  A_R_GENERAR, 2, SECUNDARIO);
            ry = r.bottom;
        }
    }
    return std::max(ly, ry) - a.y + E(s, 10);
}

void pieResumenes(Estado*, HDC, RECT) {}

// =============================================================== desarrollo

namespace {

bool respondiendo(Estado* s) {
    return !s->abierta.esNulo() && s->correccion.esNulo() && !s->tareaCorreccion.activa() && !s->tareaAbierta.activa();
}

std::wstring textoRespuesta(Estado* s) {
    int largo = GetWindowTextLengthW(s->respuesta);
    std::vector<wchar_t> b((std::size_t)largo + 1);
    GetWindowTextW(s->respuesta, b.data(), largo + 1);
    return b.data();
}

int contarPalabras(const std::wstring& t) {
    int n = 0;
    bool dentro = false;
    for (wchar_t c : t) {
        bool letra = iswalnum(c) != 0;
        if (letra && !dentro) ++n;
        dentro = letra;
    }
    return n;
}

void pedirAbierta(Estado* s, bool conIa, bool nueva) {
    if (s->tareaAbierta.activa()) return;
    js::Valor p = js::Valor::objeto();
    if (!s->desarrolloRuta.empty()) p["ruta"] = s->desarrolloRuta;
    p["dificultad"] = s->desarrolloDificultad;
    p["ia"] = conIa;
    p["nueva"] = nueva;
    int id = enviar(s, "abierta", p);
    empezarTarea(s->tareaAbierta, id);
    s->abierta = js::Valor();
    s->correccion = js::Valor();
    s->respuestaEnviada.clear();
    SetWindowTextW(s->respuesta, L"");
    s->mensaje = conIa && iaDisponible(s) ? L"Preparando una pregunta de desarrollo. Si la IA tiene que redactarla, tarda un minuto o dos."
                                          : L"Aquí tienes una pregunta sacada de las definiciones de tu apunte.";
    SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    irAVista(s, V_DESARROLLO, s->vista != V_DESARROLLO);
}

void entregar(Estado* s, bool rendirse) {
    if (s->abierta.esNulo() || s->tareaCorreccion.activa()) return;
    std::wstring r = rendirse ? L"" : textoRespuesta(s);
    if (!rendirse && contarPalabras(r) < 3) {
        s->mensaje = L"Escribe tu respuesta antes de entregarla (o pulsa «Ver la solución»).";
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }
    js::Valor p = js::Valor::objeto();
    p["pregunta"] = s->abierta["id"].comoCadena();
    p["respuesta"] = gui::aUtf8(r);
    s->respuestaEnviada = r;
    bool conIa = !rendirse && iaDisponible(s);
    int id = enviar(s, "corregir", p, !conIa, L"Corrigiendo…");
    if (conIa) {
        empezarTarea(s->tareaCorreccion, id);
        s->mensaje = L"Estoy corrigiendo tu respuesta con el apunte delante. Dame un minuto.";
        SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    }
    s->rectRespuesta = RECT();
    colocarRespuesta(s);
    s->scroll = 0;
}

}  // namespace

int pintarDesarrollo(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    s->rectRespuesta = RECT();

    // --- Preparando la pregunta
    if (s->tareaAbierta.activa()) {
        y += cabecera(s, dc, x, y, w, I_DESARROLLO, VIOLETA, L"Pregunta de desarrollo",
                      L"La IA está leyendo el apunte y redactando una pregunta como las de examen.");
        RECT r = rect(x, y, w, E(s, 230));
        cristal(dc, r, E(s, 18));
        spinner(s, dc, x + w / 2, y + E(s, 62), E(s, 24));
        int pct = std::min(99, s->tareaAbierta.caracteres * 100 / 700);
        std::wstring estado = s->tareaAbierta.caracteres ? L"Redactando…  " + std::to_wstring(pct) + L" %" : L"Leyendo el apunte…";
        texto(dc, s->f.subtitulo, tm.texto, estado, rect(x, y + E(s, 104), w, E(s, 28)), DT_CENTER | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.textoSuave, L"Tiempo: " + cronometro(s->tareaAbierta) + L"  ·  en este equipo tarda entre uno y dos minutos",
              rect(x, y + E(s, 136), w, E(s, 22)), DT_CENTER | DT_SINGLELINE);
        int bw = E(s, 240);
        boton(s, dc, rect(x + w / 2 - bw - E(s, 6), y + E(s, 172), bw, E(s, 40)), L"Usar una rápida sin IA", A_D_SIN_IA);
        boton(s, dc, rect(x + w / 2 + E(s, 6), y + E(s, 172), bw, E(s, 40)), L"Cancelar", A_D_CANCELAR, 0, FANTASMA);
        return r.bottom - a.y + E(s, 20);
    }

    // --- Inicio: elegir tema y dificultad
    if (s->abierta.esNulo()) {
        y += cabecera(s, dc, x, y, w, I_DESARROLLO, VIOLETA, L"Preguntas de desarrollo",
                      L"Como en el examen escrito: te hago una pregunta, la respondes con tus palabras y te la corrijo "
                      L"con la nota, las ideas clave que has cubierto, tus errores y una respuesta modelo.");
        RECT caja = rect(x, y, w, E(s, 0));
        int pad = E(s, 22);
        int cy = y + E(s, 18);
        const js::Valor& temas = s->desarrollo["temas"];
        int columnas = w >= E(s, 760) ? 2 : 1;
        int filaAlto = E(s, 40), subAlto = E(s, 40);
        int colAncho = (w - 2 * pad) / columnas;
        // Colocacion: cada tema con su subtitulo a todo lo ancho y sus apartados debajo en columnas.
        std::vector<FilaTema> filasT = filasPorTema(temas);
        std::vector<RECT> sitios;
        int oy = 0, col = 0;
        for (const FilaTema& f : filasT) {
            if (f.indice < 0) {
                if (col) oy += filaAlto, col = 0;
                sitios.push_back(rect(0, oy, columnas * colAncho - E(s, 8), subAlto));
                oy += subAlto;
                continue;
            }
            sitios.push_back(rect(col * colAncho, oy, colAncho - E(s, 8), filaAlto - E(s, 4)));
            if (++col == columnas) oy += filaAlto, col = 0;
        }
        if (col) oy += filaAlto;
        caja.bottom = y + E(s, 18 + 34 + 16 + 34 + 14) + E(s, 46) + oy + E(s, 18);
        cristal(dc, caja, E(s, 16));
        texto(dc, s->f.texto, tm.textoSuave, L"Dificultad", rect(x + pad, cy, E(s, 110), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int px = x + pad + E(s, 110);
        const char* difs[] = {"Básica", "Intermedia", "Avanzada"};
        const wchar_t* expl[] = {L"Definir y explicar", L"Explicar y relacionar", L"Comparar y aplicar"};
        for (int k = 0; k < 3; ++k)
            px += chip(s, dc, px, cy, W(std::string(difs[k])), s->desarrolloDificultad == difs[k], A_D_DIFICULTAD, k) + E(s, 8);
        for (int k = 0; k < 3; ++k)
            if (s->desarrolloDificultad == difs[k])
                texto(dc, s->f.peque, tm.textoSuave, expl[k], rect(px + E(s, 8), cy, x + w - px - pad, E(s, 34)),
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        cy += E(s, 50);
        texto(dc, s->f.texto, tm.textoSuave, L"Tema", rect(x + pad, cy, E(s, 110), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        chip(s, dc, x + pad + E(s, 110), cy, L"Automático: tus temas más flojos", s->desarrolloRuta.empty(), A_D_TEMA, -1);
        cy += E(s, 48);
        for (std::size_t k = 0; k < filasT.size(); ++k) {
            RECT r = sitios[k];
            OffsetRect(&r, x + pad, cy);
            if (filasT[k].indice < 0) {
                subtituloTema(s, dc, rect(r.left + E(s, 4), r.top + E(s, 2), r.right - r.left - E(s, 8), r.bottom - r.top - E(s, 4)),
                              filasT[k].grupo, plural(filasT[k].miembros, L"apartado", L"apartados"), VIOLETA);
                continue;
            }
            std::size_t i = (std::size_t)filasT[k].indice;
            const js::Valor& t = temas[i];
            bool elegido = t["ruta"].comoCadena() == s->desarrolloRuta;
            if (elegido) redondeado(dc, r, E(s, 10), mezcla(tm.panel, VIOLETA, tm.oscuro ? 0.22 : 0.14));
            else if (encima(s, A_D_TEMA, (int)i)) redondeado(dc, r, E(s, 10), tm.panelAlt);
            int cx = r.left + E(s, 18), cym = r.top + (r.bottom - r.top) / 2;
            circulo(dc, cx, cym, E(s, 8), elegido ? VIOLETA : tm.panel, elegido ? VIOLETA : tm.borde, 2.0f);
            if (elegido) circulo(dc, cx, cym, E(s, 3), RGB(255, 255, 255));
            std::wstring info;
            if (!t["precision"].esNulo()) info = std::to_wstring(t["precision"].comoEntero()) + L" %";
            int iw = info.empty() ? 0 : anchoTexto(dc, s->f.peque, info) + E(s, 10);
            texto(dc, s->f.texto, tm.texto, W(t["tema"]), rect(cx + E(s, 18), r.top, r.right - cx - E(s, 26) - iw, r.bottom - r.top),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (iw) {
                int p = t["precision"].comoEntero();
                texto(dc, s->f.peque, p >= 70 ? tm.ok : p >= 50 ? tm.medio : tm.mal, info, rect(r.right - iw - E(s, 6), r.top, iw, r.bottom - r.top),
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
            zona(s, r, A_D_TEMA, (int)i);
        }
        y = caja.bottom + E(s, 26);
        const js::Valor& intentos = s->desarrollo["intentos"];
        if (intentos.tam() > 0) {
            tituloSeccion(s, dc, x, y, w, L"Tus últimas respuestas");
            for (std::size_t i = 0; i < intentos.tam() && i < 8; ++i) {
                const js::Valor& it = intentos[i];
                double nota = it["nota"].comoNumero();
                RECT fila = rect(x, y, w, E(s, 56));
                cristal(dc, fila, E(s, 12));
                std::wstring n = numero(nota);
                COLORREF c = gui::colorDeNota(nota);
                redondeado(dc, rect(x + E(s, 14), y + E(s, 12), E(s, 52), E(s, 32)), E(s, 10), mezcla(tm.panel, c, 0.2), mezcla(tm.borde, c, 0.6));
                texto(dc, s->f.negrita, mezcla(c, tm.texto, 0.2), n, rect(x + E(s, 14), y + E(s, 12), E(s, 52), E(s, 32)),
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                texto(dc, s->f.texto, tm.texto, unaLinea(W(it["enunciado"])), rect(x + E(s, 82), y + E(s, 8), w - E(s, 100), E(s, 22)),
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                texto(dc, s->f.peque, tm.textoSuave, W(it["tema"]), rect(x + E(s, 82), y + E(s, 29), w - E(s, 100), E(s, 18)),
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                y += E(s, 62);
            }
        }
        return y - a.y;
    }

    const js::Valor& p = s->abierta;
    // --- Pregunta (comun a responder, corrigiendo y corregida)
    {
        int px = x;
        px += etiqueta(s, dc, px, y, W(p["tema"]), tm.acento) + E(s, 8);
        bool deIa = p["origen"].comoCadena() == "ia";
        px += etiqueta(s, dc, px, y, deIa ? L"Redactada por IA" : L"Pregunta rápida", deIa ? VIOLETA : tm.textoSuave) + E(s, 8);
        etiqueta(s, dc, px, y, std::to_wstring(p["n_puntos"].comoEntero()) + L" ideas clave", tm.textoSuave);
        y += E(s, 38);
        int pad = E(s, 26);
        std::wstring enun = W(p["enunciado"]);
        int h = altoTexto(dc, s->f.pregunta, enun, w - 2 * pad);
        RECT tarjeta = rect(x, y, w, h + 2 * pad);
        cristal(dc, tarjeta, E(s, 18));
        redondeado(dc, rect(x, y + E(s, 18), E(s, 4), h + 2 * pad - E(s, 36)), E(s, 2), VIOLETA);
        texto(dc, s->f.pregunta, tm.texto, enun, rect(x + pad, y + pad, w - 2 * pad, h));
        y = tarjeta.bottom + E(s, 18);
    }

    // --- Respondiendo: el campo nativo va dentro del recuadro
    if (respondiendo(s)) {
        // El campo llena lo que queda de pantalla (sin bajar del minimo ni crecer sin fin).
        int altoCampo = std::max(E(s, 170), std::min(E(s, 420), (int)s->cuerpo.bottom - y - E(s, 50)));
        RECT campo = rect(x, y, w, altoCampo);
        bool foco = GetFocus() == s->respuesta;
        redondeado(dc, campo, E(s, 16), tm.panel, foco ? VIOLETA : tm.borde, foco ? 2 : 1);
        s->rectRespuesta = {campo.left + E(s, 18), campo.top + E(s, 14), campo.right - E(s, 10), campo.bottom - E(s, 12)};
        y = campo.bottom + E(s, 10);
        int palabras = contarPalabras(textoRespuesta(s));
        std::wstring info = plural(palabras, L"palabra", L"palabras") + L"  ·  Ctrl+Intro para entregar  ·  apunta a 4-8 líneas";
        texto(dc, s->f.peque, tm.textoSuave, info, rect(x, y, w, E(s, 20)), DT_LEFT | DT_SINGLELINE);
        y += E(s, 30);
        return y - a.y;
    }

    // --- Corrigiendo
    if (s->tareaCorreccion.activa()) {
        int pad = E(s, 22);
        int h = altoTexto(dc, s->f.texto, s->respuestaEnviada, w - 2 * pad);
        RECT r = rect(x, y, w, h + 2 * pad + E(s, 26));
        cristal(dc, r, E(s, 16));
        texto(dc, s->f.mini, tm.textoSuave, L"TU RESPUESTA", rect(x + pad, y + pad - E(s, 4), w, E(s, 18)), DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.texto, s->respuestaEnviada, rect(x + pad, y + pad + E(s, 22), w - 2 * pad, h));
        y = r.bottom + E(s, 18);
        RECT c = rect(x, y, w, E(s, 120));
        cristal(dc, c, E(s, 16), CLR_INVALID, mezcla(tm.borde, VIOLETA, 0.5));
        spinner(s, dc, x + E(s, 50), y + E(s, 60), E(s, 18));
        texto(dc, s->f.subtitulo, tm.texto, L"Corrigiendo tu respuesta…", rect(x + E(s, 90), y + E(s, 30), w - E(s, 250), E(s, 28)),
              DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.textoSuave, L"Comparo con el apunte y las ideas clave  ·  " + cronometro(s->tareaCorreccion),
              rect(x + E(s, 90), y + E(s, 62), w - E(s, 250), E(s, 24)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        boton(s, dc, rect(c.right - E(s, 140), y + E(s, 40), E(s, 120), E(s, 40)), L"Cancelar", A_D_CANCELAR, 0, FANTASMA);
        return c.bottom - a.y + E(s, 20);
    }

    // --- Corregida
    const js::Valor& c = s->correccion;
    double nota = c["nota"].comoNumero();
    COLORREF color = gui::colorDeNota(nota);
    {
        int alto = E(s, 150);
        RECT r = rect(x, y, w, alto);
        cristal(dc, r, E(s, 18));
        int cx = x + E(s, 86), cy = y + alto / 2, radio = E(s, 50);
        arco(dc, cx, cy, radio, (float)E(s, 9), 0, 360, tm.panelAlt);
        if (nota > 0) arco(dc, cx, cy, radio, (float)E(s, 9), -90, (float)(36.0 * nota), color);
        texto(dc, s->f.titulo, tm.texto, numero(nota), rect(cx - radio, cy - E(s, 22), 2 * radio, E(s, 34)), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.mini, tm.textoSuave, L"sobre 10", rect(cx - radio, cy + E(s, 12), 2 * radio, E(s, 16)), DT_CENTER | DT_SINGLELINE);
        int tx = x + E(s, 170), tw = w - E(s, 190);
        bool rendido = s->respuestaEnviada.empty();
        texto(dc, s->f.titulo, color, rendido ? std::wstring(L"Solución") : calificacion(nota), rect(tx, y + E(s, 28), tw, E(s, 34)),
              DT_LEFT | DT_SINGLELINE);
        const js::Valor& puntos = c["puntos"];
        int cubiertos = 0;
        for (std::size_t i = 0; i < puntos.tam(); ++i) cubiertos += puntos[i].comoBool() ? 1 : 0;
        wchar_t b[200];
        swprintf(b, 200, L"Has cubierto %d de %d ideas clave", cubiertos, (int)puntos.tam());
        texto(dc, s->f.texto, tm.texto, b, rect(tx, y + E(s, 70), tw, E(s, 22)), DT_LEFT | DT_SINGLELINE);
        std::wstring origen = c["origen"].comoCadena() == "ia" ? L"Corregida por la IA con tu apunte"
                                                               : L"Corrección aproximada sin IA (por palabras clave)";
        if (c["intentos"].comoEntero() > 1)
            origen += L"  ·  intento " + std::to_wstring(c["intentos"].comoEntero()) + L", tu mejor nota: " + numero(c["mejor"].comoNumero());
        texto(dc, s->f.peque, tm.textoSuave, origen, rect(tx, y + E(s, 98), tw, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y = r.bottom + E(s, 16);
    }
    // Comentario del profesor
    std::wstring comentario = W(c["comentario"]);
    if (!comentario.empty()) {
        int bx = x + E(s, 60), bw = w - E(s, 60), pad = E(s, 18);
        int h = textoRico(s, dc, comentario, 0, 0, bw - 2 * pad, false);
        avatar(s, dc, x + E(s, 24), y + E(s, 24), E(s, 24));
        RECT r = rect(bx, y, bw, h + 2 * pad);
        redondeado(dc, r, E(s, 16), tm.panelAlt);
        textoRico(s, dc, comentario, bx + pad, y + pad, bw - 2 * pad);
        y = r.bottom + E(s, 18);
    }
    // Ideas clave
    {
        const js::Valor& puntos = c["puntos"];
        const js::Valor& textos = c["puntos_texto"];
        int pad = E(s, 22), iw = w - 2 * pad - E(s, 34);
        int h = E(s, 40);
        for (std::size_t i = 0; i < textos.tam(); ++i) h += std::max(E(s, 26), altoTexto(dc, s->f.texto, W(textos[i]), iw)) + E(s, 10);
        RECT r = rect(x, y, w, h + pad);
        cristal(dc, r, E(s, 16));
        texto(dc, s->f.subtitulo, tm.texto, L"Ideas clave", rect(x + pad, y + E(s, 16), w, E(s, 26)), DT_LEFT | DT_SINGLELINE);
        int iy = y + E(s, 54);
        for (std::size_t i = 0; i < textos.tam(); ++i) {
            bool ok = i < puntos.tam() && puntos[i].comoBool();
            std::wstring t = W(textos[i]);
            int th = std::max(E(s, 26), altoTexto(dc, s->f.texto, t, iw));
            marca(dc, x + pad + E(s, 11), iy + E(s, 12), E(s, 11), ok);
            texto(dc, s->f.texto, ok ? tm.texto : tm.textoSuave, t, rect(x + pad + E(s, 34), iy + E(s, 2), iw, th));
            iy += th + E(s, 10);
        }
        y = r.bottom + E(s, 16);
    }
    // Errores
    const js::Valor& errores = c["errores"];
    if (errores.tam() > 0) {
        std::wstring md;
        for (std::size_t i = 0; i < errores.tam(); ++i) md += L"- " + W(errores[i]) + L"\n";
        int pad = E(s, 22);
        int h = textoRico(s, dc, md, 0, 0, w - 2 * pad, false);
        RECT r = rect(x, y, w, h + pad + E(s, 50));
        redondeado(dc, r, E(s, 16), mezcla(tm.panel, tm.mal, tm.oscuro ? 0.10 : 0.06), mezcla(tm.borde, tm.mal, 0.5));
        texto(dc, s->f.subtitulo, tm.mal, L"Cuidado con esto", rect(x + pad, y + E(s, 16), w, E(s, 26)), DT_LEFT | DT_SINGLELINE);
        textoRico(s, dc, md, x + pad, y + E(s, 52), w - 2 * pad);
        y = r.bottom + E(s, 16);
    }
    // Tu respuesta y la respuesta modelo
    {
        int pad = E(s, 22);
        if (!s->respuestaEnviada.empty()) {
            int h = altoTexto(dc, s->f.texto, s->respuestaEnviada, w - 2 * pad);
            RECT r = rect(x, y, w, h + 2 * pad + E(s, 24));
            cristal(dc, r, E(s, 16));
            texto(dc, s->f.mini, tm.textoSuave, L"TU RESPUESTA", rect(x + pad, y + pad - E(s, 4), w, E(s, 18)), DT_LEFT | DT_SINGLELINE);
            texto(dc, s->f.texto, tm.textoSuave, s->respuestaEnviada, rect(x + pad, y + pad + E(s, 20), w - 2 * pad, h));
            y = r.bottom + E(s, 14);
        }
        std::wstring modelo = W(c["respuesta_modelo"]);
        std::wstring cita = W(c["cita"]);
        int h = altoTexto(dc, s->f.texto, modelo, w - 2 * pad);
        int hc = cita.empty() ? 0 : altoTexto(dc, s->f.peque, L"«" + cita + L"»", w - 2 * pad - E(s, 16)) + E(s, 16);
        RECT r = rect(x, y, w, h + hc + 2 * pad + E(s, 24) + E(s, 50));
        redondeado(dc, r, E(s, 16), mezcla(tm.panel, tm.ok, tm.oscuro ? 0.08 : 0.05), mezcla(tm.borde, tm.ok, 0.5));
        texto(dc, s->f.mini, tm.ok, L"RESPUESTA MODELO", rect(x + pad, y + pad - E(s, 4), w, E(s, 18)), DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.texto, modelo, rect(x + pad, y + pad + E(s, 20), w - 2 * pad, h));
        int iy = y + pad + E(s, 20) + h + E(s, 16);
        if (!cita.empty()) {
            int ch = hc - E(s, 16);
            redondeado(dc, rect(x + pad, iy, E(s, 3), ch), 1, mezcla(tm.panel, tm.ok, 0.6));
            texto(dc, s->f.peque, tm.textoSuave, L"«" + cita + L"»", rect(x + pad + E(s, 14), iy, w - 2 * pad - E(s, 16), ch));
            iy += hc;
        }
        texto(dc, s->f.peque, tm.textoSuave, L"Fuente: " + W(p["tema"]) + L"  ·  " + W(p["ubicacion"]), rect(x + pad, iy, w / 2, E(s, 34)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        botonIcono(s, dc, rect(r.right - pad - E(s, 170), iy, E(s, 170), E(s, 34)), I_LIBRO, L"Abrir el apunte", A_D_ABRIR, 0, FANTASMA);
        y = r.bottom + E(s, 16);
    }
    texto(dc, s->f.peque, tm.textoSuave, L"¿Pregunta mal planteada?", rect(x, y, w - E(s, 130), E(s, 30)), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    boton(s, dc, rect(x + w - E(s, 120), y, E(s, 120), E(s, 30)), L"Descartarla", A_D_DESCARTAR, 0, FANTASMA);
    y += E(s, 44);
    return y - a.y;
}

void pieDesarrollo(Estado* s, HDC dc, RECT pie) {
    int alto = E(s, 44), y = pie.top + (pie.bottom - pie.top - alto) / 2;
    if (s->tareaAbierta.activa() || s->tareaCorreccion.activa()) return;
    if (s->abierta.esNulo()) {
        bool ia = iaDisponible(s);
        botonIcono(s, dc, rect(pie.right - E(s, 250), y, E(s, 250), alto), ia ? I_CHISPA : I_DESARROLLO,
                   ia ? L"Nueva pregunta con IA" : L"Nueva pregunta", A_D_NUEVA, 0, PRIMARIO);
        if (ia) boton(s, dc, rect(pie.right - E(s, 480), y, E(s, 220), alto), L"Rápida, sin IA", A_D_SIN_IA);
        return;
    }
    if (respondiendo(s)) {
        boton(s, dc, rect(pie.left, y, E(s, 150), alto), L"←  Temas", A_D_VOLVER, 0, FANTASMA);
        boton(s, dc, rect(pie.left + E(s, 160), y, E(s, 170), alto), L"Ver la solución", A_D_RENDIRSE, 0, FANTASMA);
        boton(s, dc, rect(pie.right - E(s, 430), y, E(s, 200), alto), L"Otra pregunta", A_D_OTRA);
        boton(s, dc, rect(pie.right - E(s, 220), y, E(s, 220), alto), L"Entregar  →", A_D_ENTREGAR, 0, PRIMARIO);
        return;
    }
    boton(s, dc, rect(pie.left, y, E(s, 150), alto), L"←  Temas", A_D_VOLVER, 0, FANTASMA);
    if (iaDisponible(s))
        botonIcono(s, dc, rect(pie.left + E(s, 160), y, E(s, 210), alto), I_DUDAS, L"Tengo una duda", A_D_DUDA, 0, SECUNDARIO);
    boton(s, dc, rect(pie.right - E(s, 410), y, E(s, 180), alto), L"Repetirla", A_D_REPETIR);
    boton(s, dc, rect(pie.right - E(s, 220), y, E(s, 220), alto), L"Otra pregunta  →", A_D_OTRA, 0, PRIMARIO);
}

void colocarRespuesta(Estado* s) {
    if (!s->respuesta) return;
    RECT r;
    IntersectRect(&r, &s->rectRespuesta, &s->cuerpo);  // nunca encima de la barra o del pie
    bool ver = s->vista == V_DESARROLLO && respondiendo(s) && r.bottom - r.top > 20 && !s->ocupado && s->zoom.empty() && !hayCapa(s);
    if (ver) {
        RECT actual;
        GetWindowRect(s->respuesta, &actual);
        MapWindowPoints(NULL, s->ventana, (POINT*)&actual, 2);
        if (!EqualRect(&actual, &r)) MoveWindow(s->respuesta, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
        if (!IsWindowVisible(s->respuesta)) {
            ShowWindow(s->respuesta, SW_SHOW);
            SetFocus(s->respuesta);
        }
    } else if (IsWindowVisible(s->respuesta)) {
        ShowWindow(s->respuesta, SW_HIDE);
    }
}

// ================================================================= analisis

int pintarAnalisis(Estado* s, HDC dc, int x, int y, int w) {
    const gui::Tema& tm = gui::tema();
    int pad = E(s, 24);
    if (s->tareaAnalisis.activa())
        return cajaEscribiendo(s, dc, x, y, w, s->tareaAnalisis, L"El profesor está analizando tu progreso",
                               L"Repasando tus resultados, tus temas y tus fallos…", A_ANALISIS_CANCELAR);
    std::wstring md = W(s->analisis["texto"]);
    bool ia = iaDisponible(s);
    if (md.empty()) {
        RECT r = rect(x, y, w, E(s, 112));
        {
            Gdiplus::Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath camino;
            caminoRedondeado(camino, r, E(s, 18));
            Gdiplus::LinearGradientBrush fondo(Gdiplus::PointF((float)r.left, 0), Gdiplus::PointF((float)r.right, 0),
                                               gc(mezcla(tm.panel, VIOLETA, tm.oscuro ? 0.22 : 0.14)),
                                               gc(mezcla(tm.panel, tm.acento, tm.oscuro ? 0.12 : 0.08)));
            g.FillPath(&fondo, &camino);
        }
        contorno(dc, r, E(s, 18), mezcla(tm.borde, VIOLETA, 0.5));
        avatar(s, dc, x + pad + E(s, 28), y + E(s, 56), E(s, 28));
        int tx = x + pad + E(s, 74), tw = w - (tx - x) - E(s, 270);
        texto(dc, s->f.subtitulo, tm.texto, L"¿Cómo lo llevas?", rect(tx, y + E(s, 26), tw, E(s, 26)), DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.textoSuave,
              ia ? L"Pídeme un análisis de tu progreso: tus puntos débiles y un plan hasta el examen."
                 : L"Te hago un diagnóstico de tus puntos débiles y un plan hasta el examen.",
              rect(tx, y + E(s, 56), tw, E(s, 40)), DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        botonIcono(s, dc, rect(r.right - pad - E(s, 230), y + E(s, 36), E(s, 230), E(s, 42)), I_CHISPA, L"Analizar mi progreso",
                   A_ANALIZAR, 0, PRIMARIO);
        return r.bottom - y;
    }
    // Antiguedad del analisis: los datos cambian cada dia y un diagnostico viejo confunde
    // (puede citar una precision que ya no es la de las cifras de arriba).
    double momento = s->analisis["momento"].comoNumero();
    int diasAnalisis = momento > 0 ? (int)((double)time(NULL) - momento) / 86400 : -1;
    bool viejo = diasAnalisis >= 3;
    int aviso = viejo ? E(s, 46) : 0;
    int h = textoRico(s, dc, md, 0, 0, w - 2 * pad, false);
    RECT r = rect(x, y, w, h + 2 * pad + E(s, 50) + aviso);
    cristal(dc, r, E(s, 18), CLR_INVALID, mezcla(tm.borde, VIOLETA, 0.45));
    avatar(s, dc, x + pad + E(s, 16), y + pad + E(s, 14), E(s, 16));
    std::wstring origen = s->analisis["origen"].comoCadena() == "ia" ? L"Análisis del profesor (IA)" : L"Análisis del profesor";
    int tx = x + pad + E(s, 44);
    texto(dc, s->f.subtitulo, tm.texto, origen, rect(tx, y + pad, w - 2 * pad - E(s, 200), E(s, 30)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (diasAnalisis >= 0) {
        std::wstring cuando = diasAnalisis == 0 ? L"hecho hoy" : diasAnalisis == 1 ? L"hecho ayer"
                                                                  : L"hecho hace " + std::to_wstring(diasAnalisis) + L" días";
        int ancho = anchoTexto(dc, s->f.subtitulo, origen);
        etiqueta(s, dc, tx + ancho + E(s, 12), y + pad + E(s, 4), cuando, viejo ? tm.medio : tm.textoSuave);
    }
    botonIcono(s, dc, rect(r.right - pad - E(s, 150), y + pad - E(s, 2), E(s, 150), E(s, 34)), I_REPETIR, L"Actualizar", A_ANALIZAR, 1,
               viejo ? PRIMARIO : FANTASMA);
    if (viejo) {
        RECT banda = rect(x + pad, y + pad + E(s, 46), w - 2 * pad, E(s, 36));
        redondeado(dc, banda, E(s, 10), mezcla(tm.panel, tm.medio, tm.oscuro ? 0.14 : 0.10));
        icono(s, dc, I_RELOJ, banda.left + E(s, 20), banda.top + E(s, 18), E(s, 16), tm.medio);
        texto(dc, s->f.texto, tm.texto,
              L"Es de hace unos días y tus resultados han cambiado desde entonces: pulsa «Actualizar» para verlo al día.",
              rect(banda.left + E(s, 40), banda.top, banda.right - banda.left - E(s, 52), E(s, 36)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    textoRico(s, dc, md, x + pad, y + pad + E(s, 50) + aviso, w - 2 * pad);
    return r.bottom - y;
}

// ======================================================== mensajes y acciones

bool tienePie(Estado* s, Vista v) {
    if (v == V_TARJETAS) return !s->tarjetasFin;
    if (v == V_DESARROLLO) return !s->tareaAbierta.activa() && !s->tareaCorreccion.activa();
    return false;
}

bool animandoEstudio(Estado* s) {
    return (s->volteada && GetTickCount64() - s->volteo < 320) || s->truco.activa() || s->tareaResumen.activa() ||
           s->tareaAbierta.activa() || s->tareaCorreccion.activa() || s->tareaAnalisis.activa() ||
           !s->resumenesFondo.esNulo();
}

void abrirEstudio(Estado* s, Vista v) {
    switch (v) {
        case V_HUB:
            enviar(s, "inicio");
            irAVista(s, V_HUB);
            break;
        case V_TARJETAS:
            if (s->tarjetasFin) {
                s->tarjetasFin = false;
                s->tarjetas = js::Valor();
            }
            irAVista(s, V_TARJETAS);
            break;
        case V_RESUMENES:
        case V_DESARROLLO: {
            bool primera = v == V_RESUMENES ? s->resumenes.esNulo() : s->desarrollo.esNulo();
            if (primera) s->abrirAlLlegar = v;  // la vista se abre al llegar los datos
            enviar(s, v == V_RESUMENES ? "resumenes" : "desarrollo", js::Valor::objeto(), primera,
                   v == V_RESUMENES ? L"Reuniendo tus temas…" : L"Preparando las preguntas de desarrollo…");
            if (!primera) irAVista(s, v);
            break;
        }
        default: break;
    }
}

bool estudioRespuesta(Estado* s, const std::string& orden, const js::Valor& r) {
    int id = r["id"].comoEntero();
    if (orden == "inicio" || orden == "examenes") {
        s->inicio = r;
        if (r.contiene("resumen")) s->hola["resumen"] = r["resumen"];
        return true;
    }
    if (orden == "tarjetas") {
        s->tarjetas = r["tarjetas"];
        s->mazo = r["mazo"];
        s->tarjeta = 0;
        s->volteada = false;
        s->tarjetasBien = s->tarjetasOtra = 0;
        s->tarjetasFin = false;
        if (s->tarjetas.tam() == 0) s->mensaje = L"No te toca repasar ninguna tarjeta. ¡Vas al día!";
        else s->mensaje = L"Lee la tarjeta, intenta recordar la respuesta y luego gírala. Sé sincero al puntuarte.";
        irAVista(s, V_TARJETAS, s->vista != V_TARJETAS);
        return true;
    }
    if (orden == "tarjeta") {
        if (r.contiene("resumen")) s->hola["resumen"] = r["resumen"];
        return true;
    }
    if (orden == "truco") {
        s->trucos[r["tarjeta"].comoCadena()] = W(r["texto"]);
        acabarTarea(s->truco);
        s->mensaje = L"Ahí tienes un truco. Repítelo en voz alta un par de veces.";
        return true;
    }
    if (orden == "resumenes") {
        s->resumenes = r;
        if (!r["trabajando"].comoBool()) s->resumenesFondo = js::Valor();
        if (s->abrirAlLlegar != V_RESUMENES) return true;  // solo era para refrescar la lista
        s->abrirAlLlegar = V_ARRANCANDO;
        irAVista(s, V_RESUMENES);
        if (s->resumenRuta.empty() && s->resumenes["temas"].tam() > 0) {
            // Primer tema con resumen, para no empezar con la pantalla vacia.
            const js::Valor& temas = s->resumenes["temas"];
            for (std::size_t i = 0; i < temas.tam(); ++i) {
                if (temas[i]["estado"].comoCadena() == "ia" || temas[i]["estado"].comoCadena() == "auto") {
                    pedirResumen(s, temas[i]["ruta"].comoCadena(), false, true);
                    break;
                }
            }
            if (s->resumenRuta.empty()) s->resumenRuta = temas[0]["ruta"].comoCadena();
        } else if (s->resumen["ruta"].comoCadena() != s->resumenRuta) {
            // Se pidio un tema concreto (paleta de comandos) antes de tener la lista: se carga ahora
            const js::Valor& temas = s->resumenes["temas"];
            for (std::size_t i = 0; i < temas.tam(); ++i) {
                std::string estado = temas[i]["estado"].comoCadena();
                if (temas[i]["ruta"].comoCadena() == s->resumenRuta && (estado == "ia" || estado == "auto")) {
                    js::Valor p = js::Valor::objeto();
                    p["ruta"] = s->resumenRuta;
                    p["ia"] = estado == "ia";
                    enviar(s, "resumen", p);
                }
            }
        }
        if (s->verMapa) herramientasAccion(s, A_R_VISTA, 1);  // se llego desde «Mapas mentales»
        s->mensaje = L"Elige un tema para leer su resumen. Leer el resumen antes de hacer un test ayuda mucho.";
        return true;
    }
    if (orden == "resumen") {
        if (id == s->tareaResumen.id) acabarTarea(s->tareaResumen);
        s->resumen = r;
        ponerEstadoResumen(s, r["ruta"].comoCadena(), r["origen"].comoCadena());
        if (r["cancelado"].comoBool()) s->mensaje = L"He parado. Te dejo el resumen que ya había.";
        else if (r["origen"].comoCadena() == "ia") s->mensaje = L"Aquí tienes el resumen. Léelo con calma y luego ponte a prueba con un test del tema.";
        else s->mensaje = L"Resumen automático listo: son las frases más importantes del apunte.";
        if (s->vista != V_RESUMENES && r["origen"].comoCadena() == "ia") s->mensaje = L"¡El resumen de «" + W(r["tema"]) + L"» ya está listo!";
        return true;
    }
    if (orden == "resumir_todos") {
        if (r["trabajando"].comoBool()) {
            s->resumenesFondo = js::Valor::objeto();
            s->resumenesFondo["hechos"] = 0;
            s->resumenesFondo["total"] = r["pendientes"].comoEntero(1);
            s->mensaje = L"Voy resumiendo tus temas uno a uno en segundo plano. Puedes seguir estudiando mientras tanto.";
            SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
        } else {
            s->mensaje = L"Todos tus temas ya tienen su resumen de la IA.";
        }
        return true;
    }
    if (orden == "detener_resumenes") return true;
    if (orden == "desarrollo") {
        s->desarrollo = r;
        if (s->abrirAlLlegar == V_DESARROLLO) {
            s->abrirAlLlegar = V_ARRANCANDO;
            s->mensaje = L"Explicar con tus palabras es la mejor forma de comprobar que lo sabes. Elige tema y dificultad.";
            irAVista(s, V_DESARROLLO);
        }
        return true;
    }
    if (orden == "abierta") {
        if (id == s->tareaAbierta.id) acabarTarea(s->tareaAbierta);
        s->abierta = r["pregunta"];
        s->correccion = js::Valor();
        s->respuestaEnviada.clear();
        SetWindowTextW(s->respuesta, L"");
        s->scroll = 0;
        s->mensaje = L"Tómate tu tiempo. Escribe como en el examen: define, explica y, si puedes, pon un ejemplo.";
        if (s->vista != V_DESARROLLO) irAVista(s, V_DESARROLLO);
        return true;
    }
    if (orden == "corregir") {
        if (id == s->tareaCorreccion.id) acabarTarea(s->tareaCorreccion);
        s->correccion = r["correccion"];
        s->abierta = r["pregunta"];
        s->scroll = 0;
        double nota = s->correccion["nota"].comoNumero();
        if (s->respuestaEnviada.empty()) s->mensaje = L"Estudia la respuesta modelo y vuelve a intentarlo más tarde sin mirarla.";
        else if (nota >= 9) s->mensaje = L"¡Respuesta de matrícula! Lo tienes dominado.";
        else if (nota >= 7) s->mensaje = L"Muy buena respuesta. Fíjate en lo que te ha faltado para redondearla.";
        else if (nota >= 5) s->mensaje = L"Aprobada, pero se puede mejorar. Compara con la respuesta modelo.";
        else s->mensaje = L"Esta hay que trabajarla. Relee el apunte y repítela dentro de un rato.";
        enviar(s, "desarrollo");  // actualiza el historial
        return true;
    }
    if (orden == "descartar_abierta") {
        s->abierta = js::Valor();
        s->correccion = js::Valor();
        s->mensaje = W(r["mensaje"]);
        return true;
    }
    if (orden == "analisis") {
        if (id == s->tareaAnalisis.id) acabarTarea(s->tareaAnalisis);
        s->analisis = r;
        if (!W(r["texto"]).empty()) s->mensaje = L"Este es mi diagnóstico. Sigue el plan y en unos días vuelve a pedirme otro.";
        return true;
    }
    if (orden == "nueva_conversacion") {
        s->chat.clear();
        s->contextoIndice = -1;
        s->contextoPregunta.clear();
        s->mensaje = L"Conversación nueva. ¿Qué quieres que te explique?";
        return true;
    }
    return false;
}

bool estudioError(Estado* s, const std::string& orden, const std::wstring& m) {
    if (orden == "resumen") acabarTarea(s->tareaResumen);
    else if (orden == "abierta") acabarTarea(s->tareaAbierta);
    else if (orden == "corregir") acabarTarea(s->tareaCorreccion);
    else if (orden == "analisis") acabarTarea(s->tareaAnalisis);
    else if (orden == "truco") acabarTarea(s->truco);
    else if (orden != "resumir_todos" && orden != "tarjetas" && orden != "desarrollo" && orden != "resumenes") return false;
    s->mensaje = m;
    if (orden == "tarjetas" || orden == "resumir_todos") gui::avisar(s->ventana, m);
    return true;
}

bool estudioEvento(Estado* s, const js::Valor& ev) {
    std::string tipo = ev["evento"].comoCadena();
    if (tipo == "parcial") {
        int id = ev["id"].comoEntero();
        TareaIA* t = NULL;
        for (TareaIA* c : {&s->tareaResumen, &s->tareaAbierta, &s->tareaCorreccion, &s->tareaAnalisis, &s->truco})
            if (c->activa() && c->id == id) t = c;
        if (t) {
            t->parcial = W(ev["texto"]);
            t->caracteres = ev["caracteres"].comoEntero();
        } else if (ev["tipo"].comoCadena() == "resumen" && !s->resumenesFondo.esNulo() &&
                   ev["ruta"].comoCadena() == s->resumenesFondo["ruta"].comoCadena()) {
            s->resumenesFondo["parcial"] = ev["texto"].comoCadena();
        }
        return true;
    }
    if (tipo == "resumenes") {
        if (ev["estado"].comoCadena() == "terminado") {
            s->resumenesFondo = js::Valor();
            s->mensaje = L"He terminado de resumir tus temas.";
            enviar(s, "resumenes");
        } else {
            std::string parcial = s->resumenesFondo["ruta"].comoCadena() == ev["ruta"].comoCadena()
                                      ? s->resumenesFondo["parcial"].comoCadena() : "";
            s->resumenesFondo = ev;
            s->resumenesFondo["parcial"] = parcial;
            SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
        }
        return true;
    }
    if (tipo == "resumen_listo") {
        std::string ruta = ev["ruta"].comoCadena();
        ponerEstadoResumen(s, ruta, "ia");
        if (ruta == s->resumenRuta && !s->tareaResumen.activa()) {
            js::Valor p = js::Valor::objeto();
            p["ruta"] = ruta;
            enviar(s, "resumen", p);
        }
        return true;
    }
    if (tipo == "imagen_tarjeta") {
        js::Valor nuevas = js::Valor::array();
        for (std::size_t i = 0; i < s->tarjetas.tam(); ++i) {
            js::Valor t = s->tarjetas[i];
            if (t["id"].comoCadena() == ev["tarjeta"].comoCadena()) t["imagen"] = ev["imagen"];
            nuevas.agregar(t);
        }
        s->tarjetas = nuevas;
        return true;
    }
    return false;
}

bool estudioAccion(Estado* s, int accion, int dato) {
    HWND w = s->ventana;
    switch (accion) {
        case A_PLAN: {
            const js::Valor& t = s->inicio["plan"][(std::size_t)dato];
            std::string tipo = t["tipo"].comoCadena();
            if (tipo == "tarjetas") {
                irAVista(s, V_TARJETAS);
                empezarTarjetas(s, t["hecha"].comoBool());
            } else if (tipo == "repaso") {
                estudioAccion(s, A_REPASO, 0);
            } else if (tipo == "tema") {
                js::Valor c = configParaEnviar(s);
                js::Valor temas = js::Valor::array();
                temas.agregar(t["ruta"].comoCadena());
                c["temas"] = temas;
                empezarCon(s, c, L"Preparando un test de tu tema más flojo…");
            } else if (tipo == "resumen") {
                s->resumenRuta = t["ruta"].comoCadena();
                if (s->resumenes.esNulo()) {
                    s->abrirAlLlegar = V_RESUMENES;
                    enviar(s, "resumenes", js::Valor::objeto(), true, L"Reuniendo tus temas…");
                } else {
                    irAVista(s, V_RESUMENES);
                }
                pedirResumen(s, s->resumenRuta, false, true);
            } else if (tipo == "simulacro") {
                estudioAccion(s, A_SIMULACRO, 0);
            } else if (tipo == "desarrollo") {
                abrirEstudio(s, V_DESARROLLO);
            } else {
                abrirVista(s, V_INICIO);
            }
            return true;
        }
        case A_SIMULACRO: {
            js::Valor c = configParaEnviar(s);
            c["simulacro"] = true;
            c["n"] = 20;
            c["correccion"] = "final";
            c["penalizar"] = true;
            c["limite"] = 60;
            empezarCon(s, c, L"Preparando tu simulacro de examen…");
            return true;
        }
        case A_REPASO: {
            js::Valor c = configParaEnviar(s);
            c["repaso"] = true;
            c["modo"] = "rapido";
            empezarCon(s, c, L"Reuniendo las preguntas que toca repasar…");
            return true;
        }
        case A_EXPLICAR_FALLO: {
            if (!exigirIa(s)) return true;
            const js::Valor& f = s->fallos[(std::size_t)dato];
            s->contextoIndice = -1;
            s->contextoPregunta = f["id"].comoCadena();
            s->contextoEtiqueta = L"Sobre una pregunta de «Mis fallos»";
            enviarDuda(s, L"Fallé esta pregunta. Explícame por qué la respuesta correcta es la buena y en qué me he podido confundir.");
            return true;
        }
        // --- tarjetas
        case A_T_EMPEZAR: empezarTarjetas(s, false); return true;
        case A_T_EXTRA: empezarTarjetas(s, true); return true;
        case A_T_CANTIDAD: s->nTarjetas = dato; return true;
        case A_T_VOLTEAR: voltear(s); return true;
        case A_T_NOTA: calificarTarjeta(s, dato); return true;
        case A_T_TERMINAR:
            s->tarjetasFin = true;
            s->mensaje = L"Repaso terminado. Las que no has visto siguen esperándote.";
            enviar(s, "inicio");
            return true;
        case A_T_TRUCO: {
            if (!enSesionDeTarjetas(s) || s->truco.activa()) return true;
            const js::Valor& t = tarjetaActual(s);
            js::Valor p = js::Valor::objeto();
            p["tarjeta"] = t["id"].comoCadena();
            p["frente"] = t["frente"].comoCadena();
            p["reverso"] = t["reverso"].comoCadena();
            p["rehacer"] = dato == 1;
            s->trucoTarjeta = t["id"].comoCadena();
            s->trucos.erase(s->trucoTarjeta);
            empezarTarea(s->truco, enviar(s, "truco", p));
            SetTimer(w, TIMER_ANIMACION, 50, NULL);
            return true;
        }
        case A_T_DUDA: {
            if (!enSesionDeTarjetas(s) || !exigirIa(s)) return true;
            const js::Valor& t = tarjetaActual(s);
            s->contextoIndice = -1;
            s->contextoPregunta = "Tarjeta de estudio. Pregunta: " + t["frente"].comoCadena() + "\nRespuesta: " + t["reverso"].comoCadena();
            s->contextoEtiqueta = L"Sobre una tarjeta";
            enviarDuda(s, L"Explícame esta tarjeta con un ejemplo para que la entienda y no solo la memorice.");
            return true;
        }
        case A_T_ABRIR:
            if (enSesionDeTarjetas(s) && !gui::abrirEnSistema(w, W(tarjetaActual(s)["archivo"])))
                s->mensaje = L"No he podido abrir el documento.";
            return true;
        case A_T_ZOOM:
            if (enSesionDeTarjetas(s)) {
                s->zoom = W(tarjetaActual(s)["imagen"]["archivo"]);
                s->zoomPie = W(tarjetaActual(s)["tema"]) + L"  ·  " + W(tarjetaActual(s)["imagen"]["pie"]);
            }
            return true;
        // --- resumenes
        case A_R_TEMA: {
            const js::Valor& t = s->resumenes["temas"][(std::size_t)dato];
            std::string ruta = t["ruta"].comoCadena();
            if (ruta == s->resumenRuta && s->resumen["ruta"].comoCadena() == ruta) return true;
            s->resumenRuta = ruta;
            s->scroll = 0;
            if (s->verMapa) herramientasAccion(s, A_R_VISTA, 1);
            std::string estado = t["estado"].comoCadena();
            if (estado == "ia" || estado == "auto") {
                js::Valor p = js::Valor::objeto();
                p["ruta"] = ruta;
                p["ia"] = estado == "ia";
                enviar(s, "resumen", p);
            }
            return true;
        }
        case A_R_GENERAR:
            if (!s->resumenRuta.empty()) pedirResumen(s, s->resumenRuta, dato == 1, dato != 2);
            return true;
        case A_R_TODOS: enviar(s, "resumir_todos"); return true;
        case A_R_DETENER:
            enviar(s, "detener_resumenes");
            s->mensaje = L"Paro en cuanto termine de escribir… o ahora mismo, si estaba leyendo.";
            return true;
        case A_R_CANCELAR: {
            js::Valor p = js::Valor::objeto();
            p["tarea"] = s->tareaResumen.id;
            enviar(s, "cancelar", p);
            return true;
        }
        case A_R_TEST: {
            js::Valor c = configParaEnviar(s);
            js::Valor temas = js::Valor::array();
            temas.agregar(s->resumenRuta);
            c["temas"] = temas;
            empezarCon(s, c, L"Preparando un test de este tema…");
            return true;
        }
        case A_R_ABRIR: {
            const js::Valor& temas = s->hola["archivos"];
            std::wstring carpeta = W(carpetaDeAsignatura(s->asignatura, true));
            for (std::size_t i = 0; i < temas.tam(); ++i)
                if (temas[i]["ruta"].comoCadena() == s->resumenRuta) {
                    std::wstring ruta = W(s->resumenRuta);
                    for (wchar_t& c : ruta) if (c == L'/') c = L'\\';
                    if (!gui::abrirEnSistema(w, carpeta + L"\\" + ruta)) s->mensaje = L"No he podido abrir el documento.";
                }
            return true;
        }
        case A_R_COPIAR:
            if (copiar(w, sinFormato(W(s->resumen["texto"])))) s->mensaje = L"Resumen copiado. Pégalo en tus apuntes.";
            return true;
        // --- desarrollo
        case A_D_TEMA:
            s->desarrolloRuta = dato < 0 ? "" : s->desarrollo["temas"][(std::size_t)dato]["ruta"].comoCadena();
            return true;
        case A_D_DIFICULTAD: {
            const char* d[] = {"Básica", "Intermedia", "Avanzada"};
            s->desarrolloDificultad = d[dato];
            return true;
        }
        case A_D_NUEVA: pedirAbierta(s, true, false); return true;
        case A_D_SIN_IA:
            if (s->tareaAbierta.activa()) {
                js::Valor p = js::Valor::objeto();
                p["tarea"] = s->tareaAbierta.id;
                enviar(s, "cancelar", p);
                acabarTarea(s->tareaAbierta);
            }
            pedirAbierta(s, false, true);
            return true;
        case A_D_OTRA:
            if (!s->abierta.esNulo()) s->desarrolloRuta = s->desarrolloRuta.empty() ? "" : s->abierta["ruta"].comoCadena();
            pedirAbierta(s, true, false);
            return true;
        case A_D_ENTREGAR: entregar(s, false); return true;
        case A_D_RENDIRSE:
            if (gui::preguntar(w, L"¿Ver la solución sin responder? Te enseño la respuesta modelo y las ideas clave."))
                entregar(s, true);
            return true;
        case A_D_REPETIR:
            s->correccion = js::Valor();
            s->respuestaEnviada.clear();
            SetWindowTextW(s->respuesta, L"");
            s->scroll = 0;
            s->mensaje = L"Otra vez, ahora sin mirar la respuesta modelo.";
            return true;
        case A_D_VOLVER:
            s->abierta = js::Valor();
            s->correccion = js::Valor();
            s->scroll = 0;
            return true;
        case A_D_CANCELAR: {
            TareaIA& t = s->tareaAbierta.activa() ? s->tareaAbierta : s->tareaCorreccion;
            js::Valor p = js::Valor::objeto();
            p["tarea"] = t.id;
            enviar(s, "cancelar", p);
            return true;
        }
        case A_D_DUDA: {
            if (!exigirIa(s)) return true;
            s->contextoIndice = -1;
            s->contextoPregunta = "Pregunta de desarrollo: " + s->abierta["enunciado"].comoCadena() +
                                  "\nRespuesta modelo: " + s->correccion["respuesta_modelo"].comoCadena();
            s->contextoEtiqueta = L"Sobre la pregunta de desarrollo";
            enviarDuda(s, L"No acabo de entender esta pregunta de desarrollo. ¿Me explicas la respuesta paso a paso?");
            return true;
        }
        case A_D_ABRIR:
            if (!gui::abrirEnSistema(w, W(s->abierta["archivo"]))) s->mensaje = L"No he podido abrir el documento.";
            return true;
        case A_D_DESCARTAR:
            if (gui::preguntar(w, L"¿Descartar esta pregunta? No volverá a salir.")) {
                js::Valor p = js::Valor::objeto();
                p["pregunta"] = s->abierta["id"].comoCadena();
                enviar(s, "descartar_abierta", p);
            }
            return true;
        // --- analisis
        case A_ANALIZAR: {
            if (s->tareaAnalisis.activa()) return true;
            js::Valor p = js::Valor::objeto();
            p["rehacer"] = true;
            bool ia = iaDisponible(s);
            int id = enviar(s, "analisis", p, !ia, L"Analizando tu progreso…");
            if (ia) {
                empezarTarea(s->tareaAnalisis, id);
                s->mensaje = L"Dame un momento: estoy repasando tus resultados.";
                SetTimer(w, TIMER_ANIMACION, 50, NULL);
            }
            return true;
        }
        case A_ANALISIS_CANCELAR: {
            js::Valor p = js::Valor::objeto();
            p["tarea"] = s->tareaAnalisis.id;
            enviar(s, "cancelar", p);
            return true;
        }
        default: return false;
    }
}

bool estudioTecla(Estado* s, WPARAM tecla) {
    if (s->vista == V_TARJETAS && enSesionDeTarjetas(s) && !s->ocupado && s->zoom.empty()) {
        if (tecla == VK_SPACE || (tecla == VK_RETURN && !s->volteada)) {
            voltear(s);
            InvalidateRect(s->ventana, NULL, FALSE);
            return true;
        }
        if (s->volteada && (tecla == '1' || tecla == VK_NUMPAD1)) calificarTarjeta(s, 0);
        else if (s->volteada && (tecla == '2' || tecla == VK_NUMPAD2 || tecla == VK_RETURN)) calificarTarjeta(s, 1);
        else if (s->volteada && (tecla == '3' || tecla == VK_NUMPAD3)) calificarTarjeta(s, 2);
        else return false;
        return true;
    }
    return false;
}

}  // namespace ui
}  // namespace profesor
