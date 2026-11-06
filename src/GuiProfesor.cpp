// GuiProfesor.cpp - Ventana del profesor: navegacion, tests, progreso y dudas.
//
// Toda la ventana se dibuja a mano (doble bufer, GDI para el texto y GDI+
// para las formas suavizadas), incluida la barra de titulo: la de Windows se
// quita en WM_NCCALCSIZE y la propia lleva el boton de volver, el estado de la
// IA y los botones de ventana. La navegacion va en el lateral, bajo el
// profesor. Cada pintado registra "zonas" clicables; el raton y el teclado se
// resuelven contra esas zonas. La logica (preguntas, correccion, progreso,
// dudas) vive en profesor/servidor.py, con el que se habla mediante
// ProfesorMotor. Las herramientas de estudio (inicio, tarjetas, resumenes,
// desarrollo) estan en GuiProfesorEstudio.cpp.
#include "GuiProfesor.hpp"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "ProfesorUi.hpp"
#include "Musica.hpp"
#include "Rutas.hpp"

namespace profesor {
namespace ui {

const wchar_t* kClase = L"UniversalisProfesor";

// ================================================================ utilidades

std::wstring W(const std::string& s) { return gui::aW(s); }
std::wstring W(const js::Valor& v) { return gui::aW(v.comoCadena()); }

int E(Estado* s, int px) { return (int)std::lround(px * s->factor); }

Gdiplus::Color gc(COLORREF c, int alfa) {
    return Gdiplus::Color((BYTE)alfa, GetRValue(c), GetGValue(c), GetBValue(c));
}

COLORREF mezcla(COLORREF a, COLORREF b, double t) { return gui::mezclar(a, b, t); }

void caminoRedondeado(Gdiplus::GraphicsPath& p, const RECT& r, int radio) {
    radio = gui::radioEstilo(radio);
    float x = (float)r.left, y = (float)r.top, w = (float)(r.right - r.left), h = (float)(r.bottom - r.top);
    float d = (float)std::min<int>(radio * 2, (int)std::min(w, h));
    if (d <= 1) {
        p.AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d - 1, y, d, d, 270, 90);
    p.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    p.AddArc(x, y + h - d - 1, d, d, 90, 90);
    p.CloseFigure();
}

void redondeado(HDC dc, const RECT& r, int radio, COLORREF fondo, COLORREF borde, int grosor) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath p;
    caminoRedondeado(p, r, radio);
    Gdiplus::SolidBrush b(gc(fondo));
    g.FillPath(&b, &p);
    if (borde != CLR_INVALID) {
        Gdiplus::Pen lapiz(gc(borde), (float)grosor);
        g.DrawPath(&lapiz, &p);
    }
}

// Tarjeta "de cristal": un poco translucida sobre el fondo, con un leve brillo arriba,
// borde fino y (en el tema claro) una sombra suave.
void cristal(HDC dc, const RECT& r, int radio, COLORREF base, COLORREF borde, int grosor) {
    const gui::Tema& tm = gui::tema();
    if (base == CLR_INVALID) base = tm.panel;
    if (borde == CLR_INVALID) borde = tm.borde;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if (!gui::estilo().cristal) {  // tarjetas planas: sin brillo, sombra ni transparencia
        redondeado(dc, r, radio, base, borde, grosor);
        return;
    }
    if (!tm.oscuro) {
        for (int i = 3; i >= 1; --i) {
            RECT sr = r;
            OffsetRect(&sr, 0, i * 2);
            InflateRect(&sr, i, i);
            Gdiplus::GraphicsPath ps;
            caminoRedondeado(ps, sr, radio + i);
            Gdiplus::SolidBrush sombra(Gdiplus::Color(9, 20, 30, 60));
            g.FillPath(&sombra, &ps);
        }
    }
    Gdiplus::GraphicsPath p;
    caminoRedondeado(p, r, radio);
    int alfa = tm.oscuro ? 226 : 250;
    Gdiplus::LinearGradientBrush relleno(Gdiplus::PointF(0, (float)r.top - 1), Gdiplus::PointF(0, (float)r.bottom + 1),
                                         gc(mezcla(base, RGB(255, 255, 255), tm.oscuro ? 0.05 : 0.0), alfa),
                                         gc(base, alfa));
    g.FillPath(&relleno, &p);
    Gdiplus::Pen lapiz(gc(borde), (float)grosor);
    g.DrawPath(&lapiz, &p);
}

// Rectangulo redondeado con degradado vertical (botones principales).
void degradado(HDC dc, const RECT& r, int radio, COLORREF arriba, COLORREF abajo) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath p;
    caminoRedondeado(p, r, radio);
    Gdiplus::LinearGradientBrush b(Gdiplus::PointF(0, (float)r.top - 1), Gdiplus::PointF(0, (float)r.bottom + 1),
                                   gc(arriba), gc(abajo));
    g.FillPath(&b, &p);
}

// Rectangulo (sin redondear) relleno con un color semitransparente.
void velo(HDC dc, const RECT& r, COLORREF c, int alfa) {
    Gdiplus::Graphics g(dc);
    Gdiplus::SolidBrush b(gc(c, alfa));
    g.FillRectangle(&b, (int)r.left, (int)r.top, (int)(r.right - r.left), (int)(r.bottom - r.top));
}

// Mancha de luz difusa (degradado radial hacia transparente).
void halo(Gdiplus::Graphics& g, float cx, float cy, float radio, COLORREF c, int alfa) {
    Gdiplus::GraphicsPath p;
    p.AddEllipse(cx - radio, cy - radio, 2 * radio, 2 * radio);
    Gdiplus::PathGradientBrush b(&p);
    b.SetCenterColor(gc(c, alfa));
    Gdiplus::Color borde = gc(c, 0);
    int n = 1;
    b.SetSurroundColors(&borde, &n);
    g.FillPath(&b, &p);
}

// Solo el borde de un rectangulo redondeado.
void contorno(HDC dc, const RECT& r, int radio, COLORREF color, float grosor) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath p;
    caminoRedondeado(p, r, radio);
    Gdiplus::Pen lapiz(gc(color), grosor);
    g.DrawPath(&lapiz, &p);
}

void circulo(HDC dc, int cx, int cy, int radio, COLORREF fondo, COLORREF borde, float grosor) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush b(gc(fondo));
    g.FillEllipse(&b, cx - radio, cy - radio, radio * 2, radio * 2);
    if (borde != CLR_INVALID) {
        Gdiplus::Pen lapiz(gc(borde), grosor);
        g.DrawEllipse(&lapiz, (float)(cx - radio), (float)(cy - radio), (float)radio * 2, (float)radio * 2);
    }
}

void arco(HDC dc, int cx, int cy, int radio, float grosor, float inicio, float barrido, COLORREF color) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen lapiz(gc(color), grosor);
    lapiz.SetStartCap(Gdiplus::LineCapRound);
    lapiz.SetEndCap(Gdiplus::LineCapRound);
    g.DrawArc(&lapiz, (float)(cx - radio), (float)(cy - radio), (float)radio * 2, (float)radio * 2, inicio, barrido);
}

void linea(HDC dc, float x1, float y1, float x2, float y2, COLORREF color, float grosor) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen lapiz(gc(color), grosor);
    lapiz.SetStartCap(Gdiplus::LineCapRound);
    lapiz.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&lapiz, x1, y1, x2, y2);
}

void texto(HDC dc, HFONT f, COLORREF c, const std::wstring& t, RECT r, UINT flags) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, t.c_str(), (int)t.size(), &r, flags | DT_NOPREFIX);
}

int altoTexto(HDC dc, HFONT f, const std::wstring& t, int ancho) {
    if (t.empty()) return 0;
    SelectObject(dc, f);
    RECT r = {0, 0, ancho, 0};
    DrawTextW(dc, t.c_str(), (int)t.size(), &r, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    return r.bottom;
}

int anchoTexto(HDC dc, HFONT f, const std::wstring& t) {
    SelectObject(dc, f);
    SIZE sz = {0, 0};
    GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz);
    return sz.cx;
}

RECT rect(int x, int y, int w, int h) {
    RECT r = {x, y, x + w, y + h};
    return r;
}

std::wstring numero(double v) {
    wchar_t b[32];
    swprintf(b, 32, L"%.1f", v);
    std::wstring t = b;
    for (wchar_t& c : t) {
        if (c == L'.') c = L',';
    }
    return t;
}

std::wstring tiempo(int segundos) {
    wchar_t b[32];
    if (segundos >= 3600) swprintf(b, 32, L"%d:%02d:%02d", segundos / 3600, (segundos / 60) % 60, segundos % 60);
    else swprintf(b, 32, L"%d:%02d", segundos / 60, segundos % 60);
    return b;
}

std::wstring calificacion(double nota) {
    if (nota < 5) return L"Suspenso";
    if (nota < 6) return L"Aprobado";
    if (nota < 7) return L"Bien";
    if (nota < 9) return L"Notable";
    return L"Sobresaliente";
}

std::wstring unaLinea(std::wstring t) {
    for (wchar_t& c : t) {
        if (c == L'\n' || c == L'\r') c = L' ';
    }
    return t;
}

void zona(Estado* s, RECT r, int accion, int dato) { s->zonas.push_back({r, accion, dato}); }

bool encima(Estado* s, int accion, int dato) { return s->hoverAccion == accion && s->hoverDato == dato; }

// En el modo enfoque no hay barra de titulo ni lateral.
int altoTitulo(Estado* s) { return s->enfoque ? 0 : E(s, 52); }
int anchoLateral(Estado* s) { return s->enfoque ? 0 : E(s, 280); }

// =========================================================== componentes

void boton(Estado* s, HDC dc, RECT r, const std::wstring& t, int accion, int dato, EstiloBoton estilo, bool activo) {
    const gui::Tema& tm = gui::tema();
    bool h = activo && encima(s, accion, dato);
    COLORREF fondo, borde, tinta;
    switch (estilo) {
        case PRIMARIO:
            fondo = h ? mezcla(tm.acento, RGB(255, 255, 255), 0.12) : tm.acento;
            borde = fondo;
            tinta = RGB(255, 255, 255);
            break;
        case PELIGRO:
            fondo = h ? mezcla(tm.panel, tm.mal, 0.22) : mezcla(tm.panel, tm.mal, 0.10);
            borde = mezcla(tm.panel, tm.mal, 0.45);
            tinta = tm.mal;
            break;
        case FANTASMA:
            fondo = h ? tm.panelAlt : tm.panel;
            borde = h ? tm.borde : tm.panel;
            tinta = h ? tm.texto : tm.textoSuave;
            break;
        default:
            fondo = h ? mezcla(tm.panelAlt, tm.acento, 0.15) : tm.panelAlt;
            borde = h ? mezcla(tm.borde, tm.acento, 0.5) : tm.borde;
            tinta = tm.texto;
    }
    if (!activo) {
        fondo = mezcla(fondo, tm.fondo, 0.55);
        borde = mezcla(borde, tm.fondo, 0.55);
        tinta = mezcla(tinta, tm.fondo, 0.5);
    }
    if (estilo == PRIMARIO && activo) {
        // Sombra suave del color del boton (algo mas marcada al pasar por encima)
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        for (int k = 3; k >= 1; --k) {
            RECT sr = r;
            InflateRect(&sr, E(s, k), E(s, k) / 2);
            OffsetRect(&sr, 0, E(s, 2 + k));
            Gdiplus::GraphicsPath p;
            caminoRedondeado(p, sr, E(s, 10 + k));
            Gdiplus::SolidBrush b(gc(fondo, (h ? 34 : 20) / k));
            g.FillPath(&b, &p);
        }
        degradado(dc, r, E(s, 10), mezcla(fondo, RGB(255, 255, 255), 0.16), mezcla(fondo, RGB(0, 0, 0), 0.10));
    }
    else if (estilo == SECUNDARIO)
        cristal(dc, r, E(s, 10), fondo, borde);
    else
        redondeado(dc, r, E(s, 10), fondo, borde);
    texto(dc, s->f.negrita, tinta, t, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (activo) zona(s, r, accion, dato);
}

// Pastilla seleccionable. Devuelve su ancho.
int chip(Estado* s, HDC dc, int x, int y, const std::wstring& t, bool elegido, int accion, int dato) {
    const gui::Tema& tm = gui::tema();
    int w = anchoTexto(dc, s->f.negrita, t) + E(s, 28);  // elegido va en negrita: que quepa igual
    RECT r = rect(x, y, w, E(s, 34));
    bool h = encima(s, accion, dato);
    COLORREF fondo = elegido ? mezcla(tm.panel, tm.acento, tm.oscuro ? 0.32 : 0.16) : (h ? tm.panelAlt : tm.panel);
    COLORREF borde = elegido ? tm.acento : (h ? mezcla(tm.borde, tm.acento, 0.4) : tm.borde);
    redondeado(dc, r, E(s, 17), fondo, borde, elegido ? 2 : 1);
    texto(dc, elegido ? s->f.negrita : s->f.texto, elegido ? tm.texto : tm.textoSuave, t, r,
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    zona(s, r, accion, dato);
    return w;
}

// Etiqueta de color, sin accion. Devuelve su ancho.
int etiqueta(Estado* s, HDC dc, int x, int y, const std::wstring& t, COLORREF c) {
    const gui::Tema& tm = gui::tema();
    int tw = anchoTexto(dc, s->f.peque, t) + E(s, 20);
    RECT r = rect(x, y, tw, E(s, 26));
    redondeado(dc, r, E(s, 13), mezcla(tm.fondo, c, 0.16), mezcla(tm.fondo, c, 0.4));
    texto(dc, s->f.peque, mezcla(c, tm.texto, 0.25), t, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return tw;
}

void interruptor(Estado* s, HDC dc, int x, int y, bool activo, const std::wstring& t, int accion) {
    const gui::Tema& tm = gui::tema();
    int w = E(s, 40), h = E(s, 22);
    RECT pista = rect(x, y + E(s, 6), w, h);
    redondeado(dc, pista, h / 2, activo ? tm.acento : tm.panelAlt, activo ? tm.acento : tm.borde);
    int cx = activo ? pista.right - h / 2 : pista.left + h / 2;
    circulo(dc, cx, pista.top + h / 2, h / 2 - E(s, 3), RGB(255, 255, 255));
    int tw = anchoTexto(dc, s->f.texto, t);
    texto(dc, s->f.texto, encima(s, accion) ? tm.texto : tm.textoSuave, t,
          rect(x + w + E(s, 10), y, tw + E(s, 4), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    zona(s, rect(x, y, w + E(s, 14) + tw, E(s, 34)), accion);
}

void casilla(Estado* s, HDC dc, RECT r, bool marcada) {
    const gui::Tema& tm = gui::tema();
    redondeado(dc, r, E(s, 5), marcada ? tm.acento : tm.panel, marcada ? tm.acento : tm.borde, 2);
    if (marcada) {
        float x = (float)r.left, y = (float)r.top, w = (float)(r.right - r.left);
        linea(dc, x + w * 0.24f, y + w * 0.52f, x + w * 0.43f, y + w * 0.70f, RGB(255, 255, 255), (float)E(s, 2));
        linea(dc, x + w * 0.43f, y + w * 0.70f, x + w * 0.76f, y + w * 0.32f, RGB(255, 255, 255), (float)E(s, 2));
    }
}

// Marca de acierto (V) o fallo (X) dentro de un circulo.
void marca(HDC dc, int cx, int cy, int radio, bool bien) {
    const gui::Tema& tm = gui::tema();
    circulo(dc, cx, cy, radio, bien ? tm.ok : tm.mal);
    float r = (float)radio, g = std::max(2.0f, radio / 4.5f);
    COLORREF b = RGB(255, 255, 255);
    if (bien) {
        linea(dc, cx - r * 0.45f, cy + r * 0.02f, cx - r * 0.1f, cy + r * 0.38f, b, g);
        linea(dc, cx - r * 0.1f, cy + r * 0.38f, cx + r * 0.48f, cy - r * 0.35f, b, g);
    } else {
        linea(dc, cx - r * 0.38f, cy - r * 0.38f, cx + r * 0.38f, cy + r * 0.38f, b, g);
        linea(dc, cx + r * 0.38f, cy - r * 0.38f, cx - r * 0.38f, cy + r * 0.38f, b, g);
    }
}

void spinner(Estado* s, HDC dc, int cx, int cy, int radio) {
    const gui::Tema& tm = gui::tema();
    arco(dc, cx, cy, radio, (float)E(s, 4), 0, 360, tm.panelAlt);
    arco(dc, cx, cy, radio, (float)E(s, 4), (float)(s->animacion * 18 % 360), 100, tm.acento);
}

void barra(HDC dc, RECT r, double fraccion, COLORREF color) {
    const gui::Tema& tm = gui::tema();
    int h = r.bottom - r.top;
    redondeado(dc, r, h / 2, tm.panelAlt);
    fraccion = std::max(0.0, std::min(1.0, fraccion));
    if (fraccion > 0) {
        RECT lleno = r;
        lleno.right = r.left + std::max(h, (int)((r.right - r.left) * fraccion));
        redondeado(dc, lleno, h / 2, color);
    }
}

void cifra(Estado* s, HDC dc, RECT r, const std::wstring& valor, const std::wstring& nombre, COLORREF color,
           int ico) {
    const gui::Tema& tm = gui::tema();
    cristal(dc, r, E(s, 14));
    if (ico >= 0)
        icono(s, dc, (Icono)ico, r.right - E(s, 30), r.top + E(s, 30), E(s, 22), mezcla(tm.textoSuave, tm.panel, 0.35));
    texto(dc, s->f.titulo, color, valor, rect(r.left + E(s, 16), r.top + E(s, 12), r.right - r.left - E(s, 24), E(s, 36)),
          DT_LEFT | DT_TOP | DT_SINGLELINE);
    texto(dc, s->f.peque, tm.textoSuave, nombre,
          rect(r.left + E(s, 16), r.bottom - E(s, 30), r.right - r.left - E(s, 24), E(s, 20)),
          DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void tituloSeccion(Estado* s, HDC dc, int x, int& y, int w, const std::wstring& t, const std::wstring& sub) {
    const gui::Tema& tm = gui::tema();
    texto(dc, s->f.subtitulo, tm.texto, t, rect(x, y, w, E(s, 26)), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    y += E(s, 28);
    if (!sub.empty()) {
        int h = altoTexto(dc, s->f.peque, sub, w);
        texto(dc, s->f.peque, tm.textoSuave, sub, rect(x, y, w, h));
        y += h + E(s, 4);
    }
    y += E(s, 8);
}

// ------------------------------------------------------------ listas por temas

std::vector<FilaTema> filasPorTema(const js::Valor& lista) {
    std::vector<int> orden;
    for (std::size_t i = 0; i < lista.tam(); ++i) orden.push_back((int)i);
    std::stable_sort(orden.begin(), orden.end(), [&](int a, int b) {
        return lista[(std::size_t)a]["orden"].comoEntero(a) < lista[(std::size_t)b]["orden"].comoEntero(b);
    });
    std::vector<FilaTema> filas;
    int grupos = 0;
    std::string actual;
    for (int i : orden) {
        std::string g = lista[(std::size_t)i]["grupo"].comoCadena();
        if (filas.empty() || g != actual) {
            filas.push_back({-1, W(g), 0, grupos++});
            actual = g;
        }
        filas.push_back({i, W(g), 0, grupos - 1});
    }
    // Cuenta los apartados de cada tema en su subtitulo
    for (std::size_t k = 0; k < filas.size(); ++k)
        if (filas[k].indice < 0)
            for (std::size_t j = k + 1; j < filas.size() && filas[j].indice >= 0; ++j) ++filas[k].miembros;
    if (grupos < 2) {
        std::vector<FilaTema> solo;
        for (const FilaTema& f : filas)
            if (f.indice >= 0) solo.push_back({f.indice, f.grupo, 0, 0});
        return solo;
    }
    return filas;
}

bool hayTemas(const std::vector<FilaTema>& filas) {
    for (const FilaTema& f : filas)
        if (f.indice < 0) return true;
    return false;
}

void subtituloTema(Estado* s, HDC dc, RECT r, const std::wstring& grupo, const std::wstring& info, COLORREF c,
                   int marcado) {
    const gui::Tema& tm = gui::tema();
    int x = r.left, h = r.bottom - r.top, cy = r.top + h / 2;
    if (marcado >= 0) {
        RECT cr = rect(x + E(s, 6), cy - E(s, 10), E(s, 20), E(s, 20));
        if (marcado == 1) {
            redondeado(dc, cr, E(s, 5), tm.acento, tm.acento, 2);
            linea(dc, (float)cr.left + E(s, 5), (float)cy, (float)cr.right - E(s, 5), (float)cy, RGB(255, 255, 255),
                  (float)E(s, 2));
        } else {
            casilla(s, dc, cr, marcado == 2);
        }
        x += E(s, 40);
    }
    // «Tema 3 · Redes neuronales» -> pastilla «TEMA 3» + «Redes neuronales»
    std::wstring pastilla, titulo = grupo;
    std::size_t punto = grupo.find(L" · ");
    if (grupo.compare(0, 5, L"Tema ") == 0) {
        pastilla = grupo.substr(0, punto);
        titulo = punto == std::wstring::npos ? L"" : grupo.substr(punto + 3);
    }
    if (!pastilla.empty() && h >= E(s, 50) &&
        anchoTexto(dc, s->f.negrita, titulo) > (int)(r.right - x) - anchoTexto(dc, s->f.mini, pastilla) - E(s, 16) -
                                                  (info.empty() ? 0 : anchoTexto(dc, s->f.mini, info) + E(s, 20)))
        cy = r.top + E(s, 4) + E(s, 11);  // dos lineas (ver abajo)
    if (!pastilla.empty()) {
        for (wchar_t& ch : pastilla) ch = (wchar_t)towupper(ch);
        int pw = anchoTexto(dc, s->f.mini, pastilla) + E(s, 16);
        redondeado(dc, rect(x, cy - E(s, 11), pw, E(s, 22)), E(s, 11), mezcla(tm.panel, c, tm.oscuro ? 0.26 : 0.16));
        texto(dc, s->f.mini, mezcla(c, tm.texto, tm.oscuro ? 0.15 : 0.35), pastilla, rect(x, cy - E(s, 11), pw, E(s, 22)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += pw + E(s, 10);
    }
    int iw = info.empty() ? 0 : std::min((int)(r.right - x) / 3, anchoTexto(dc, s->f.mini, info) + E(s, 4));
    int tw = std::min(anchoTexto(dc, s->f.negrita, titulo), (int)(r.right - x - iw - E(s, 16)));
    // Alto de sobra y el titulo no cabe: la pastilla y la cuenta arriba y el titulo entero debajo.
    if (h >= E(s, 50) && !pastilla.empty() && tw < anchoTexto(dc, s->f.negrita, titulo)) {
        int arriba = r.top + E(s, 4), alto1 = E(s, 22);
        if (r.right - iw - E(s, 10) > x + E(s, 10))
            linea(dc, (float)x, (float)arriba + alto1 / 2 + 0.5f, (float)(r.right - iw - E(s, 10)), (float)arriba + alto1 / 2 + 0.5f,
                  tm.borde, 1.0f);
        if (iw) texto(dc, s->f.mini, tm.textoSuave, info, rect(r.right - iw, arriba, iw, alto1), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        RECT abajo = rect(r.left + (marcado >= 0 ? E(s, 40) : 0), arriba + alto1 + E(s, 2), r.right - r.left - (marcado >= 0 ? E(s, 40) : 0),
                          r.bottom - arriba - alto1 - E(s, 2));
        texto(dc, s->f.negrita, tm.texto, titulo, abajo, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }
    if (!titulo.empty())
        texto(dc, s->f.negrita, tm.texto, titulo, rect(x, r.top, std::max(0, tw), h),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int lx = x + (titulo.empty() ? 0 : tw + E(s, 12));
    if (r.right - iw - E(s, 10) > lx + E(s, 10))
        linea(dc, (float)lx, (float)cy + 0.5f, (float)(r.right - iw - E(s, 10)), (float)cy + 0.5f, tm.borde, 1.0f);
    if (iw)
        texto(dc, s->f.mini, tm.textoSuave, info, rect(r.right - iw, r.top, iw, h), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void aplicarGrupos(js::Valor& lista, const js::Valor& grupos) {
    if (!lista.esArray() || !grupos.esObjeto()) return;
    js::Valor nueva = js::Valor::array();
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        js::Valor e = lista[i];
        const js::Valor& g = grupos[e["ruta"].comoCadena()];
        if (!g.esNulo()) {
            e["grupo"] = g["grupo"];
            e["grupo_orden"] = g["grupo_orden"];
            e["orden"] = g["orden"];
        }
        nueva.agregar(e);
    }
    lista = nueva;
}

// Dibuja una imagen ajustada (sin deformar) dentro de un rectangulo.
void imagenAjustada(Gdiplus::Image* im, HDC dc, RECT r) {
    if (!im || im->GetLastStatus() != Gdiplus::Ok || im->GetWidth() == 0) return;
    double ew = r.right - r.left, eh = r.bottom - r.top;
    double esc = std::min(ew / im->GetWidth(), eh / im->GetHeight());
    int w = (int)(im->GetWidth() * esc), h = (int)(im->GetHeight() * esc);
    Gdiplus::Graphics g(dc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.DrawImage(im, (int)(r.left + (ew - w) / 2), (int)(r.top + (eh - h) / 2), w, h);
}

Gdiplus::Image* imagen(Estado* s, const std::wstring& ruta) {
    if (ruta.empty()) return NULL;
    auto it = s->imagenes.find(ruta);
    if (it != s->imagenes.end()) return it->second;
    if (s->imagenes.size() > 40) {
        for (auto& par : s->imagenes) delete par.second;
        s->imagenes.clear();
    }
    Gdiplus::Image* im = Gdiplus::Image::FromFile(ruta.c_str());
    if (im && im->GetLastStatus() != Gdiplus::Ok) {
        delete im;
        im = NULL;
    }
    s->imagenes[ruta] = im;
    return im;
}

// ================================================================ motor

int enviar(Estado* s, const std::string& orden, js::Valor peticion, bool bloquea, const std::wstring& espera) {
    peticion["cmd"] = orden;
    int id = s->motor.enviar(peticion);
    if (!id) {
        s->vista = V_ERROR;
        s->error = L"El motor del profesor no responde.";
        InvalidateRect(s->ventana, NULL, FALSE);
        return 0;
    }
    if (orden != "cancelar_duda" && orden != "cancelar") s->peticiones[id] = orden;
    if (bloquea) {
        s->ocupado = true;
        s->cargando = espera.empty() ? L"Un momento…" : espera;
        SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    }
    InvalidateRect(s->ventana, NULL, FALSE);
    return id;
}

void arrancarMotor(Estado* s) {
    std::wstring error;
    s->vista = V_ARRANCANDO;
    s->cargando = L"Despertando al profesor…";
    s->peticiones.clear();
    s->ocupado = false;
    if (!s->motor.arrancar(s->ventana, WM_MOTOR, error)) {
        s->vista = V_ERROR;
        s->error = error;
    }
    SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    InvalidateRect(s->ventana, NULL, FALSE);
}

std::string cfg(Estado* s, const char* clave, const char* defecto) { return s->config[clave].comoCadena(defecto); }

js::Valor configParaEnviar(Estado* s) {
    js::Valor c = js::Valor::objeto();
    for (const char* k : {"modo", "dificultad", "tipo", "correccion", "modelo"}) c[k] = cfg(s, k);
    c["n"] = s->config["n"].comoEntero(10);
    c["penalizar"] = s->config["penalizar"].comoBool();
    c["revision"] = s->config["revision"].comoBool();
    c["auto_guardar"] = s->config["auto_guardar"].comoBool(true);
    c["reserva"] = s->config["reserva"].comoBool(true);
    c["limite"] = s->config["limite"].comoEntero(0);
    c["formatos"] = s->config["formatos"].esArray() ? s->config["formatos"] : js::Valor::array();
    js::Valor temas = js::Valor::array();
    const js::Valor& archivos = s->hola["archivos"];
    bool todos = true;
    for (std::size_t i = 0; i < archivos.tam() && i < s->temas.size(); ++i) {
        if (s->temas[i]) temas.agregar(archivos[i]["ruta"].comoCadena());
        else todos = false;
    }
    c["temas"] = todos ? js::Valor::array() : temas;
    return c;
}

bool hayTest(Estado* s) { return s->sesion.contiene("token"); }
bool testTerminado(Estado* s) { return s->sesion["terminada"].comoBool(); }
bool iaDisponible(Estado* s) { return s->hola["ia"]["disponible"].comoBool(); }
bool iaExterna(Estado* s) { return s->hola["ia"]["proveedor"].comoCadena("ollama") != "ollama"; }
std::wstring servidorIa(Estado* s) {
    std::wstring n = W(s->hola["ia"]["servidor"]);
    return n.empty() ? L"Ollama" : n;
}

bool exigirIa(Estado* s) {
    if (iaDisponible(s)) return true;
    aviso(s, !s->hola["ia"]["instalado"].comoBool()
                 ? L"Para preguntar al profesor hace falta la IA (" + servidorIa(s) + L"), y no está instalada"
             : iaExterna(s) ? L"No hay conexión con la IA (" + servidorIa(s) + L"): pulsa «IA sin conexión · Reintentar» arriba"
                            : std::wstring(L"Para preguntar al profesor hace falta la IA y está apagada: enciéndela con «IA apagada · Iniciar»"));
    // Por si se ha encendido por fuera desde la ultima vez (como mucho una comprobacion cada 10 s)
    static ULONGLONG ultima = 0;
    if (GetTickCount64() - ultima > 10000) {
        ultima = GetTickCount64();
        enviar(s, "ia");
    }
    return false;
}
bool esExamen(Estado* s) { return s->sesion["config"]["correccion"].comoCadena() == "final" && !testTerminado(s); }

bool todasRespondidas(Estado* s) {
    const js::Valor& e = s->sesion["estados"];
    for (std::size_t i = 0; i < e.tam(); ++i) {
        std::string x = e[i].comoCadena();
        if (x == "sin" || x == "preparando") return false;
    }
    return e.tam() > 0;
}

void colocarEdit(Estado* s);

// Segundos que quedan si el test tiene limite de tiempo; -1 si no tiene.
int segundosRestantes(Estado* s) {
    int porPregunta = s->sesion["config"]["limite"].comoEntero(0);
    if (porPregunta <= 0 || !hayTest(s)) return -1;
    int total = porPregunta * s->sesion["n"].comoEntero(1);
    int pasados = s->segundosBase + (int)((GetTickCount64() - s->relojBase) / 1000);
    return std::max(0, total - pasados);
}

// Guarda el fotograma actual para fundirlo con el siguiente (~0,2 s).
void empezarTransicion(Estado* s) {
    if (!s->marco || !IsWindowVisible(s->ventana) || !gui::estilo().animaciones) return;
    HDC pantalla = GetDC(s->ventana);
    if (!s->anterior) s->anterior = CreateCompatibleBitmap(pantalla, s->marcoAncho, s->marcoAlto);
    HDC origen = CreateCompatibleDC(pantalla), destino = CreateCompatibleDC(pantalla);
    HGDIOBJ vo = SelectObject(origen, s->marco), vd = SelectObject(destino, s->anterior);
    BitBlt(destino, 0, 0, s->marcoAncho, s->marcoAlto, origen, 0, 0, SRCCOPY);
    SelectObject(origen, vo);
    SelectObject(destino, vd);
    DeleteDC(origen);
    DeleteDC(destino);
    ReleaseDC(s->ventana, pantalla);
    s->transicion = GetTickCount64();
    SetTimer(s->ventana, TIMER_ANIMACION, 15, NULL);
}

// Animaciones cortas del test. Avance de 0 a 1 de una animacion que empezo en `desde` y dura `ms`.
double avanceAnim(ULONGLONG desde, int ms) {
    if (!desde || !gui::estilo().animaciones) return 1.0;
    ULONGLONG ahora = GetTickCount64();
    return ahora <= desde ? 0.0 : std::min(1.0, (double)(ahora - desde) / ms);
}

// Curva de frenado: arranca rapido y llega despacio.
double frenado(double t) { return 1.0 - std::pow(1.0 - t, 3.0); }

void lanzarAnim(Estado* s, ULONGLONG& marca, int retraso = 0) {
    marca = GetTickCount64() + retraso;
    if (gui::estilo().animaciones) SetTimer(s->ventana, TIMER_ANIMACION, 15, NULL);
}

bool animandoTest(Estado* s) {
    if (!gui::estilo().animaciones) return false;
    if (s->vista == V_PREGUNTA) return avanceAnim(s->animPregunta, 900) < 1 || avanceAnim(s->animCorreccion, 800) < 1;
    if (s->vista == V_RESULTADO) return avanceAnim(s->animResultado, 2600) < 1;
    return false;
}

// Cambia de pantalla. `recordar` guarda la actual para el boton de volver.
void irAVista(Estado* s, Vista v, bool recordar) {
    if (s->vista == V_TEMARIO && v != V_TEMARIO) guardarLectura(s);
    if (v != s->vista) empezarTransicion(s);
    if (recordar && v != s->vista && s->vista != V_ARRANCANDO && s->vista != V_ERROR) {
        s->historial.push_back(s->vista);
        if (s->historial.size() > 40) s->historial.erase(s->historial.begin());
    }
    s->vista = v;
    s->scroll = 0;
    s->zoom.clear();
    if (v == V_PREGUNTA) SetTimer(s->ventana, TIMER_RELOJ, 1000, NULL);
    else KillTimer(s->ventana, TIMER_RELOJ);
    ShowWindow(s->edit, v == V_CHAT ? SW_SHOW : SW_HIDE);
    if (v != V_DESARROLLO) ShowWindow(s->respuesta, SW_HIDE);  // se vuelve a colocar al pintar
    if (v != V_BUSCAR) ShowWindow(s->busqueda, SW_HIDE);
    s->selector = false;
    if (v == V_CHAT) {
        colocarEdit(s);
        SetFocus(s->edit);
        s->chatAlFinal = true;
    } else {
        SetFocus(s->ventana);
    }
    InvalidateRect(s->ventana, NULL, FALSE);
}

void pedirPregunta(Estado* s, int indice) {
    js::Valor p = js::Valor::objeto();
    p["token"] = s->sesion["token"].comoCadena();
    p["indice"] = indice;
    enviar(s, "pregunta", p);
}

// Abre la pantalla de una pestana, pidiendo al motor lo que haga falta.
void abrirVista(Estado* s, Vista v) {
    if (v == V_PROGRESO) {
        enviar(s, "progreso", js::Valor::objeto(), true, L"Calculando tu progreso…");
        return;  // la vista se cambia al llegar los datos
    }
    if (v == V_FALLOS) {
        enviar(s, "fallos", js::Valor::objeto(), true, L"Reuniendo tus fallos…");
        return;
    }
    if (v == V_HUB || v == V_TARJETAS || v == V_RESUMENES || v == V_DESARROLLO) {
        abrirEstudio(s, v);
        return;
    }
    if (v == V_BUSCAR) {
        abrirHerramientas(s, v);
        return;
    }
    if (v == V_TEMARIO) {
        abrirTemario(s);
        return;
    }
    if (v == V_CHAT && !s->chatEnCurso && !exigirIa(s)) return;  // las dudas las responde la IA
    if (v == V_PREGUNTA && hayTest(s) && !s->pregunta.contiene("indice")) pedirPregunta(s, 0);
    // El bocadillo del profesor habla de la pantalla nueva (si no, se quedaba el de la anterior)
    if (v == V_CHAT && !s->chatEnCurso)
        s->mensaje = L"Pregúntame lo que no entiendas: te respondo con tus apuntes y te digo de qué página lo saco.";
    else if (v == V_INICIO)
        s->mensaje = L"Elige qué temas entran y cómo lo quieres. «Rápido» empieza al instante, sin esperar a la IA.";
    irAVista(s, v);
}

void volver(Estado* s) {
    if (!s->zoom.empty()) {
        s->zoom.clear();
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }
    while (!s->historial.empty()) {
        Vista v = s->historial.back();
        s->historial.pop_back();
        if (v == s->vista) continue;
        if ((v == V_PREGUNTA && !hayTest(s)) || (v == V_RESULTADO && s->resultado.esNulo())) continue;
        irAVista(s, v, false);
        return;
    }
    if (s->vista != V_HUB) {
        enviar(s, "inicio");
        irAVista(s, V_HUB, false);
    }
}

void ponerPregunta(Estado* s, const js::Valor& p) {
    int anterior = s->pregunta["indice"].comoEntero(-1);
    bool estabaPreparando = s->pregunta["preparando"].comoBool();
    bool estabaCorregida = s->pregunta["corregida"].comoBool();
    bool misma = anterior == p["indice"].comoEntero(-1);
    if (!p["preparando"].comoBool() && (!misma || estabaPreparando)) {
        lanzarAnim(s, s->animPregunta);
        s->animCorreccion = 0;
    } else if (misma && !estabaCorregida && p["corregida"].comoBool()) lanzarAnim(s, s->animCorreccion);
    s->pregunta = p;
    if (p.contiene("sesion")) s->sesion = p["sesion"];
    s->seleccion = p["eleccion"].esNulo() ? -1 : p["eleccion"].comoEntero(-1);
    s->dudosa = p["dudosa"].comoBool();
    if (p.contiene("mis_fallos")) s->hola["resumen"]["mis_fallos"] = p["mis_fallos"].comoEntero();
    if (anterior != p["indice"].comoEntero(-1)) {
        s->scroll = 0;
        if (anterior >= 0 && s->vista == V_PREGUNTA) empezarTransicion(s);
    }
    s->segundosBase = s->sesion["segundos"].comoEntero();
    s->relojBase = GetTickCount64();
    if (p["preparando"].comoBool()) SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
}

void terminarTest(Estado* s, bool preguntarAntes) {
    if (preguntarAntes && !todasRespondidas(s)) {
        if (!gui::preguntar(s->ventana, L"Hay preguntas sin responder: contarán como en blanco.\n¿Terminar el test ahora?"))
            return;
    }
    js::Valor p = js::Valor::objeto();
    p["token"] = s->sesion["token"].comoCadena();
    enviar(s, "terminar", p, true, L"Corrigiendo tu test…");
}

void responder(Estado* s, int eleccion) {
    js::Valor p = js::Valor::objeto();
    p["token"] = s->sesion["token"].comoCadena();
    p["indice"] = s->pregunta["indice"].comoEntero();
    p["eleccion"] = eleccion;
    p["dudosa"] = s->dudosa;
    enviar(s, "responder", p);
}

void empezar(Estado* s, bool deFallos, int soloTema) {
    js::Valor c = configParaEnviar(s);
    if (deFallos) {
        c["fallos"] = true;
        c["n"] = s->nFallos;
    }
    if (soloTema >= 0) {
        js::Valor t = js::Valor::array();
        t.agregar(s->progreso["temas"][(std::size_t)soloTema]["ruta"].comoCadena());
        c["temas"] = t;
    }
    empezarCon(s, c, deFallos ? L"Preparando el test de tus fallos…" : L"Preparando tu test…");
}

void empezarCon(Estado* s, js::Valor config, const std::wstring& espera) {
    js::Valor p = js::Valor::objeto();
    p["config"] = config;
    s->generacion = js::Valor();
    s->resultado = js::Valor();
    enviar(s, "empezar", p, true, espera);
}

// ----------------------------------------------------------------- chat

std::wstring contextoDePregunta(Estado* s) {
    if (!s->contextoPregunta.empty()) return s->contextoEtiqueta;
    if (!hayTest(s) || s->contextoIndice < 0) return L"";
    return L"Sobre la pregunta " + std::to_wstring(s->contextoIndice + 1) + L" del test";
}

void enviarDuda(Estado* s, const std::wstring& duda) {
    std::wstring t = duda;
    while (!t.empty() && iswspace(t.back())) t.pop_back();
    while (!t.empty() && iswspace(t.front())) t.erase(t.begin());
    if (t.empty() || s->chatEnCurso || !exigirIa(s)) return;
    js::Valor p = js::Valor::objeto();
    p["texto"] = gui::aUtf8(t);
    if (!s->contextoPregunta.empty()) {
        p["pregunta"] = s->contextoPregunta;
    } else if (s->contextoIndice >= 0 && hayTest(s)) {
        p["token"] = s->sesion["token"].comoCadena();
        p["indice"] = s->contextoIndice;
    }
    p["etiqueta"] = gui::aUtf8(contextoDePregunta(s));
    // Las ultimas conversaciones, para poder repreguntar («¿y un ejemplo?»).
    js::Valor historial = js::Valor::array();
    for (std::size_t i = 0; i + 1 < s->chat.size(); i += 2) {
        if (!s->chat[i].alumno || s->chat[i + 1].alumno) continue;
        js::Valor h = js::Valor::objeto();
        h["pregunta"] = gui::aUtf8(s->chat[i].texto);
        h["respuesta"] = gui::aUtf8(s->chat[i + 1].texto);
        historial.agregar(h);
    }
    p["historial"] = historial;
    Mensaje alumno;
    alumno.alumno = true;
    alumno.texto = t;
    alumno.contexto = contextoDePregunta(s);
    s->chat.push_back(alumno);
    Mensaje respuesta;
    respuesta.enCurso = true;
    s->chat.push_back(respuesta);
    s->chatEnCurso = true;
    s->chatDesde = GetTickCount64();
    s->chatAlFinal = true;
    s->mensaje = L"Buena pregunta. Déjame pensarlo con tus apuntes…";
    if (s->vista != V_CHAT && !campoEnPanelLectura(s)) irAVista(s, V_CHAT);
    SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    enviar(s, "duda", p);
}

// ======================================================= mensajes del motor

// Pruebas evaluables de la asignatura (del calendario del gestor), para el plan y la cuenta atras.
js::Valor examenesParaEnviar(Estado* s) {
    js::Valor lista = js::Valor::array();
    for (const Examen& e : s->asignatura.examenes) {
        if (!e.fecha.valida()) continue;
        char fecha[16];
        snprintf(fecha, sizeof(fecha), "%04d-%02d-%02d", e.fecha.anio, e.fecha.mes, e.fecha.dia);
        js::Valor x = js::Valor::objeto();
        x["nombre"] = e.nombre;
        x["fecha"] = std::string(fecha);
        x["peso"] = e.ponderacion;
        x["calificado"] = e.calificado;
        lista.agregar(x);
    }
    return lista;
}

// Carga la conversacion guardada del chat (llega con «hola»).
void cargarChat(Estado* s, const js::Valor& turnos) {
    s->chat.clear();
    for (std::size_t i = 0; i < turnos.tam(); ++i) {
        Mensaje alumno;
        alumno.alumno = true;
        alumno.texto = W(turnos[i]["pregunta"]);
        alumno.contexto = W(turnos[i]["contexto"]);
        s->chat.push_back(alumno);
        Mensaje respuesta;
        respuesta.texto = W(turnos[i]["respuesta"]);
        respuesta.fuentes = turnos[i]["fuentes"];
        s->chat.push_back(respuesta);
    }
}

void alRecibirEvento(Estado* s, const js::Valor& ev) {
    std::string tipo = ev["evento"].comoCadena();
    if (herramientasEvento(s, ev) || estudioEvento(s, ev)) {
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }
    if (tipo == "listo") {
        js::Valor p = js::Valor::objeto();
        p["codigo"] = s->asignatura.codigo;
        p["nombre"] = s->asignatura.nombre;
        p["temario"] = carpetaDeAsignatura(s->asignatura, true);
        p["examenes"] = examenesParaEnviar(s);
        enviar(s, "hola", p, true, L"Leyendo tu Temario…");
    } else if (tipo == "leyendo") {
        wchar_t b[64];
        swprintf(b, 64, L" (%d de %d)", ev["hechos"].comoEntero() + 1, ev["total"].comoEntero());
        s->cargando = L"Leyendo «" + W(ev["archivo"]) + L"»" + b;
    } else if (tipo == "ia") {
        s->iaFase = ev["estado"].comoCadena();
        if (s->iaFase == "lista" && s->vista == V_INICIO) s->mensaje = L"La IA ya está cargada y lista para redactar preguntas.";
    } else if (tipo == "generacion") {
        std::string estado = ev["estado"].comoCadena();
        if (estado == "terminada" || estado == "parada") {
            s->generacion = js::Valor();
            if (estado == "parada") s->mensaje = W(ev["mensaje"]);
        } else {
            s->generacion = ev;
            SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
        }
    } else if (tipo == "pregunta_lista") {
        if (ev["token"].comoCadena() == s->sesion["token"].comoCadena()) {
            s->sesion = ev["sesion"];
            if (s->vista == V_PREGUNTA && s->pregunta["preparando"].comoBool() &&
                s->pregunta["indice"].comoEntero() == ev["indice"].comoEntero()) {
                s->mensaje = L"¡Lista! Aquí tienes la pregunta.";
                pedirPregunta(s, ev["indice"].comoEntero());
            }
        }
    } else if (tipo == "reserva") {
        s->reserva = ev;
        s->hola["resumen"]["ia_nuevas"] = ev["listas"].comoEntero();
        if (ev["estado"].comoCadena() == "trabajando") SetTimer(s->ventana, TIMER_ANIMACION, 110, NULL);
    } else if (tipo == "aviso") {
        s->mensaje = W(ev["mensaje"]);
    } else if (tipo == "duda") {
        if (s->chatEnCurso && !s->chat.empty()) {
            s->chat.back().texto = W(ev["texto"]);
            s->chatAlFinal = true;
        }
    }
    InvalidateRect(s->ventana, NULL, FALSE);
}

void alRecibirRespuesta(Estado* s, const js::Valor& r) {
    int id = r["id"].comoEntero();
    auto it = s->peticiones.find(id);
    if (it == s->peticiones.end()) return;
    std::string orden = it->second;
    s->peticiones.erase(it);
    if (orden != "duda" && orden != "precalentar" && orden != "pagina") s->ocupado = false;

    if (!r["ok"].comoBool()) {
        std::wstring m = W(r["mensaje"]);
        if (temarioError(s, id, orden) || herramientasError(s, orden, m) || estudioError(s, orden, m)) {
            InvalidateRect(s->ventana, NULL, FALSE);
            return;
        }
        if (orden == "hola") {
            s->vista = V_ERROR;
            s->error = m;
        } else if (orden == "duda") {
            s->chatEnCurso = false;
            if (!s->chat.empty()) {
                s->chat.back().texto = m;
                s->chat.back().enCurso = false;
            }
            s->mensaje = L"No he podido responder ahora mismo.";
            enviar(s, "ia");  // quiza se ha apagado la IA: si es asi, dejan de ofrecerse las preguntas
        } else {
            s->mensaje = m;
            if (orden == "empezar" || orden == "continuar") gui::avisar(s->ventana, m);
        }
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }

    if (temarioRespuesta(s, orden, r) || herramientasRespuesta(s, orden, r)) {
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }
    if (orden == "hola") {
        s->hola = r;
        if (!r["ia"]["aviso"].comoCadena().empty()) aviso(s, W(r["ia"]["aviso"]));  // ia.json con errores
        s->reserva = js::Valor();
        s->config = r["config"];
        const js::Valor& archivos = r["archivos"];
        const js::Valor& elegidos = s->config["temas"];
        s->temas.assign(archivos.tam(), elegidos.tam() == 0);
        for (std::size_t i = 0; i < archivos.tam(); ++i) {
            for (std::size_t j = 0; j < elegidos.tam(); ++j) {
                if (elegidos[j].comoCadena() == archivos[i]["ruta"].comoCadena()) s->temas[i] = true;
            }
        }
        s->inicio = r["inicio"];
        cargarChat(s, r["chat"]);
        const js::Valor& examen = s->inicio["examen"];
        if (archivos.tam() == 0) {
            s->mensaje = L"Tu Temario está vacío. Copia tus apuntes (PDF, Word, PowerPoint o texto) en la "
                         L"carpeta Temario y vuelve a abrirme.";
        } else if (!r["pendiente"].esNulo()) {
            s->mensaje = L"Tienes un test a medias. ¿Lo terminamos?";
        } else if (!examen.esNulo() && examen["dias"].comoEntero() <= 7) {
            s->mensaje = L"¡Hola! " + W(examen["nombre"]) +
                         (examen["dias"].comoEntero() == 0 ? L" es hoy. Repasemos lo esencial."
                                                           : L" se acerca. Te he preparado un plan para hoy.");
        } else {
            s->mensaje = L"¡Hola! Te he preparado un plan para hoy. Todo sale de tus apuntes.";
        }
        if (r["ia"]["disponible"].comoBool()) {
            s->iaFase = "cargando";
            enviar(s, "precalentar");
        }
        s->historial.clear();
        irAVista(s, V_HUB, false);
        if (archivos.tam() > 0) enviar(s, "temario");  // para «Seguir leyendo» y lo que queda por leer
    } else if (orden == "empezar" || orden == "continuar") {
        s->sesion = r["sesion"];
        s->pregunta = js::Valor();
        ponerPregunta(s, r["pregunta"]);
        std::string modo = s->sesion["config"]["modo"].comoCadena();
        if (s->sesion["config"]["fallos"].comoBool()) s->mensaje = L"Vamos con tus fallos. Esta vez seguro que sale mejor.";
        else if (orden == "continuar") s->mensaje = L"Seguimos donde lo dejaste.";
        else if (modo == "ia") s->mensaje = L"La IA está redactando tus preguntas. Puedes empezar en cuanto esté la primera.";
        else if (modo == "mixto") s->mensaje = L"Empezamos ya. Mientras respondes, la IA mejora las preguntas que aún no has visto.";
        else s->mensaje = L"¡Vamos allá! Lee con calma cada pregunta.";
        if (esExamen(s)) s->mensaje = L"Modo examen: no verás la corrección hasta entregar. Puedes cambiar tus respuestas.";
        if (s->sesion["config"]["simulacro"].comoBool())
            s->mensaje = L"Simulacro de examen: tienes un minuto por pregunta, los fallos restan y la nota sale al entregar. ¡Suerte!";
        if (s->sesion["config"]["repaso"].comoBool()) s->mensaje = L"Repaso de las preguntas que fallaste. Esta vez seguro que sí.";
        s->hola["pendiente"] = js::Valor();
        irAVista(s, V_PREGUNTA);
    } else if (orden == "pregunta" || orden == "responder" || orden == "guardar") {
        ponerPregunta(s, r["pregunta"]);
        if (orden == "responder" && r["pregunta"].contiene("mensaje")) s->mensaje = W(r["pregunta"]["mensaje"]);
        if (orden == "responder" && r["pregunta"]["corregida"].comoBool()) s->irACorreccion = true;
        if (orden == "guardar") s->mensaje = W(r["mensaje"]);
        if (s->vista != V_PREGUNTA) irAVista(s, V_PREGUNTA);
        if (orden == "responder" && esExamen(s)) {
            int i = s->pregunta["indice"].comoEntero(), n = s->sesion["n"].comoEntero();
            if (i + 1 < n) pedirPregunta(s, i + 1);  // en examen se avanza solo
        }
    } else if (orden == "terminar") {
        s->entregando = false;
        s->resultado = r["resultado"];
        s->sesion = r["sesion"];
        s->hola["resumen"] = r["resumen"];
        s->generacion = js::Valor();
        double nota = s->resultado["nota"].comoNumero();
        if (nota >= 9) s->mensaje = L"¡Sobresaliente! Dominas este tema.";
        else if (nota >= 7) s->mensaje = L"¡Muy buen trabajo! Repasa los fallos y estarás listo.";
        else if (nota >= 5) s->mensaje = L"Aprobado. Con un repaso de los fallos lo bordarás.";
        else if (s->resultado["fallos_guardados"].comoEntero() > 0)
            s->mensaje = L"No te preocupes: tus fallos quedan guardados para repasarlos conmigo.";
        else s->mensaje = L"No te preocupes: revisa las explicaciones y vuelve a intentarlo.";
        if (s->resultado["fallos"].comoEntero() == 0 && nota >= 5)
            s->mensaje = s->resultado["blanco"].comoEntero() ? L"Ningún fallo. Ahora a por las que dejaste en blanco."
                                                            : L"¡Pleno! Ni un solo fallo.";
        if (s->resultado["simulacro"].comoBool())
            s->mensaje = nota >= 5 ? L"Simulacro aprobado. Revisa los fallos: en el examen de verdad no se escapará ninguno."
                                   : L"El simulacro no ha salido, y para eso está. Repasa los fallos y vuelve a intentarlo.";
        enviar(s, "inicio");  // el plan del dia cambia al terminar un test
        irAVista(s, V_RESULTADO);
        lanzarAnim(s, s->animResultado, 150);  // la nota sube cuando ya ha acabado el fundido
    } else if (orden == "progreso") {
        s->progreso = r;
        s->hola["resumen"] = r["resumen"];
        s->mensaje = L"Este es tu recorrido. Los temas con menos aciertos son los que más conviene practicar.";
        if (s->analisis.esNulo() && !s->tareaAnalisis.activa()) {
            js::Valor p = js::Valor::objeto();
            p["generar"] = false;  // solo el ultimo analisis guardado, si lo hay
            enviar(s, "analisis", p);
        }
        irAVista(s, V_PROGRESO);
    } else if (orden == "fallos" || orden == "quitar_fallo") {
        s->fallos = r["fallos"];
        if (r.contiene("resumen")) s->hola["resumen"] = r["resumen"];
        s->hola["resumen"]["mis_fallos"] = (int)s->fallos.tam();
        if (orden == "quitar_fallo") {
            int q = r["quitadas"].comoEntero();
            s->mensaje = q == 1 ? L"Quitada de «Mis fallos»." : L"He quitado " + std::to_wstring(q) + L" preguntas de «Mis fallos».";
            s->resultado["acertadas_guardadas"] = 0;
        } else {
            s->mensaje = s->fallos.tam() ? L"Aquí están las preguntas que se te resisten. ¡A por ellas!"
                                         : L"No tienes fallos guardados. ¡Así me gusta!";
        }
        if (s->vista != V_RESULTADO || orden == "fallos") irAVista(s, V_FALLOS);
    } else if (orden == "rellenar") {
        s->sesion = r["sesion"];
        s->generacion = js::Valor();
        s->mensaje = L"He completado el test con preguntas rápidas.";
        pedirPregunta(s, s->pregunta["indice"].comoEntero());
    } else if (orden == "arrancar_ia" || orden == "ia") {
        s->hola["ia"] = r["ia"];
        if (r["ia"]["disponible"].comoBool()) {
            s->mensaje = L"IA conectada. Ya puedes usar los modos Mixto y Solo IA.";
            s->iaFase = "cargando";
            enviar(s, "precalentar");
        }
    } else if (orden == "duda") {
        s->chatEnCurso = false;
        if (!s->chat.empty()) {
            s->chat.back().texto = W(r["respuesta"]);
            s->chat.back().fuentes = r["fuentes"];
            s->chat.back().enCurso = false;
        }
        s->chatAlFinal = true;
        s->mensaje = L"¿Ha quedado claro? Puedes repreguntarme lo que quieras.";
    } else if (orden == "descartar") {
        s->mensaje = W(r["mensaje"]);
    } else if (orden == "reserva") {
        s->hola["resumen"]["ia_nuevas"] = r["listas"].comoEntero();
        if (!r["reserva"].comoBool()) s->reserva = js::Valor();
        s->mensaje = r["reserva"].comoBool() ? L"Iré dejando preguntas de IA preparadas cuando la IA esté libre."
                                             : L"Vale, solo usaré la IA cuando me lo pidas.";
    } else {
        estudioRespuesta(s, orden, r);
    }
    InvalidateRect(s->ventana, NULL, FALSE);
}

// ================================================================ pintado

// ---------------------------------------------------------------- inicio

int pintarInicio(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& resumen = s->hola["resumen"];
    const js::Valor& archivos = s->hola["archivos"];

    int paginas = 0, legibles = 0;
    for (std::size_t i = 0; i < archivos.tam(); ++i) {
        paginas += archivos[i]["paginas"].comoEntero();
        if (archivos[i]["fragmentos"].comoEntero() > 0) ++legibles;
    }
    wchar_t sub[200];
    swprintf(sub, 200, L"%ls  ·  %ls  ·  %d preguntas de IA (%d sin estrenar)  ·  %d en «Mis fallos»",
             plural(legibles, L"documento", L"documentos").c_str(), plural(paginas, L"página", L"páginas").c_str(),
             resumen["banco_ia"].comoEntero(), resumen["ia_nuevas"].comoEntero(), resumen["mis_fallos"].comoEntero());
    {
        // Cabecera con icono, como el resto de herramientas
        int lado = E(s, 46);
        redondeado(dc, rect(x, y, lado, lado), E(s, 13), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.22 : 0.16),
                   mezcla(tm.panel, tm.acento, 0.45));
        icono(s, dc, I_TEST, x + lado / 2, y + lado / 2, E(s, 24), tm.acento);
        texto(dc, s->f.titulo, tm.texto, L"Nuevo test", rect(x + lado + E(s, 16), y - E(s, 4), w, E(s, 34)),
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        texto(dc, s->f.texto, tm.textoSuave, sub, rect(x + lado + E(s, 16), y + E(s, 30), w - lado - E(s, 16), E(s, 22)),
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += lado + E(s, 26);
    }

    const js::Valor& pend = s->hola["pendiente"];
    if (!pend.esNulo()) {
        RECT r = rect(x, y, w, E(s, 68));
        redondeado(dc, r, E(s, 14), mezcla(tm.panel, tm.acento, 0.12), mezcla(tm.borde, tm.acento, 0.6));
        wchar_t b[160];
        swprintf(b, 160, L"Tienes un test a medias: %d de %d respondidas", pend["respondidas"].comoEntero(),
                 pend["n"].comoEntero());
        texto(dc, s->f.negrita, tm.texto, b, rect(x + E(s, 18), y + E(s, 12), w - E(s, 200), E(s, 22)),
              DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.peque, tm.textoSuave, L"Retómalo justo donde lo dejaste.",
              rect(x + E(s, 18), y + E(s, 36), w - E(s, 200), E(s, 20)), DT_LEFT | DT_SINGLELINE);
        boton(s, dc, rect(r.right - E(s, 160), y + E(s, 15), E(s, 142), E(s, 38)), L"Continuar", A_CONTINUAR, 0, PRIMARIO);
        y += E(s, 68) + E(s, 20);
    }

    // Configuracion (la altura se calcula antes de pintar la tarjeta)
    bool iaOk = s->hola["ia"]["disponible"].comoBool();
    const js::Valor& modelos = s->hola["ia"]["modelos"];
    bool filaModelo = (iaOk && modelos.tam() > 1) || (!iaOk && s->hola["ia"]["instalado"].comoBool());
    // En pantallas anchas: configuracion a la izquierda y temas a la derecha.
    bool dosColumnas = w >= E(s, 1000);
    int wConfig = dosColumnas ? w * 57 / 100 : w;
    int ancho1 = E(s, 120);
    // Formatos de pregunta: pastillas que pueden ocupar varias lineas
    const char* clavesFormato[] = {"definicion", "completar", "aplicacion", "relacion", "razonamiento", "falsa"};
    const wchar_t* nombresFormato[] = {L"Definiciones", L"Completar", L"Casos prácticos", L"Relacionar", L"Razonamiento",
                                       L"¿Cuál es falsa?"};
    int lineasFormato = 1;
    {
        int disponible = wConfig - E(s, 44) - ancho1, px = 0;
        for (const wchar_t* nombre : nombresFormato) {
            int cw = anchoTexto(dc, s->f.negrita, nombre) + E(s, 28);
            if (px > 0 && px + cw > disponible) {
                ++lineasFormato;
                px = 0;
            }
            px += cw + E(s, 8);
        }
    }
    int alturaConfig = E(s, 18 + 40 + 86 + 4 * 46 + 50 + 38 * 4 + 8) + (filaModelo ? E(s, 46) : 0) +
                       lineasFormato * E(s, 40) + E(s, 30);
    int xt = dosColumnas ? x + wConfig + E(s, 20) : x;
    int wt = dosColumnas ? w - wConfig - E(s, 20) : w;
    int yArriba = y;
    RECT tarjeta = rect(x, y, wConfig, alturaConfig);
    cristal(dc, tarjeta, E(s, 16));
    int cx = x + E(s, 22), cy = y + E(s, 18), cwid = wConfig - E(s, 44);
    texto(dc, s->f.subtitulo, tm.texto, L"Configura tu test", rect(cx, cy, cwid, E(s, 26)), DT_LEFT | DT_SINGLELINE);
    cy += E(s, 40);
    auto fila = [&](const wchar_t* nombre) {
        texto(dc, s->f.texto, tm.textoSuave, nombre, rect(cx, cy, ancho1, E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    };

    std::string modo = cfg(s, "modo", "mixto");
    fila(L"Modo");
    {
        int px = cx + ancho1;
        const char* modos[] = {"rapido", "mixto", "ia"};
        const wchar_t* nombres[] = {L"Rápido · sin IA", L"Mixto", L"Solo IA"};
        for (int i = 0; i < 3; ++i) px += chip(s, dc, px, cy, nombres[i], modo == modos[i], A_MODO, i) + E(s, 8);
    }
    cy += E(s, 40);
    std::wstring explicacion;
    if (modo == "rapido") explicacion = L"Al instante y sin IA: preguntas sacadas directamente de las frases de tus apuntes.";
    else if (modo == "mixto") explicacion = L"Empieza al instante; la IA sustituye las preguntas que aún no has visto por otras redactadas por ella.";
    else if (iaExterna(s)) explicacion = L"Todas redactadas por la IA (" + servidorIa(s) + L"). Las que falten se preparan mientras respondes.";
    else explicacion = L"Todas redactadas por la IA local. Las que falten se preparan mientras respondes (≈ 45 s cada una en este equipo).";
    if (modo != "rapido" && !iaOk) explicacion = L"La IA (" + servidorIa(s) + L") no está disponible ahora: se usará el modo Rápido.";
    texto(dc, s->f.peque, modo != "rapido" && !iaOk ? tm.medio : tm.textoSuave, explicacion,
          rect(cx + ancho1, cy, cwid - ancho1, E(s, 40)), DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    cy += E(s, 46);

    fila(L"Preguntas");
    {
        int px = cx + ancho1, n = s->config["n"].comoEntero(10);
        const int cantidades[] = {5, 10, 15, 20, 25, 30};
        for (int i = 0; i < 6; ++i)
            px += chip(s, dc, px, cy, std::to_wstring(cantidades[i]), n == cantidades[i], A_CANTIDAD, cantidades[i]) + E(s, 8);
    }
    cy += E(s, 46);
    fila(L"Dificultad");
    {
        int px = cx + ancho1;
        std::string d = cfg(s, "dificultad", "Intermedia");
        const char* difs[] = {"Básica", "Intermedia", "Avanzada"};
        for (int i = 0; i < 3; ++i) px += chip(s, dc, px, cy, W(std::string(difs[i])), d == difs[i], A_DIFICULTAD, i) + E(s, 8);
    }
    cy += E(s, 46);
    fila(L"Tipo");
    {
        int px = cx + ancho1;
        std::string t = cfg(s, "tipo", "variado");
        const char* tipos[] = {"variado", "opcion", "vf"};
        const wchar_t* nombres[] = {L"Variado", L"Opción múltiple", L"Verdadero / falso"};
        for (int i = 0; i < 3; ++i) px += chip(s, dc, px, cy, nombres[i], t == tipos[i], A_TIPO, i) + E(s, 8);
    }
    cy += E(s, 46);
    fila(L"Formatos");
    {
        // Seleccion multiple; sin ninguno marcado en la configuracion entran todos
        const js::Valor& elegidos = s->config["formatos"];
        int px = cx + ancho1;
        for (int i = 0; i < 6; ++i) {
            bool marcado = elegidos.tam() == 0;
            for (std::size_t j = 0; j < elegidos.tam(); ++j)
                if (elegidos[j].comoCadena() == clavesFormato[i]) marcado = true;
            int cw = anchoTexto(dc, s->f.negrita, nombresFormato[i]) + E(s, 28);
            if (px > cx + ancho1 && px + cw > cx + cwid) {
                px = cx + ancho1;
                cy += E(s, 40);
            }
            px += chip(s, dc, px, cy, nombresFormato[i], marcado, A_FORMATO, i) + E(s, 8);
        }
        cy += E(s, 40);
        const wchar_t* nota = modo == "rapido"
                                  ? L"Sin IA: definiciones, huecos y «¿cuál es falsa?»; el resto lo redacta la IA."
                                  : L"La IA reparte estos formatos por el test y, con varios temas, también los relaciona.";
        texto(dc, s->f.peque, tm.textoSuave, nota, rect(cx + ancho1, cy - E(s, 2), cwid - ancho1, E(s, 22)),
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        cy += E(s, 30);
    }
    fila(L"Corrección");
    {
        int px = cx + ancho1;
        bool final = cfg(s, "correccion", "inmediata") == "final";
        px += chip(s, dc, px, cy, L"Al momento", !final, A_CORRECCION, 0) + E(s, 8);
        chip(s, dc, px, cy, L"Al final · modo examen", final, A_CORRECCION, 1);
    }
    cy += E(s, 46);
    fila(L"Tiempo");
    {
        int px = cx + ancho1, limite = s->config["limite"].comoEntero(0);
        const int valores[] = {0, 60, 120};
        const wchar_t* nombres[] = {L"Sin límite", L"1 min por pregunta", L"2 min por pregunta"};
        for (int i = 0; i < 3; ++i) px += chip(s, dc, px, cy, nombres[i], limite == valores[i], A_LIMITE, valores[i]) + E(s, 8);
    }
    cy += E(s, 50);
    fila(L"Opciones");
    interruptor(s, dc, cx + ancho1, cy, s->config["auto_guardar"].comoBool(true),
                L"Guardar mis fallos automáticamente en «Mis fallos»", A_AUTOGUARDAR);
    cy += E(s, 38);
    interruptor(s, dc, cx + ancho1, cy, s->config["penalizar"].comoBool(), L"Restar los fallos (como en el examen real)",
                A_PENALIZAR);
    cy += E(s, 38);
    interruptor(s, dc, cx + ancho1, cy, s->config["revision"].comoBool(),
                L"Verificación doble de la IA (más fiable, más lenta)", A_REVISION);
    cy += E(s, 38);
    interruptor(s, dc, cx + ancho1, cy, s->config["reserva"].comoBool(true),
                L"Ir preparando preguntas de IA en segundo plano", A_RESERVA);
    cy += E(s, 40);
    if (iaOk && modelos.tam() > 1) {
        fila(L"Modelo IA");
        int px = cx + ancho1;
        std::string actual = cfg(s, "modelo");
        if (actual.empty()) actual = s->hola["ia"]["modelo"].comoCadena();
        for (std::size_t i = 0; i < modelos.tam() && i < 5; ++i)
            px += chip(s, dc, px, cy, W(modelos[i]), modelos[i].comoCadena() == actual, A_MODELO, (int)i) + E(s, 8);
    } else if (!iaOk && s->hola["ia"]["instalado"].comoBool()) {
        fila(iaExterna(s) ? L"IA" : L"IA local");
        boton(s, dc, rect(cx + ancho1, cy, E(s, 190), E(s, 34)), iaExterna(s) ? L"Volver a conectar" : L"Iniciar Ollama", A_ARRANCAR_IA);
    }
    y = dosColumnas ? yArriba : tarjeta.bottom + E(s, 20);

    // Temas
    int seleccionados = 0;
    for (bool b : s->temas) seleccionados += b ? 1 : 0;
    wchar_t cab[96];
    if (seleccionados == (int)s->temas.size() && seleccionados > 1)
        swprintf(cab, 96, L"Qué entra  ·  todo el temario");
    else
        swprintf(cab, 96, dosColumnas ? L"Qué entra  ·  %d de %d" : L"Qué entra en el test  ·  %d de %d apartados", seleccionados,
                 (int)s->temas.size());
    texto(dc, s->f.subtitulo, tm.texto, cab, rect(xt, y, wt - E(s, 230), E(s, 30)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    boton(s, dc, rect(xt + wt - E(s, 226), y, E(s, 96), E(s, 30)), L"Todos", A_TODOS, 1, FANTASMA);
    boton(s, dc, rect(xt + wt - E(s, 124), y, E(s, 124), E(s, 30)), L"Abrir carpeta", A_ABRIR_TEMARIO, 0, FANTASMA);
    y += E(s, 40);
    int filaAlto = E(s, 46), subAlto = E(s, 42);
    std::vector<FilaTema> filas = filasPorTema(archivos);
    bool porTemas = hayTemas(filas);
    int altoFilas = 0;
    for (const FilaTema& f : filas) altoFilas += f.indice < 0 ? subAlto : filaAlto;
    if (s->temasIa == "trabajando") {
        spinner(s, dc, xt + E(s, 10), y + E(s, 11), E(s, 7));
        texto(dc, s->f.peque, tm.textoSuave, L"La IA está poniendo nombre a tus temas…",
              rect(xt + E(s, 26), y, wt - E(s, 26), E(s, 22)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += E(s, 30);
    }
    RECT lista = rect(xt, y, wt, std::max(filaAlto, altoFilas) + E(s, 8));
    cristal(dc, lista, E(s, 14));
    if (archivos.tam() == 0) {
        texto(dc, s->f.texto, tm.textoSuave, L"No hay documentos. Pulsa «Abrir carpeta» y copia allí tus apuntes.",
              rect(xt + E(s, 18), y, wt - E(s, 36), filaAlto), DT_LEFT | DT_VCENTER | DT_WORDBREAK);
    }
    int fy = y + E(s, 4);
    for (const FilaTema& f : filas) {
        if (f.indice < 0) {
            int elegidos = 0;
            for (std::size_t j = 0; j < archivos.tam(); ++j)
                if (j < s->temas.size() && s->temas[j] && W(archivos[j]["grupo"]) == f.grupo) ++elegidos;
            RECT r = rect(xt + E(s, 6), fy, wt - E(s, 12), subAlto);
            if (encima(s, A_TEMA_GRUPO, f.grupo_n)) redondeado(dc, r, E(s, 10), mezcla(tm.panelAlt, tm.acento, 0.06));
            RECT dentro = r;
            dentro.left += E(s, 6);
            dentro.right -= E(s, 12);
            wchar_t info[64];
            swprintf(info, 64, L"%d de %d", elegidos, f.miembros);
            subtituloTema(s, dc, dentro, f.grupo, info, tm.acento, elegidos == 0 ? 0 : elegidos == f.miembros ? 2 : 1);
            zona(s, r, A_TEMA_GRUPO, f.grupo_n);
            fy += subAlto;
            continue;
        }
        std::size_t i = (std::size_t)f.indice;
        const js::Valor& ar = archivos[i];
        RECT r = rect(xt + E(s, porTemas ? 30 : 6), fy, wt - E(s, porTemas ? 36 : 12), filaAlto);
        fy += filaAlto;
        bool err = !ar["error"].comoCadena().empty();
        if (encima(s, A_TEMA, (int)i)) redondeado(dc, r, E(s, 10), tm.panelAlt);
        casilla(s, dc, rect(r.left + E(s, 12), r.top + (filaAlto - E(s, 20)) / 2, E(s, 20), E(s, 20)),
                i < s->temas.size() && s->temas[i]);
        std::wstring info;
        if (err) {
            info = W(ar["error"]);
        } else {
            wchar_t b[96];
            swprintf(b, 96, L"%d págs.", ar["paginas"].comoEntero());
            info = b;
            int nia = ar["preguntas_ia"].comoEntero();
            if (nia) info += L"  ·  " + plural(nia, L"pregunta IA", L"preguntas IA");
        }
        int infoAncho = std::min(std::min(E(s, 330), wt * 2 / 5), anchoTexto(dc, s->f.peque, info) + E(s, 4));
        texto(dc, s->f.texto, err ? tm.textoSuave : tm.texto, W(ar["tema"]),
              rect(r.left + E(s, 46), r.top, r.right - r.left - E(s, 60) - infoAncho, filaAlto),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.peque, err ? tm.mal : tm.textoSuave, info, rect(r.right - infoAncho - E(s, 12), r.top, infoAncho, filaAlto),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        zona(s, r, A_TEMA, (int)i);
    }
    y = std::max(lista.bottom, dosColumnas ? tarjeta.bottom : 0L) + E(s, 20);
    return y - a.y;
}

void pieInicio(Estado* s, HDC dc, RECT pie) {
    int alto = E(s, 44), y = pie.top + (pie.bottom - pie.top - alto) / 2;
    int misFallos = s->hola["resumen"]["mis_fallos"].comoEntero();
    int seleccionados = 0;
    for (bool b : s->temas) seleccionados += b ? 1 : 0;
    boton(s, dc, rect(pie.right - E(s, 220), y, E(s, 220), alto), L"Empezar test  →", A_EMPEZAR, 0, PRIMARIO,
          seleccionados > 0);
    int x = pie.right - E(s, 450);
    if (misFallos > 0) {
        boton(s, dc, rect(x, y, E(s, 220), alto), L"Test de mis fallos (" + std::to_wstring(misFallos) + L")", A_TEST_FALLOS, 0,
              SECUNDARIO);
        x -= E(s, 230);
    }
    if (x >= pie.left)
        botonIcono(s, dc, rect(x, y, E(s, 220), alto), I_RELOJ, L"Simulacro de examen", A_SIMULACRO, 0, SECUNDARIO,
                   seleccionados > 0);
}

// --------------------------------------------------------------- pregunta

void partirEnunciado(const std::wstring& t, std::wstring& cabecera, std::wstring& cuerpo) {
    std::size_t salto = t.find(L'\n');
    if (salto != std::wstring::npos && salto < 60) {
        cabecera = t.substr(0, salto);
        cuerpo = t.substr(salto + 1);
    } else {
        cabecera.clear();
        cuerpo = t;
    }
}

// Tarjeta con la figura del apunte; si no hay, una portada del tema.
void tarjetaImagen(Estado* s, HDC dc, RECT r, const js::Valor& p) {
    const gui::Tema& tm = gui::tema();
    std::wstring ruta = W(p["imagen"]["archivo"]);
    Gdiplus::Image* im = imagen(s, ruta);
    int pieAlto = E(s, 30);
    if (im) {
        bool h = encima(s, A_ZOOM);
        redondeado(dc, r, E(s, 18), tm.panel, h ? tm.acento : tm.borde, h ? 2 : 1);
        RECT lienzo = {r.left + E(s, 10), r.top + E(s, 10), r.right - E(s, 10), r.bottom - pieAlto};
        redondeado(dc, lienzo, E(s, 12), RGB(255, 255, 255));
        RECT dentro = lienzo;
        InflateRect(&dentro, -E(s, 6), -E(s, 6));
        imagenAjustada(im, dc, dentro);
        texto(dc, s->f.mini, tm.textoSuave, W(p["imagen"]["pie"]) + L"  ·  clic para ampliar",
              rect(r.left + E(s, 14), r.bottom - pieAlto, r.right - r.left - E(s, 28), pieAlto),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        zona(s, r, A_ZOOM);
        return;
    }
    // Portada del tema: degradado con sus iniciales
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath camino;
        caminoRedondeado(camino, r, E(s, 18));
        Gdiplus::LinearGradientBrush degradado(Gdiplus::PointF((float)r.left, (float)r.top),
                                               Gdiplus::PointF((float)r.right, (float)r.bottom),
                                               gc(mezcla(tm.panel, tm.acento, 0.55)), gc(mezcla(tm.panel, RGB(167, 139, 250), 0.35)));
        g.FillPath(&degradado, &camino);
    }
    std::wstring tema = W(p["tema"]), iniciales;
    bool nueva = true;
    for (wchar_t c : tema) {
        if (iswspace(c)) nueva = true;
        else if (nueva && iswalpha(c) && iniciales.size() < 2) {
            iniciales += (wchar_t)towupper(c);
            nueva = false;
        } else nueva = false;
    }
    texto(dc, s->f.iniciales, mezcla(tm.texto, tm.acento, 0.2), iniciales,
          rect(r.left, r.top, r.right - r.left, (r.bottom - r.top) * 2 / 3), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    texto(dc, s->f.negrita, tm.texto, tema, rect(r.left + E(s, 16), r.bottom - E(s, 70), r.right - r.left - E(s, 32), E(s, 40)),
          DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    texto(dc, s->f.mini, tm.textoSuave, L"Tema del apunte", rect(r.left, r.bottom - E(s, 30), r.right - r.left, E(s, 20)),
          DT_CENTER | DT_SINGLELINE);
}

// Color de cada formato de pregunta (el mismo en la pregunta y en los resultados).
COLORREF colorFormato(const std::string& f) {
    const gui::Tema& tm = gui::tema();
    if (f == "completar") return RGB(20, 184, 166);
    if (f == "aplicacion") return RGB(245, 158, 11);
    if (f == "relacion") return RGB(236, 72, 153);
    if (f == "razonamiento") return RGB(139, 92, 246);
    if (f == "falsa") return tm.mal;
    if (f == "vf") return mezcla(tm.textoSuave, tm.acento, 0.3);
    return tm.acento;
}

// Aciertos seguidos hasta la ultima pregunta corregida (las sin responder no cortan la racha).
int rachaTest(Estado* s) {
    const js::Valor& estados = s->sesion["estados"];
    int racha = 0;
    for (std::size_t i = estados.tam(); i-- > 0;) {
        std::string e = estados[i].comoCadena();
        if (e == "bien") ++racha;
        else if (e == "mal") break;
    }
    return racha;
}

// Una tecla dibujada como en el teclado. Devuelve su ancho.
int tecla(Estado* s, HDC dc, int x, int y, const std::wstring& t) {
    const gui::Tema& tm = gui::tema();
    int w = std::max(E(s, 24), anchoTexto(dc, s->f.mini, t) + E(s, 14)), h = E(s, 22);
    redondeado(dc, rect(x, y + E(s, 2), w, h), E(s, 6), mezcla(tm.borde, tm.fondo, 0.2));  // sombra inferior
    redondeado(dc, rect(x, y, w, h), E(s, 6), tm.panelAlt, tm.borde);
    texto(dc, s->f.mini, tm.texto, t, rect(x, y, w, h), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return w;
}

// Fila de atajos: grupos de teclas, cada uno con lo que hace.
void filaAtajos(Estado* s, HDC dc, int x, int y, const std::vector<std::pair<std::vector<std::wstring>, std::wstring>>& atajos) {
    const gui::Tema& tm = gui::tema();
    for (std::size_t i = 0; i < atajos.size(); ++i) {
        if (i) {
            texto(dc, s->f.peque, mezcla(tm.textoSuave, tm.fondo, 0.4), L"·", rect(x, y, E(s, 20), E(s, 24)),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            x += E(s, 20);
        }
        for (const std::wstring& t : atajos[i].first) x += tecla(s, dc, x, y, t) + E(s, 4);
        int tw = anchoTexto(dc, s->f.peque, atajos[i].second) + E(s, 6);
        texto(dc, s->f.peque, tm.textoSuave, atajos[i].second, rect(x + E(s, 2), y, tw, E(s, 24)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += tw + E(s, 4);
    }
}

// Pastilla «No estoy seguro» (tecla S). Devuelve su ancho.
int pastillaDuda(Estado* s, HDC dc, int x, int y) {
    const gui::Tema& tm = gui::tema();
    std::wstring t = L"No estoy seguro";
    int w = anchoTexto(dc, s->f.peque, t) + E(s, 84), h = E(s, 30);
    RECT r = rect(x, y - E(s, 3), w, h);
    bool h2 = encima(s, A_DUDOSA);
    COLORREF c = tm.medio;
    redondeado(dc, r, h / 2, s->dudosa ? mezcla(tm.panel, c, 0.22) : (h2 ? tm.panelAlt : tm.panel),
               s->dudosa ? c : mezcla(tm.borde, c, h2 ? 0.5 : 0.0), s->dudosa ? 2 : 1);
    casilla(s, dc, rect(r.left + E(s, 10), r.top + (h - E(s, 16)) / 2, E(s, 16), E(s, 16)), s->dudosa);
    texto(dc, s->f.peque, s->dudosa ? tm.texto : tm.textoSuave, t, rect(r.left + E(s, 34), r.top, w - E(s, 70), h),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    tecla(s, dc, r.right - E(s, 32), r.top + (h - E(s, 22)) / 2, L"S");
    zona(s, r, A_DUDOSA);
    return w;
}

int pintarPregunta(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& p = s->pregunta;
    const js::Valor& estados = s->sesion["estados"];
    int n = s->sesion["n"].comoEntero(1), indice = p["indice"].comoEntero();

    wchar_t cab[64];
    swprintf(cab, 64, L"Pregunta %d de %d", indice + 1, n);
    texto(dc, s->f.titulo, tm.texto, cab, rect(x, y, w, E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    std::wstring derecha;
    COLORREF colorReloj = tm.textoSuave;
    int restantes = segundosRestantes(s);
    if (testTerminado(s)) {
        derecha = L"Revisión del test";
    } else if (restantes >= 0) {
        derecha = L"Quedan  " + tiempo(restantes);
        colorReloj = restantes <= 60 ? tm.mal : restantes <= 180 ? tm.medio : tm.texto;
    } else {
        derecha = L"Tiempo  " + tiempo(s->segundosBase + (int)((GetTickCount64() - s->relojBase) / 1000));
    }
    texto(dc, restantes >= 0 ? s->f.negrita : s->f.texto, colorReloj, derecha, rect(x, y, w, E(s, 34)),
          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    y += E(s, 42);

    // Puntos de progreso (clicables)
    {
        int hueco = E(s, 5);
        int pw = std::max(E(s, 8), std::min(E(s, 40), (w - (n - 1) * hueco) / std::max(1, n)));
        for (int i = 0; i < n; ++i) {
            std::string e = estados[(std::size_t)i].comoCadena();
            COLORREF c = e == "bien" ? tm.ok : e == "mal" ? tm.mal : e == "respondida" ? tm.acento
                       : e == "preparando" ? mezcla(tm.panelAlt, tm.medio, 0.35)
                       : mezcla(tm.panelAlt, tm.textoSuave, tm.oscuro ? 0.15 : 0.35);
            // La actual, mas alta y con un halo que aparece al llegar a ella
            RECT r = i == indice ? rect(x + i * (pw + hueco), y - E(s, 2), pw, E(s, 14)) : rect(x + i * (pw + hueco), y, pw, E(s, 10));
            if (i == indice) {
                double t = frenado(avanceAnim(s->animPregunta, 500));
                RECT g = r;
                InflateRect(&g, E(s, 3), E(s, 3));
                redondeado(dc, g, E(s, 9), mezcla(tm.fondo, tm.acento, 0.25 * t));
            }
            redondeado(dc, r, E(s, i == indice ? 7 : 5), c, i == indice ? tm.texto : CLR_INVALID, i == indice ? 2 : 1);
            RECT z = r;
            InflateRect(&z, 0, E(s, 6));
            zona(s, z, A_IR, i);
        }
        y += E(s, 26);
    }

    if (p["preparando"].comoBool()) {
        RECT r = rect(x, y, w, E(s, 270));
        cristal(dc, r, E(s, 18));
        spinner(s, dc, x + w / 2, y + E(s, 70), E(s, 26));
        if (!s->generacion.esNulo()) {
            // Barra del lote que se esta redactando (la IA escribe tres preguntas a la vez)
            int bw = std::min(E(s, 360), w - E(s, 80));
            barra(dc, rect(x + (w - bw) / 2, y + E(s, 180), bw, E(s, 8)), s->generacion["progreso"].comoEntero() / 100.0, tm.acento);
        }
        texto(dc, s->f.subtitulo, tm.texto, L"La IA está redactando esta pregunta…", rect(x, y + E(s, 112), w, E(s, 28)),
              DT_CENTER | DT_SINGLELINE);
        std::wstring detalle = L"Puedes repasar las anteriores mientras tanto.";
        if (!s->generacion.esNulo()) {
            wchar_t b[160];
            swprintf(b, 160, L"Tema: %ls  ·  %d\u00A0%%", W(s->generacion["tema"]).c_str(), s->generacion["progreso"].comoEntero());
            detalle = b;
        }
        texto(dc, s->f.texto, tm.textoSuave, detalle, rect(x, y + E(s, 146), w, E(s, 24)), DT_CENTER | DT_SINGLELINE);
        boton(s, dc, rect(x + w / 2 - E(s, 150), y + E(s, 208), E(s, 300), E(s, 40)), L"No esperar: usar preguntas rápidas",
              A_RELLENAR);
        return r.bottom - a.y + E(s, 20);
    }

    // Etiquetas: formato, dificultad, tema y origen; a la derecha, la racha de aciertos
    double entrada = frenado(avanceAnim(s->animPregunta, 420));
    {
        int racha = rachaTest(s), anchoRacha = 0;
        if (racha >= 2 && !esExamen(s)) {
            std::wstring t = L"Racha  ×" + std::to_wstring(racha);
            anchoRacha = anchoTexto(dc, s->f.peque, t) + E(s, 20);
            etiqueta(s, dc, x + w - anchoRacha, y, t, racha >= 5 ? RGB(245, 158, 11) : tm.ok);
            anchoRacha += E(s, 12);
        }
        int px = x;
        auto poner = [&](const std::wstring& t, COLORREF c) {
            if (px + anchoTexto(dc, s->f.peque, t) + E(s, 20) > x + w - anchoRacha) return;  // no cabe
            px += etiqueta(s, dc, px, y, t, c) + E(s, 8);
        };
        std::string formato = p["tipo"].comoCadena() == "vf" ? "vf" : p["formato"].comoCadena();
        std::wstring nombreFormato = W(p["formato_nombre"]);
        if (nombreFormato.empty()) nombreFormato = formato == "vf" ? L"Verdadero o falso" : L"Pregunta";
        poner(nombreFormato, colorFormato(formato));
        std::string dif = p["dificultad"].comoCadena();
        if (!dif.empty()) poner(W(dif), dif == "Avanzada" ? tm.mal : dif == "Básica" ? tm.ok : tm.medio);
        poner(W(p["tema"]), tm.acento);
        bool deIa = p["origen"].comoCadena() == "ia";
        poner(deIa ? L"Redactada por IA" : L"Pregunta rápida", deIa ? RGB(167, 139, 250) : tm.textoSuave);
        if (p["guardada"].comoBool()) poner(L"En «Mis fallos»", tm.medio);
        else if (p["fallos_previos"].comoEntero() > 0) poner(L"La fallaste antes", tm.medio);
        y += E(s, 38);
    }
    y += (int)((1.0 - entrada) * E(s, 22));  // la pregunta entra subiendo

    // Enunciado e imagen, lado a lado (o la imagen encima si no hay anchura)
    std::wstring cabecera, cuerpo;
    partirEnunciado(W(p["enunciado"]), cabecera, cuerpo);
    int pad = E(s, 24);
    bool aLado = w >= E(s, 720);
    int anchoImg = aLado ? std::min(w * 38 / 100, E(s, 440)) : w;
    int anchoTarjeta = aLado ? w - anchoImg - E(s, 16) : w;
    int hCab = cabecera.empty() ? 0 : altoTexto(dc, s->f.texto, cabecera, anchoTarjeta - 2 * pad) + E(s, 8);
    int hCuerpo = altoTexto(dc, s->f.pregunta, cuerpo, anchoTarjeta - 2 * pad);
    int altoTarjeta = hCab + hCuerpo + 2 * pad;
    if (aLado) {
        altoTarjeta = std::max(altoTarjeta, std::min(E(s, 330), anchoImg * 3 / 4));
        tarjetaImagen(s, dc, rect(x + anchoTarjeta + E(s, 16), y, anchoImg, altoTarjeta), p);
    } else {
        tarjetaImagen(s, dc, rect(x, y, w, E(s, 230)), p);
        y += E(s, 230) + E(s, 16);
    }
    RECT tarjeta = rect(x, y, anchoTarjeta, altoTarjeta);
    cristal(dc, tarjeta, E(s, 18));
    redondeado(dc, rect(x, y + E(s, 18), E(s, 4), altoTarjeta - E(s, 36)), E(s, 2), tm.acento);
    int ty = y + std::max(pad, (altoTarjeta - hCab - hCuerpo) / 2);
    if (!cabecera.empty()) texto(dc, s->f.texto, tm.textoSuave, cabecera, rect(x + pad, ty, anchoTarjeta - 2 * pad, hCab));
    texto(dc, s->f.pregunta, tm.texto, cuerpo, rect(x + pad, ty + hCab, anchoTarjeta - 2 * pad, hCuerpo));
    y = tarjeta.bottom + E(s, 18);

    // Opciones
    const js::Valor& opciones = p["opciones"];
    bool corregida = p["corregida"].comoBool();
    int correcta = p["correcta"].comoEntero(-1);
    bool vf = p["tipo"].comoCadena() == "vf";
    double golpe = avanceAnim(s->animCorreccion, 650);  // 0..1 tras comprobar (1 = sin animacion)
    // Al corregir, cada opcion dice por que es correcta o en que falla
    const js::Valor& motivos = p["motivos"];
    bool conMotivos = corregida && motivos.tam() == opciones.tam();
    for (std::size_t i = 0; i < opciones.tam(); ++i) {
        std::wstring t = W(opciones[i]);
        int tw = w - E(s, 90);
        std::wstring motivo = conMotivos ? W(motivos[i]) : L"";
        int hTexto = altoTexto(dc, s->f.texto, t, tw);
        int hMotivo = motivo.empty() ? 0 : altoTexto(dc, s->f.peque, motivo, tw) + E(s, 6);
        int h = std::max(E(s, 58), hTexto + hMotivo + E(s, 30));
        // Entrada escalonada: cada opcion llega desde la derecha un poco despues que la anterior
        double llegada = frenado(avanceAnim(s->animPregunta ? s->animPregunta + 70 * (i + 1) : 0, 380));
        int ox = (int)((1.0 - llegada) * E(s, 46));
        bool elegida = s->seleccion == (int)i;
        if (corregida && golpe < 1 && elegida && (int)i != correcta)  // la fallada tiembla
            ox += (int)(std::sin(golpe * 3.14159265 * 6) * (1.0 - golpe) * E(s, 9));
        RECT r = rect(x + ox, y, w, h);
        bool hov = !corregida && !testTerminado(s) && encima(s, A_OPCION, (int)i);
        COLORREF fondo = tm.panel, borde = tm.borde, letraFondo = tm.panelAlt, letraTinta = tm.textoSuave, tinta = tm.texto;
        int grosor = 1;
        if (corregida) {
            if ((int)i == correcta) {
                fondo = mezcla(tm.panel, tm.ok, tm.oscuro ? 0.20 : 0.12);
                borde = tm.ok;
                grosor = 2;
            } else if (elegida) {
                fondo = mezcla(tm.panel, tm.mal, tm.oscuro ? 0.20 : 0.10);
                borde = tm.mal;
                grosor = 2;
            } else {
                tinta = tm.textoSuave;
            }
        } else if (elegida) {
            fondo = mezcla(tm.panel, tm.acento, tm.oscuro ? 0.22 : 0.12);
            borde = tm.acento;
            letraFondo = tm.acento;
            letraTinta = RGB(255, 255, 255);
            grosor = 2;
        } else if (hov) {
            fondo = tm.panelAlt;
            borde = mezcla(tm.borde, tm.acento, 0.5);
        }
        if (llegada < 1) tinta = mezcla(tinta, tm.panel, (1.0 - llegada) * 0.8);
        if (corregida && golpe < 1 && (int)i == correcta) {
            // Onda que se expande desde la respuesta buena
            int k = (int)(frenado(golpe) * E(s, 12));
            RECT onda = r;
            InflateRect(&onda, k, k);
            contorno(dc, onda, E(s, 14) + k, mezcla(tm.ok, tm.fondo, golpe), 0.5f + 2.5f * (float)(1.0 - golpe));
            fondo = mezcla(fondo, tm.ok, 0.22 * (1.0 - golpe));
        }
        cristal(dc, r, E(s, 14), fondo, borde, grosor);
        int cx = r.left + E(s, 32), cy = y + h / 2, radio = E(s, 16);
        if (corregida && ((int)i == correcta || elegida)) {
            // La marca aparece con un pequeño salto
            double esc = golpe < 1 ? 0.55 + 0.45 * frenado(std::min(1.0, golpe * 1.8)) + 0.12 * std::sin(std::min(1.0, golpe * 1.8) * 3.14159265) : 1.0;
            marca(dc, cx, cy, (int)(radio * esc), (int)i == correcta);
        } else {
            circulo(dc, cx, cy, radio, letraFondo);
            std::wstring letra = vf ? std::wstring(1, i == 0 ? L'V' : L'F') : std::wstring(1, (wchar_t)(L'A' + i));
            texto(dc, s->f.negrita, letraTinta, letra, rect(cx - radio, cy - radio, radio * 2, radio * 2),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        if (hov) redondeado(dc, rect(r.left, y + E(s, 12), E(s, 4), h - E(s, 24)), E(s, 2), tm.acento);
        int ty = y + (h - hTexto - hMotivo) / 2;
        texto(dc, s->f.texto, tinta, t, rect(r.left + E(s, 64), ty, tw, hTexto));
        if (!motivo.empty()) {
            COLORREF cm = (int)i == correcta ? mezcla(tm.ok, tm.texto, 0.25)
                          : elegida ? mezcla(tm.mal, tm.texto, 0.25) : tm.textoSuave;
            texto(dc, s->f.peque, cm, motivo, rect(r.left + E(s, 64), ty + hTexto + E(s, 6), tw, hMotivo));
        }
        if (!corregida && !testTerminado(s)) zona(s, r, A_OPCION, (int)i);
        y += h + E(s, 10);
    }

    // Correccion
    if (corregida) {
        y += E(s, 8) + (int)((1.0 - frenado(avanceAnim(s->animCorreccion, 450))) * E(s, 18));  // sube al aparecer
        bool acierto = p["acierto"].comoBool();
        std::wstring titulo = acierto ? (p["dudosa"].comoBool() ? L"¡Correcto! · como dudabas, te la repetiré pronto" : L"¡Correcto!")
                                      : L"No es correcta";
        if (!acierto && correcta >= 0) {
            titulo += L" · la respuesta buena es la ";
            titulo += vf ? (correcta == 0 ? L"V" : L"F") : std::wstring(1, (wchar_t)(L'A' + correcta));
        }
        std::wstring expl = W(p["explicacion"]);
        std::wstring cita = L"«" + W(p["cita"]) + L"»";
        std::wstring fuente = L"Fuente:  " + W(p["tema"]) + L"  ·  " + W(p["ubicacion"]);
        int iw = w - 2 * pad;
        int hExpl = altoTexto(dc, s->f.texto, expl, iw);
        int hCita = altoTexto(dc, s->f.peque, cita, iw - E(s, 18));
        int alto = pad + E(s, 34) + hExpl + E(s, 16) + hCita + E(s, 20) + E(s, 22) + E(s, 16) + E(s, 38) + pad;
        COLORREF color = acierto ? tm.ok : tm.mal;
        RECT r = rect(x, y, w, alto);
        redondeado(dc, r, E(s, 18), mezcla(tm.panel, color, tm.oscuro ? 0.08 : 0.05), mezcla(tm.borde, color, 0.55));
        int iy = y + pad;
        marca(dc, x + pad + E(s, 12), iy + E(s, 12), E(s, 12), acierto);
        texto(dc, s->f.subtitulo, color, titulo, rect(x + pad + E(s, 34), iy, iw - E(s, 34), E(s, 26)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        iy += E(s, 34);
        texto(dc, s->f.texto, tm.texto, expl, rect(x + pad, iy, iw, hExpl));
        iy += hExpl + E(s, 16);
        redondeado(dc, rect(x + pad, iy, E(s, 3), hCita), 1, mezcla(tm.panel, color, 0.6));
        texto(dc, s->f.peque, tm.textoSuave, cita, rect(x + pad + E(s, 16), iy, iw - E(s, 18), hCita));
        iy += hCita + E(s, 20);
        texto(dc, s->f.peque, tm.textoSuave, fuente, rect(x + pad, iy, iw, E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        iy += E(s, 22) + E(s, 16);
        int bx = x + pad, bh = E(s, 38);
        int bw = std::min(E(s, 210), (iw - 3 * E(s, 10)) / 4);
        botonIcono(s, dc, rect(bx, iy, bw, bh), I_LIBRO, L"Abrir el apunte", A_ABRIR_FUENTE);
        bx += bw + E(s, 10);
        // Acertada: no se ofrece «No lo entiendo» sino ampliar la explicacion (las dos las responde la IA)
        if (iaDisponible(s)) {
            botonIcono(s, dc, rect(bx, iy, bw, bh), I_DUDAS, acierto ? L"Explícamela más" : L"No lo entiendo", A_DUDA_PREGUNTA,
                       acierto ? 1 : 0);
            bx += bw + E(s, 10);
        }
        // «Mis fallos»: guardar solo tiene sentido si se ha fallado; si ya estaba guardada se puede quitar
        // (sobre todo al acertarla). Una acertada que no estaba guardada no ofrece nada.
        bool guardada = p["guardada"].comoBool();
        if (guardada) {
            botonIcono(s, dc, rect(bx, iy, bw, bh), I_CHECK, L"Quitar de mis fallos", A_GUARDAR, 0, acierto ? PRIMARIO : SECUNDARIO);
            bx += bw + E(s, 10);
        } else if (!acierto) {
            botonIcono(s, dc, rect(bx, iy, bw, bh), I_FALLOS, L"Guardar en mis fallos", A_GUARDAR, 0, PRIMARIO);
            bx += bw + E(s, 10);
        }
        boton(s, dc, rect(bx, iy, bw, bh), L"Pregunta errónea", A_DESCARTAR, 0, FANTASMA);
        s->finCorreccion = r.bottom - a.y;
        y = r.bottom + E(s, 16);
    } else if (p["respondida"].comoBool() && p["examen"].comoBool()) {
        int dx = pastillaDuda(s, dc, x, y) + E(s, 16);
        texto(dc, s->f.peque, tm.textoSuave, L"Respuesta guardada. Puedes cambiarla hasta entregar el examen.",
              rect(x + dx, y, w - dx, E(s, 24)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += E(s, 38);
    } else if (!testTerminado(s)) {
        // Marcar «No estoy seguro» antes de comprobar: si aciertas, la pregunta vuelve pronto a repaso
        int dx = pastillaDuda(s, dc, x, y) + E(s, 20);
        if (vf) filaAtajos(s, dc, x + dx, y, {{{L"V", L"F"}, L"elegir"}, {{L"Intro"}, L"comprobar"}, {{L"←", L"→"}, L"moverte"}});
        else filaAtajos(s, dc, x + dx, y, {{{L"1", L"2", L"3", L"4"}, L"elegir"}, {{L"Intro"}, L"comprobar"}, {{L"←", L"→"}, L"moverte"}});
        y += E(s, 40);
    }
    return y - a.y;
}

void piePregunta(Estado* s, HDC dc, RECT pie) {
    const js::Valor& p = s->pregunta;
    int alto = E(s, 44), y = pie.top + (pie.bottom - pie.top - alto) / 2;
    int indice = p["indice"].comoEntero(), n = s->sesion["n"].comoEntero(1);
    RECT principal = rect(pie.right - E(s, 210), y, E(s, 210), alto);
    if (testTerminado(s)) {
        // Revision de un test ya corregido
        boton(s, dc, rect(pie.left, y, E(s, 230), alto), L"←  Volver a resultados", A_PESTANA, V_RESULTADO, SECUNDARIO,
              !s->resultado.esNulo());
        boton(s, dc, rect(principal.left - E(s, 160), y, E(s, 150), alto), L"Anterior", A_ANTERIOR, 0, SECUNDARIO, indice > 0);
        boton(s, dc, principal, L"Siguiente  →", A_SIGUIENTE, 0, PRIMARIO, indice + 1 < n);
        return;
    }
    bool examen = esExamen(s);
    boton(s, dc, rect(pie.left, y, E(s, 140), alto), L"←  Anterior", A_ANTERIOR, 0, SECUNDARIO, indice > 0);
    boton(s, dc, rect(pie.left + E(s, 150), y, E(s, 150), alto), examen ? L"Entregar" : L"Terminar test", A_TERMINAR, 0,
          FANTASMA);
    if (p["preparando"].comoBool()) {
        boton(s, dc, principal, L"Siguiente  →", A_SIGUIENTE, 0, SECUNDARIO, indice + 1 < n);
    } else if (!p["corregida"].comoBool() && !examen) {
        boton(s, dc, principal, L"Comprobar", A_COMPROBAR, 0, PRIMARIO, s->seleccion >= 0);
        boton(s, dc, rect(principal.left - E(s, 130), y, E(s, 120), alto), L"Saltar", A_SIGUIENTE, 0, FANTASMA, indice + 1 < n);
    } else if (indice + 1 < n) {
        boton(s, dc, principal, L"Siguiente  →", A_SIGUIENTE, 0, PRIMARIO);
    } else if (todasRespondidas(s)) {
        boton(s, dc, principal, examen ? L"Entregar examen" : L"Ver resultados", A_TERMINAR, 0, PRIMARIO);
    } else {
        boton(s, dc, principal, L"Ir a la primera sin hacer", A_SIGUIENTE, 0, SECUNDARIO);
    }
}

// --------------------------------------------------------------- resultado

int pintarResultado(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& r = s->resultado;
    double nota = r["nota"].comoNumero();
    int n = r["n"].comoEntero(1), aciertos = r["aciertos"].comoEntero(), fallos = r["fallos"].comoEntero(),
        blanco = r["blanco"].comoEntero();
    COLORREF color = gui::colorDeNota(nota);
    // La nota sube desde cero y las cifras cuentan hasta su valor
    double sube = frenado(avanceAnim(s->animResultado, 1200));
    double notaVista = nota * sube;
    auto cuenta = [&](int v) { return std::to_wstring((int)std::lround(v * sube)); };

    int alto = E(s, 210);
    RECT tarjeta = rect(x, y, w, alto);
    cristal(dc, tarjeta, E(s, 18));
    int cx = x + E(s, 120), cy = y + alto / 2, radio = E(s, 72);
    if (sube >= 1 && nota >= 5) {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        halo(g, (float)cx, (float)cy, radio * 1.45f, color, tm.oscuro ? 60 : 45);
    }
    arco(dc, cx, cy, radio, (float)E(s, 12), 0, 360, tm.panelAlt);
    if (notaVista > 0) arco(dc, cx, cy, radio, (float)E(s, 12), -90, (float)(360.0 * notaVista / 10.0), mezcla(tm.acento, color, sube));
    texto(dc, s->f.grande, tm.texto, numero(notaVista), rect(cx - radio, cy - E(s, 34), radio * 2, E(s, 50)),
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    texto(dc, s->f.peque, tm.textoSuave, L"sobre 10", rect(cx - radio, cy + E(s, 16), radio * 2, E(s, 20)),
          DT_CENTER | DT_SINGLELINE);
    int tx = x + E(s, 240), tw = w - E(s, 260);
    std::wstring titulo = calificacion(nota);
    if (r["de_fallos"].comoBool()) titulo += L"  ·  test de mis fallos";
    else if (r["simulacro"].comoBool()) titulo += L"  ·  simulacro";
    else if (r["repaso"].comoBool()) titulo += L"  ·  repaso";
    texto(dc, s->f.titulo, color, titulo, rect(tx, y + E(s, 40), tw, E(s, 36)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    wchar_t lin[200];
    int dudosas = r["dudosas"].comoEntero();
    wchar_t dudas[64] = L"";
    if (dudosas > 0) swprintf(dudas, 64, L"  ·  %d %ls", dudosas, dudosas == 1 ? L"acertada dudando" : L"acertadas dudando");
    swprintf(lin, 200, L"%d %ls de %d  ·  %d\u00A0%%%ls%ls", aciertos, aciertos == 1 ? L"acierto" : L"aciertos", n,
             aciertos * 100 / std::max(1, n), dudas, r["penalizar"].comoBool() ? L"  ·  los fallos restan" : L"");
    texto(dc, s->f.texto, tm.texto, lin, rect(tx, y + E(s, 84), tw, E(s, 24)), DT_LEFT | DT_SINGLELINE);
    std::string modo = r["modo"].comoCadena();
    std::wstring nombreModo = modo == "ia" ? L"Solo IA" : modo == "mixto" ? L"Mixto" : L"Rápido";
    texto(dc, s->f.peque, tm.textoSuave,
          L"Tiempo: " + tiempo(r["segundos"].comoEntero()) + L"   ·   Dificultad " + W(r["dificultad"]) + L"   ·   Modo " + nombreModo,
          rect(tx, y + E(s, 114), tw, E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    int guardados = r["fallos_guardados"].comoEntero();
    if (guardados > 0)
        texto(dc, s->f.peque, tm.medio,
              std::to_wstring(guardados) +
                  (guardados == 1 ? L" pregunta fallada está guardada" : L" preguntas falladas están guardadas") +
                  L" en «Mis fallos».",
              rect(tx, y + E(s, 142), tw, E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y = tarjeta.bottom + E(s, 16);

    int hueco = E(s, 12), cw = (w - 2 * hueco) / 3, ch = E(s, 80);
    cifra(s, dc, rect(x, y, cw, ch), cuenta(aciertos), L"Aciertos", tm.ok, I_CHECK);
    cifra(s, dc, rect(x + cw + hueco, y, cw, ch), cuenta(fallos), L"Fallos", fallos ? tm.mal : tm.texto, I_FALLOS);
    cifra(s, dc, rect(x + 2 * (cw + hueco), y, cw, ch), cuenta(blanco), L"En blanco", tm.textoSuave, I_PAUSA);
    y += ch + E(s, 24);

    int acertadas = r["acertadas_guardadas"].comoEntero();
    if (acertadas > 0) {
        RECT b = rect(x, y, w, E(s, 64));
        redondeado(dc, b, E(s, 14), mezcla(tm.panel, tm.ok, 0.10), mezcla(tm.borde, tm.ok, 0.5));
        texto(dc, s->f.negrita, tm.texto,
              L"Has acertado " + std::to_wstring(acertadas) + L" de las preguntas de «Mis fallos».",
              rect(x + E(s, 18), y, w - E(s, 290), E(s, 64)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        boton(s, dc, rect(b.right - E(s, 260), y + E(s, 12), E(s, 244), E(s, 40)), L"Quitarlas de mis fallos",
              A_QUITAR_ACERTADAS, 0, PRIMARIO);
        y += E(s, 64) + E(s, 20);
    }

    // Desgloses con barras que crecen una tras otra: por tema y por tipo de pregunta
    int barraN = 0;
    auto filaBarra = [&](const std::wstring& nombre, int ac, int t, COLORREF punto) {
        double fr = (double)ac / std::max(1, t);
        double crece = frenado(avanceAnim(s->animResultado ? s->animResultado + 500 + 90 * barraN++ : 0, 650));
        int nx = x;
        if (punto != CLR_INVALID) {
            circulo(dc, x + E(s, 6), y + E(s, 12), E(s, 5), punto);
            nx += E(s, 20);
        }
        texto(dc, s->f.texto, tm.texto, nombre, rect(nx, y, w * 45 / 100 - (nx - x), E(s, 24)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        barra(dc, rect(x + w * 47 / 100, y + E(s, 8), w * 40 / 100, E(s, 9)), fr * crece,
              fr >= 0.7 ? tm.ok : fr >= 0.5 ? tm.medio : tm.mal);
        texto(dc, s->f.peque, tm.textoSuave, std::to_wstring(ac) + L" / " + std::to_wstring(t),
              rect(x + w * 89 / 100, y, w * 11 / 100, E(s, 24)), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        y += E(s, 32);
    };
    const js::Valor& temas = r["por_tema"];
    if (temas.tam() > 1) {
        tituloSeccion(s, dc, x, y, w, L"Por temas");
        for (std::size_t i = 0; i < temas.tam(); ++i)
            filaBarra(W(temas[i]["tema"]), temas[i]["aciertos"].comoEntero(), temas[i]["total"].comoEntero(1), CLR_INVALID);
        y += E(s, 16);
    }
    const js::Valor& formatos = r["por_formato"];
    if (formatos.tam() > 1) {
        tituloSeccion(s, dc, x, y, w, L"Por tipo de pregunta", L"Dónde fallas: definiciones, casos prácticos, relacionar conceptos…");
        for (std::size_t i = 0; i < formatos.tam(); ++i)
            filaBarra(W(formatos[i]["formato"]), formatos[i]["aciertos"].comoEntero(), formatos[i]["total"].comoEntero(1),
                      colorFormato(formatos[i]["clave"].comoCadena()));
        y += E(s, 16);
    }

    tituloSeccion(s, dc, x, y, w, L"Revisa tus respuestas", L"Pulsa una pregunta para ver la explicación.");
    const js::Valor& lista = r["preguntas"];
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        std::string e = lista[i]["estado"].comoCadena();
        RECT fila = rect(x, y, w, E(s, 50));
        cristal(dc, fila, E(s, 12), encima(s, A_VER, (int)i) ? tm.panelAlt : tm.panel);
        if (e == "blanco") circulo(dc, x + E(s, 26), y + E(s, 25), E(s, 11), tm.panelAlt, tm.textoSuave);
        else marca(dc, x + E(s, 26), y + E(s, 25), E(s, 11), e == "bien");
        std::wstring nombreFormato = W(lista[i]["formato"]);
        int fw = nombreFormato.empty() || nombreFormato == L"—" ? 0 : anchoTexto(dc, s->f.peque, nombreFormato) + E(s, 20);
        if (fw)
            etiqueta(s, dc, fila.right - fw - E(s, 14), y + E(s, 12), nombreFormato, colorFormato(lista[i]["clave"].comoCadena()));
        if (lista[i]["dudosa"].comoBool()) {  // acertada sin estar seguro: volverá a repaso
            int dw = anchoTexto(dc, s->f.peque, L"Dudaste") + E(s, 20);
            etiqueta(s, dc, fila.right - fw - dw - E(s, 22), y + E(s, 12), L"Dudaste", tm.medio);
            fw += dw + E(s, 8);
        }
        texto(dc, s->f.texto, tm.texto, std::to_wstring(i + 1) + L".  " + unaLinea(W(lista[i]["enunciado"])),
              rect(x + E(s, 50), y, w - E(s, 80) - fw, E(s, 50)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        zona(s, fila, A_VER, (int)i);
        y += E(s, 58);
    }

    // Confeti para las notas altas: cae sobre la pantalla visible durante un par de segundos
    double caida = avanceAnim(s->animResultado, 2600);
    if (nota >= 8.5 && caida < 1 && gui::estilo().animaciones) {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        RECT c = s->cuerpo;
        int ancho = c.right - c.left, altoVisible = c.bottom - c.top;
        const COLORREF paleta[] = {tm.acento, tm.ok, RGB(245, 158, 11), RGB(236, 72, 153), RGB(139, 92, 246), RGB(20, 184, 166)};
        for (int i = 0; i < 70; ++i) {
            unsigned h = (unsigned)(i + 1) * 2654435761u;  // azar fijo por pieza
            double x0 = (h % 1000) / 1000.0, retraso = ((h >> 10) % 300) / 1000.0, velocidad = 0.8 + ((h >> 20) % 400) / 1000.0;
            double t = (caida - retraso) / (1.0 - retraso);
            if (t <= 0) continue;
            float px = (float)(c.left + x0 * ancho + std::sin(t * 9 + i) * E(s, 18));
            float py = (float)(c.top - E(s, 20) + t * velocidad * altoVisible * 1.1);
            int alfa = (int)(255 * std::min(1.0, (1.0 - caida) * 3));
            float lado = (float)E(s, 5 + i % 4);
            Gdiplus::SolidBrush pincel(gc(paleta[i % 6], alfa));
            g.TranslateTransform(px, py);
            g.RotateTransform((float)(t * 720 + i * 37));
            g.FillRectangle(&pincel, -lado / 2, -lado / 4, lado, lado / 2);
            g.ResetTransform();
        }
    }
    return y - a.y;
}

void pieResultado(Estado* s, HDC dc, RECT pie) {
    int alto = E(s, 44), y = pie.top + (pie.bottom - pie.top - alto) / 2;
    boton(s, dc, rect(pie.left, y, E(s, 200), alto), L"Revisar preguntas", A_VER, 0, SECUNDARIO);
    // Repetir solo las falladas y las que quedaron en blanco, sin esperar a nada
    int dudas = s->resultado["dudosas"].comoEntero();
    int repetir = s->resultado["fallos"].comoEntero() + s->resultado["blanco"].comoEntero() + dudas;
    if (repetir > 0 && !s->resultado["de_fallos"].comoBool() && pie.left + E(s, 450) < pie.right - E(s, 440))
        boton(s, dc, rect(pie.left + E(s, 210), y, E(s, 230), alto),
              (dudas ? L"Repetir fallos y dudas (" : L"Repetir las falladas (") + std::to_wstring(repetir) + L")",
              A_REPETIR_TEST, 0, SECUNDARIO);
    boton(s, dc, rect(pie.right - E(s, 200), y, E(s, 200), alto), L"Nuevo test", A_PESTANA, V_INICIO, PRIMARIO);
    int misFallos = s->hola["resumen"]["mis_fallos"].comoEntero();
    if (misFallos > 0)
        boton(s, dc, rect(pie.right - E(s, 430), y, E(s, 220), alto),
              L"Test de mis fallos (" + std::to_wstring(misFallos) + L")", A_TEST_FALLOS, 0, SECUNDARIO);
}

// -------------------------------------------------------------- mis fallos

int pintarFallos(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& lista = s->fallos;
    int total = (int)lista.tam();
    y += cabecera(s, dc, x, y, w, I_FALLOS, tm.medio, L"Mis fallos",
                  std::wstring(L"Las preguntas que fallas se guardan aquí. Haz tests solo con ellas hasta dominarlas; cuando las "
                               L"aciertes, puedes quitarlas.") +
                      (iaDisponible(s) ? L" «Explicar» le pide al profesor que te la aclare." : L""));

    if (total == 0) {
        RECT r = rect(x, y, w, E(s, 200));
        cristal(dc, r, E(s, 18));
        marca(dc, x + w / 2, y + E(s, 60), E(s, 26), true);
        texto(dc, s->f.subtitulo, tm.texto, L"No tienes fallos guardados", rect(x, y + E(s, 100), w, E(s, 28)),
              DT_CENTER | DT_SINGLELINE);
        texto(dc, s->f.texto, tm.textoSuave, L"Cuando falles una pregunta en un test aparecerá aquí.",
              rect(x, y + E(s, 134), w, E(s, 24)), DT_CENTER | DT_SINGLELINE);
        return r.bottom - a.y + E(s, 20);
    }

    // Cifras y temas distintos
    std::map<std::wstring, int> porTema;
    int enRacha = 0;
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        porTema[W(lista[i]["tema"])]++;
        if (lista[i]["racha"].comoEntero() > 0) ++enRacha;
    }
    int hueco = E(s, 12), cw = (w - 2 * hueco) / 3, ch = E(s, 80);
    cifra(s, dc, rect(x, y, cw, ch), std::to_wstring(total), L"Preguntas guardadas", tm.medio, I_FALLOS);
    cifra(s, dc, rect(x + cw + hueco, y, cw, ch), std::to_wstring((int)porTema.size()), L"Temas con fallos", tm.texto, I_MAPA);
    cifra(s, dc, rect(x + 2 * (cw + hueco), y, cw, ch), std::to_wstring(enRacha), L"Ya acertadas en un repaso", tm.ok, I_CHECK);
    y += ch + E(s, 20);

    // Tarjeta para lanzar el test
    RECT t = rect(x, y, w, E(s, 92));
    redondeado(dc, t, E(s, 16), mezcla(tm.panel, tm.acento, 0.10), mezcla(tm.borde, tm.acento, 0.55));
    texto(dc, s->f.negrita, tm.texto, L"Test de mis fallos", rect(x + E(s, 20), y + E(s, 14), w, E(s, 24)),
          DT_LEFT | DT_SINGLELINE);
    int px = x + E(s, 20), py = y + E(s, 44);
    texto(dc, s->f.texto, tm.textoSuave, L"Preguntas:", rect(px, py, E(s, 90), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    px += E(s, 92);
    const int cuantas[] = {5, 10, 20, 0};
    for (int c : cuantas) {
        if (c && c >= total) continue;
        std::wstring nombre = c ? std::to_wstring(c) : L"Todas (" + std::to_wstring(total) + L")";
        px += chip(s, dc, px, py, nombre, s->nFallos == c, A_N_FALLOS, c) + E(s, 8);
    }
    boton(s, dc, rect(t.right - E(s, 236), y + E(s, 24), E(s, 216), E(s, 44)), L"Empezar  →", A_TEST_FALLOS, 0, PRIMARIO);
    y = t.bottom + E(s, 24);

    tituloSeccion(s, dc, x, y, w, L"Preguntas guardadas");
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        const js::Valor& f = lista[i];
        std::wstring enun = unaLinea(W(f["enunciado"]));
        std::wstring resp = L"Respuesta: " + W(f["respuesta"]);
        int veces = f["veces"].comoEntero(), racha = f["racha"].comoEntero();
        std::wstring info = L"Fallada " + std::to_wstring(veces) + (veces == 1 ? L" vez" : L" veces");
        if (racha > 0) info += L"  ·  acertada " + std::to_wstring(racha) + (racha == 1 ? L" vez" : L" veces") + L" desde entonces";
        RECT fila = rect(x, y, w, E(s, 104));
        cristal(dc, fila, E(s, 14));
        int ix = x + E(s, 18), iw = w - E(s, 160);
        int ew = etiqueta(s, dc, ix, y + E(s, 12), W(f["tema"]), tm.acento);
        if (f.contiene("formato_nombre"))
            etiqueta(s, dc, ix + ew + E(s, 8), y + E(s, 12), W(f["formato_nombre"]), colorFormato(f["formato"].comoCadena()));
        texto(dc, s->f.negrita, tm.texto, enun, rect(ix, y + E(s, 44), iw, E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.peque, tm.ok, resp, rect(ix, y + E(s, 70), iw * 6 / 10, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.peque, racha ? tm.ok : tm.textoSuave, info, rect(ix + iw * 6 / 10 + E(s, 10), y + E(s, 70), iw * 4 / 10, E(s, 20)),
              DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);
        bool ia = iaDisponible(s);  // «Explicar» lo responde la IA
        if (ia)
            botonIcono(s, dc, rect(fila.right - E(s, 124), y + E(s, 14), E(s, 108), E(s, 36)), I_DUDAS, L"Explicar", A_EXPLICAR_FALLO,
                       (int)i, SECUNDARIO);
        boton(s, dc, rect(fila.right - E(s, 124), y + (ia ? E(s, 56) : E(s, 35)), E(s, 108), E(s, 34)), L"Quitar", A_QUITAR_FALLO, (int)i,
              FANTASMA);
        y += E(s, 114);
    }
    return y - a.y;
}

// ---------------------------------------------------------------- progreso

int pintarProgreso(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& res = s->progreso["resumen"];
    y += cabecera(s, dc, x, y, w, I_PROGRESO, tm.ok, L"Tu progreso",
                  L"Tus resultados en esta asignatura: precisión, notas, actividad y dominio de cada tema.");
    int hueco = E(s, 12), cw = (w - 3 * hueco) / 4, ch = E(s, 84);
    int prec = res["precision"].comoEntero(-1);
    cifra(s, dc, rect(x, y, cw, ch), prec < 0 ? L"—" : std::to_wstring(prec) + L"\u00A0%", L"Precisión global",
          prec < 0 ? tm.textoSuave : prec >= 70 ? tm.ok : prec >= 50 ? tm.medio : tm.mal, I_DIANA);
    int racha = res["racha"].comoEntero();
    cifra(s, dc, rect(x + cw + hueco, y, cw, ch), std::to_wstring(racha) + (racha == 1 ? L" día" : L" días"),
          L"Racha de estudio", racha >= 3 ? tm.ok : racha > 0 ? tm.medio : tm.textoSuave, I_FUEGO);
    cifra(s, dc, rect(x + 2 * (cw + hueco), y, cw, ch), std::to_wstring(res["tests"].comoEntero()), L"Tests", tm.texto, I_TEST);
    cifra(s, dc, rect(x + 3 * (cw + hueco), y, cw, ch), std::to_wstring(res["mis_fallos"].comoEntero()), L"En «Mis fallos»",
          res["mis_fallos"].comoEntero() ? tm.medio : tm.texto, I_FALLOS);
    y += ch + E(s, 20);
    y += pintarAnalisis(s, dc, x, y, w) + E(s, 28);

    const js::Valor& hist = s->progreso["historial"];
    tituloSeccion(s, dc, x, y, w, L"Notas de tus últimos tests");
    int gh = E(s, 190);
    RECT graf = rect(x, y, w, gh);
    cristal(dc, graf, E(s, 16));
    if (hist.tam() == 0) {
        texto(dc, s->f.texto, tm.textoSuave, L"Aún no has terminado ningún test. ¡Haz el primero!", graf,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        int izq = x + E(s, 46), der = x + w - E(s, 20), arriba = y + E(s, 20), abajo = y + gh - E(s, 34);
        for (int v = 0; v <= 10; v += 5) {
            int ly = abajo - (abajo - arriba) * v / 10;
            linea(dc, (float)izq, (float)ly, (float)der, (float)ly, v == 5 ? tm.medio : tm.borde, 1);
            texto(dc, s->f.mini, tm.textoSuave, std::to_wstring(v), rect(x + E(s, 8), ly - E(s, 9), E(s, 30), E(s, 18)),
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        int cuantas = (int)hist.tam();
        int paso = (der - izq) / std::max(1, cuantas);
        int bw = std::min(E(s, 34), paso * 6 / 10);
        for (int i = 0; i < cuantas; ++i) {
            double nota = hist[(std::size_t)i]["nota"].comoNumero();
            int bx = izq + i * paso + (paso - bw) / 2;
            int top = abajo - (int)((abajo - arriba) * nota / 10.0);
            RECT b = {bx, std::min(top, abajo - E(s, 4)), bx + bw, abajo};
            redondeado(dc, b, E(s, 5), gui::colorDeNota(nota));
            texto(dc, s->f.mini, tm.textoSuave, numero(nota), rect(bx - E(s, 10), abajo + E(s, 6), bw + E(s, 20), E(s, 18)),
                  DT_CENTER | DT_SINGLELINE);
        }
    }
    y += gh + E(s, 28);

    // Mapa de actividad: 8 semanas (columnas) x 7 dias (filas)
    {
        const js::Valor& dias = s->progreso["actividad"];
        int hoyN = res["hoy"].comoEntero();
        std::wstring sub = L"Cada cuadro es un día; más intenso cuantas más preguntas respondiste. Hoy: " +
                           std::to_wstring(hoyN) + (hoyN == 1 ? L" respuesta." : L" respuestas.");
        tituloSeccion(s, dc, x, y, w, L"Tu actividad", sub);
        int celda = E(s, 22), hueco2 = E(s, 5), izquierda = E(s, 34);
        int altoMapa = 7 * (celda + hueco2) + E(s, 30);
        RECT caja = rect(x, y, w, altoMapa + E(s, 20));
        cristal(dc, caja, E(s, 16));
        const wchar_t* letras = L"LMXJVSD";
        int oy = y + E(s, 16), ox = x + E(s, 18) + izquierda;
        for (int f = 0; f < 7; ++f)
            texto(dc, s->f.mini, tm.textoSuave, std::wstring(1, letras[f]),
                  rect(x + E(s, 18), oy + f * (celda + hueco2), izquierda - E(s, 8), celda), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        for (std::size_t i = 0; i < dias.tam(); ++i) {
            int col = (int)i / 7, fila = (int)i % 7;
            int n = dias[i]["n"].comoEntero();
            bool futuro = dias[i]["futuro"].comoBool();
            COLORREF c = futuro ? mezcla(tm.panel, tm.fondo, 0.5)
                       : n == 0 ? tm.panelAlt
                       : mezcla(tm.panelAlt, tm.acento, n < 5 ? 0.35 : n < 15 ? 0.65 : 1.0);
            RECT r = rect(ox + col * (celda + hueco2), oy + fila * (celda + hueco2), celda, celda);
            bool hoy = !futuro && (i + 1 == dias.tam() || dias[i + 1]["futuro"].comoBool());
            redondeado(dc, r, E(s, 5), c, hoy ? tm.texto : CLR_INVALID, hoy ? 2 : 1);
        }
        // Leyenda
        int lx = ox + 8 * (celda + hueco2) + E(s, 30);
        if (lx + E(s, 220) < x + w) {
            texto(dc, s->f.subtitulo, racha ? tm.ok : tm.textoSuave,
                  racha ? std::to_wstring(racha) + (racha == 1 ? L" día seguido" : L" días seguidos") : L"Sin racha todavía",
                  rect(lx, oy + E(s, 10), x + w - lx - E(s, 18), E(s, 26)), DT_LEFT | DT_SINGLELINE);
            texto(dc, s->f.peque, tm.textoSuave,
                  racha ? L"Responde al menos una pregunta cada día para mantenerla." : L"Haz un test hoy para empezar una racha.",
                  rect(lx, oy + E(s, 42), x + w - lx - E(s, 18), E(s, 40)), DT_LEFT | DT_WORDBREAK);
            int ly = oy + E(s, 100);
            texto(dc, s->f.mini, tm.textoSuave, L"Menos", rect(lx, ly, E(s, 50), celda), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            for (int k = 0; k < 4; ++k) {
                COLORREF c = k == 0 ? tm.panelAlt : mezcla(tm.panelAlt, tm.acento, k == 1 ? 0.35 : k == 2 ? 0.65 : 1.0);
                redondeado(dc, rect(lx + E(s, 50) + k * (celda + hueco2), ly, celda, celda), E(s, 5), c);
            }
            texto(dc, s->f.mini, tm.textoSuave, L"Más", rect(lx + E(s, 56) + 4 * (celda + hueco2), ly, E(s, 50), celda),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        y = caja.bottom + E(s, 28);
    }

    const js::Valor& temas = s->progreso["temas"];
    y += pintarLogros(s, dc, x, y, w);
    std::vector<FilaTema> filasT = filasPorTema(temas);
    tituloSeccion(s, dc, x, y, w, L"Dominio por tema",
                  hayTemas(filasT) ? L"Agrupados por tema y, dentro de cada uno, de más flojo a más fuerte. Practica uno con su botón."
                                   : L"Ordenados de más flojo a más fuerte. Practica uno con su botón.");
    if (temas.tam() == 0) {
        texto(dc, s->f.texto, tm.textoSuave, L"Responde algunas preguntas para ver aquí tus puntos fuertes y débiles.",
              rect(x, y, w, E(s, 24)), DT_LEFT | DT_SINGLELINE);
        y += E(s, 30);
    }
    for (const FilaTema& f : filasT) {
        if (f.indice < 0) {
            int ac = 0, t = 0;
            for (std::size_t j = 0; j < temas.tam(); ++j)
                if (W(temas[j]["grupo"]) == f.grupo) ac += temas[j]["aciertos"].comoEntero(), t += temas[j]["total"].comoEntero();
            std::wstring info = t ? std::to_wstring((int)std::lround(100.0 * ac / t)) + L"\u00A0% de aciertos en el tema" : L"";
            subtituloTema(s, dc, rect(x + E(s, 4), y + (&f == &filasT.front() ? 0 : E(s, 6)), w - E(s, 8), E(s, 34)), f.grupo, info,
                          tm.acento);
            y += E(s, &f == &filasT.front() ? 42 : 48);
            continue;
        }
        std::size_t i = (std::size_t)f.indice;
        int t = temas[i]["total"].comoEntero(1), ac = temas[i]["aciertos"].comoEntero();
        double fr = (double)ac / std::max(1, t);
        RECT fila = rect(x, y, w, E(s, 56));
        cristal(dc, fila, E(s, 12));
        texto(dc, s->f.texto, tm.texto, W(temas[i]["tema"]), rect(x + E(s, 16), y, w * 40 / 100, E(s, 56)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        barra(dc, rect(x + w * 42 / 100, y + E(s, 24), w * 24 / 100, E(s, 9)), fr, fr >= 0.7 ? tm.ok : fr >= 0.5 ? tm.medio : tm.mal);
        wchar_t b[48];
        swprintf(b, 48, L"%d\u00A0%%  (%d/%d)", (int)std::lround(fr * 100), ac, t);
        texto(dc, s->f.peque, tm.textoSuave, b, rect(x + w * 67 / 100, y, w - w * 67 / 100 - E(s, 132), E(s, 56)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        boton(s, dc, rect(x + w - E(s, 120), y + E(s, 10), E(s, 106), E(s, 36)), L"Practicar", A_PRACTICAR_TEMA, (int)i);
        y += E(s, 64);
    }
    return y - a.y;
}

// --------------------------------------------------------------- dudas

const wchar_t* kSugerencias[] = {L"Explícame la pregunta del test paso a paso", L"Resúmeme las ideas clave del tema",
                                 L"Ponme un ejemplo práctico", L"¿Qué es lo más importante para el examen?"};

// Repreguntas rapidas bajo la ultima respuesta.
const wchar_t* kSeguimiento[] = {L"Más sencillo", L"Ponme un ejemplo", L"Ponme a prueba", L"Resúmelo en 3 puntos"};
const wchar_t* kSeguimientoTexto[] = {
    L"No lo he entendido del todo. ¿Me lo explicas más sencillo, como si empezara de cero?",
    L"¿Me pones un ejemplo concreto y cotidiano de esto?",
    L"Hazme una pregunta corta para comprobar si lo he entendido, sin darme la respuesta todavía.",
    L"Resúmemelo en 3 puntos clave para el examen."};

// Retrato del profesor en pequenito, recortado en circulo. Usa una copia reducida del
// retrato (el original es enorme y escalarlo en cada pintado seria lento).
void avatar(Estado* s, HDC dc, int cx, int cy, int radio) {
    const gui::Tema& tm = gui::tema();
    circulo(dc, cx, cy, radio, mezcla(tm.panel, tm.acento, 0.45));
    if (!s->retrato || s->retrato->GetLastStatus() != Gdiplus::Ok || s->retrato->GetWidth() == 0) return;
    if (!s->retratoPeque) {
        int ancho = 320, alto = (int)(320.0 * s->retrato->GetHeight() / s->retrato->GetWidth());
        s->retratoPeque = new Gdiplus::Bitmap(ancho, alto, PixelFormat32bppPARGB);
        Gdiplus::Graphics gb(s->retratoPeque);
        gb.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        gb.DrawImage(s->retrato, 0, 0, ancho, alto);
    }
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    Gdiplus::GraphicsPath p;
    p.AddEllipse(cx - radio, cy - radio, radio * 2, radio * 2);
    g.SetClip(&p);
    // Encuadre de la cara: la mitad superior del retrato
    int ancho = radio * 4, alto = ancho * (int)s->retratoPeque->GetHeight() / std::max(1, (int)s->retratoPeque->GetWidth());
    g.DrawImage(s->retratoPeque, cx - ancho / 2, cy - radio - alto / 12, ancho, alto);
}

int pintarChat(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    if (s->chat.empty()) {
        avatar(s, dc, x + w / 2, y + E(s, 60), E(s, 52));
        y += E(s, 130);
        texto(dc, s->f.titulo, tm.texto, L"Pregúntame lo que no entiendas", rect(x, y, w, E(s, 36)),
              DT_CENTER | DT_SINGLELINE);
        y += E(s, 42);
        std::wstring sub = L"Busco en tu Temario y te lo explico con mis palabras, citando de dónde sale. "
                           L"Puedes repreguntar tantas veces como quieras.";
        int h = altoTexto(dc, s->f.texto, sub, w * 8 / 10);
        texto(dc, s->f.texto, tm.textoSuave, sub, rect(x + w / 10, y, w * 8 / 10, h), DT_CENTER | DT_WORDBREAK);
        y += h + E(s, 28);
        // Sugerencias: la pregunta del test (si hay uno) y preguntas sacadas de tu temario.
        std::vector<std::pair<int, std::wstring>> sugerencias;
        if (hayTest(s)) sugerencias.push_back({0, kSugerencias[0]});
        const js::Valor& delTemario = s->inicio["sugerencias"];
        for (std::size_t i = 0; i < delTemario.tam() && sugerencias.size() < 4; ++i)
            sugerencias.push_back({10 + (int)i, W(delTemario[i])});
        for (int i = 1; i < 4 && sugerencias.size() < 2; ++i) sugerencias.push_back({i, kSugerencias[i]});
        int cw = (w - E(s, 12)) / 2;
        for (std::size_t k = 0; k < sugerencias.size(); ++k) {
            RECT r = rect(x + (int)(k % 2) * (cw + E(s, 12)), y + (int)(k / 2) * E(s, 68), cw, E(s, 58));
            int dato = sugerencias[k].first;
            bool h2 = encima(s, A_SUGERENCIA, dato);
            redondeado(dc, r, E(s, 14), h2 ? tm.panelAlt : tm.panel, h2 ? tm.acento : tm.borde);
            icono(s, dc, dato == 0 ? I_TEST : I_CHISPA, r.left + E(s, 26), r.top + E(s, 29), E(s, 16), tm.acento);
            texto(dc, s->f.texto, tm.texto, sugerencias[k].second, rect(r.left + E(s, 46), r.top, cw - E(s, 62), E(s, 58)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            zona(s, r, A_SUGERENCIA, dato);
        }
        return y + (int)((sugerencias.size() + 1) / 2) * E(s, 68) - a.y;
    }
    // Cabecera: empezar de cero (la conversacion se guarda entre sesiones)
    {
        int n = (int)(s->chat.size() + 1) / 2;
        texto(dc, s->f.peque, tm.textoSuave,
              std::to_wstring(n) + (n == 1 ? L" pregunta en esta conversación" : L" preguntas en esta conversación") +
                  L"  ·  se guarda aunque cierres el profesor",
              rect(x, y, w - E(s, 220), E(s, 32)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        botonIcono(s, dc, rect(x + w - E(s, 210), y, E(s, 210), E(s, 32)), I_REPETIR, L"Nueva conversación",
                   A_NUEVA_CONVERSACION, 0, FANTASMA, !s->chatEnCurso);
        y += E(s, 48);
    }
    int maxAncho = w * 82 / 100;
    for (std::size_t i = 0; i < s->chat.size(); ++i) {
        const Mensaje& m = s->chat[i];
        if (m.alumno) {
            int tw = std::min(maxAncho - E(s, 32), anchoTexto(dc, s->f.texto, m.texto) + E(s, 2));
            int th = altoTexto(dc, s->f.texto, m.texto, tw);
            RECT b = rect(x + w - tw - E(s, 32), y, tw + E(s, 32), th + E(s, 24));
            redondeado(dc, b, E(s, 18), tm.acento);
            texto(dc, s->f.texto, RGB(255, 255, 255), m.texto, rect(b.left + E(s, 16), y + E(s, 12), tw, th));
            y = b.bottom + E(s, 4);
            if (!m.contexto.empty()) {
                texto(dc, s->f.mini, tm.textoSuave, m.contexto, rect(x, y, w, E(s, 16)), DT_RIGHT | DT_SINGLELINE);
                y += E(s, 18);
            }
            y += E(s, 14);
        } else {
            int ax = x + E(s, 20);
            avatar(s, dc, ax, y + E(s, 20), E(s, 20));
            int bx = x + E(s, 52), bw = maxAncho;
            std::wstring t = m.texto;
            bool pensando = m.enCurso && t.empty();
            if (m.enCurso && !t.empty()) t += (s->animacion / 8) % 2 ? L" ▍" : L"  ";
            int th = pensando ? E(s, 24) : textoRico(s, dc, t, 0, 0, bw - E(s, 36), false);
            RECT b = rect(bx, y, bw, th + E(s, 28));
            cristal(dc, b, E(s, 18));
            if (pensando) {
                for (int k = 0; k < 3; ++k) {
                    int fase = (s->animacion / 4 + 3 - k) % 3;
                    circulo(dc, bx + E(s, 26) + k * E(s, 16), y + E(s, 26), E(s, 4),
                            fase == 0 ? tm.acento : mezcla(tm.panel, tm.textoSuave, 0.5));
                }
                // En CPU la IA tarda en leer los apuntes antes de escribir la primera palabra: se cuenta
                // el tiempo y se dice en que esta, para que no parezca colgado
                int seg = s->chatDesde ? (int)((GetTickCount64() - s->chatDesde) / 1000) : 0;
                std::wstring fase = seg < 4    ? L"Buscando en tus apuntes…"
                                    : seg < 45 ? L"Leyendo lo que he encontrado en tus apuntes…"
                                               : L"Preparando la explicación (en este ordenador el primer paso es el más lento)…";
                if (seg >= 4) fase += L"   " + tiempo(seg);
                texto(dc, s->f.peque, tm.textoSuave, fase,
                      rect(bx + E(s, 80), y, bw - E(s, 90), th + E(s, 28)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else {
                textoRico(s, dc, t, bx + E(s, 18), y + E(s, 14), bw - E(s, 36));
            }
            y = b.bottom + E(s, 8);
            const js::Valor& fuentes = m.fuentes;
            int fx = bx;
            if (!m.enCurso && !m.texto.empty()) {
                // Copiar la respuesta (sin el formato) al portapapeles
                RECT r = rect(fx, y, E(s, 92), E(s, 26));
                bool h2 = encima(s, A_COPIAR, (int)i);
                redondeado(dc, r, E(s, 13), h2 ? tm.panelAlt : tm.fondo, h2 ? tm.acento : tm.borde);
                icono(s, dc, I_COPIAR, r.left + E(s, 18), r.top + E(s, 13), E(s, 13), h2 ? tm.texto : tm.textoSuave);
                texto(dc, s->f.peque, h2 ? tm.texto : tm.textoSuave, L"Copiar", rect(r.left + E(s, 32), r.top, E(s, 56), E(s, 26)),
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                zona(s, r, A_COPIAR, (int)i);
                fx += E(s, 100);
            }
            if (fuentes.tam() > 0 || fx > bx) {
                for (std::size_t k = 0; k < fuentes.tam(); ++k) {
                    std::wstring nombre = L"[" + std::to_wstring(k + 1) + L"] " + W(fuentes[k]["titulo"]);
                    int fw = std::min(bw, anchoTexto(dc, s->f.peque, nombre) + E(s, 24));
                    if (fx + fw > bx + bw) {
                        fx = bx;
                        y += E(s, 32);
                    }
                    RECT r = rect(fx, y, fw, E(s, 26));
                    int dato = (int)(i * 16 + k);
                    bool h2 = encima(s, A_FUENTE, dato);
                    redondeado(dc, r, E(s, 13), h2 ? tm.panelAlt : tm.fondo, h2 ? tm.acento : tm.borde);
                    texto(dc, s->f.peque, tm.acento, nombre, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    zona(s, r, A_FUENTE, dato);
                    fx += fw + E(s, 8);
                }
                y += E(s, 34);
            }
            // Repreguntas rapidas tras la ultima respuesta
            if (i + 1 == s->chat.size() && !m.enCurso && !s->chatEnCurso && !m.texto.empty()) {
                int px = bx;
                for (int k = 0; k < 4; ++k) {
                    int cw = anchoTexto(dc, s->f.texto, kSeguimiento[k]) + E(s, 28);
                    if (px + cw > x + w) break;
                    RECT r = rect(px, y + E(s, 4), cw, E(s, 34));
                    bool h2 = encima(s, A_SEGUIMIENTO, k);
                    redondeado(dc, r, E(s, 17), h2 ? mezcla(tm.panel, tm.acento, 0.2) : tm.panel, h2 ? tm.acento : mezcla(tm.borde, tm.acento, 0.4));
                    texto(dc, s->f.texto, h2 ? tm.texto : tm.acento, kSeguimiento[k], r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    zona(s, r, A_SEGUIMIENTO, k);
                    px += cw + E(s, 8);
                }
                y += E(s, 46);
            }
            y += E(s, 14);
        }
    }
    return y - a.y;
}

// Barra de escritura del chat: el EDIT nativo va dentro del campo redondeado.
RECT campoChat(Estado* s, RECT pie) {
    return {pie.left, pie.top + E(s, 14), pie.right - E(s, 64), pie.bottom - E(s, 14)};
}

void pieChat(Estado* s, HDC dc, RECT pie) {
    const gui::Tema& tm = gui::tema();
    RECT campo = campoChat(s, pie);
    bool foco = GetFocus() == s->edit;
    redondeado(dc, campo, E(s, 22), tm.panel, foco ? tm.acento : tm.borde, foco ? 2 : 1);
    // Contexto (pregunta del test) encima del campo
    std::wstring ctx = contextoDePregunta(s);
    if (!ctx.empty()) {
        int cw = anchoTexto(dc, s->f.peque, ctx) + E(s, 46);
        RECT c = rect(pie.left, pie.top - E(s, 32), cw, E(s, 26));
        redondeado(dc, c, E(s, 13), mezcla(tm.fondo, tm.acento, 0.18), mezcla(tm.fondo, tm.acento, 0.5));
        texto(dc, s->f.peque, tm.texto, ctx, rect(c.left + E(s, 12), c.top, cw - E(s, 40), E(s, 26)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT x = rect(c.right - E(s, 26), c.top + E(s, 3), E(s, 20), E(s, 20));
        COLORREF cx = encima(s, A_QUITAR_CONTEXTO) ? tm.texto : tm.textoSuave;
        linea(dc, (float)x.left + E(s, 6), (float)x.top + E(s, 6), (float)x.right - E(s, 6), (float)x.bottom - E(s, 6), cx, 1.6f);
        linea(dc, (float)x.right - E(s, 6), (float)x.top + E(s, 6), (float)x.left + E(s, 6), (float)x.bottom - E(s, 6), cx, 1.6f);
        zona(s, c, A_QUITAR_CONTEXTO);
    }
    // Enviar (o detener mientras responde)
    int r = E(s, 24);
    int cx = pie.right - r, cy = (campo.top + campo.bottom) / 2;
    RECT b = rect(cx - r, cy - r, 2 * r, 2 * r);
    if (s->chatEnCurso) {
        circulo(dc, cx, cy, r, encima(s, A_CANCELAR_DUDA) ? tm.mal : mezcla(tm.panel, tm.mal, 0.6));
        redondeado(dc, rect(cx - E(s, 7), cy - E(s, 7), E(s, 14), E(s, 14)), E(s, 3), RGB(255, 255, 255));
        zona(s, b, A_CANCELAR_DUDA);
    } else {
        circulo(dc, cx, cy, r, encima(s, A_ENVIAR) ? mezcla(tm.acento, RGB(255, 255, 255), 0.15) : tm.acento);
        float k = (float)E(s, 8);
        linea(dc, (float)cx, cy + k, (float)cx, cy - k, RGB(255, 255, 255), (float)E(s, 2) + 0.5f);
        linea(dc, cx - k * 0.75f, cy - k * 0.2f, (float)cx, cy - k, RGB(255, 255, 255), (float)E(s, 2) + 0.5f);
        linea(dc, cx + k * 0.75f, cy - k * 0.2f, (float)cx, cy - k, RGB(255, 255, 255), (float)E(s, 2) + 0.5f);
        zona(s, b, A_ENVIAR);
    }
}

// ----------------------------------------------------------------- lateral

// Proxima prueba evaluable de la asignatura (sin nota y aun por llegar), o NULL.
const Examen* proximaPrueba(Estado* s, long& dias) {
    const Examen* mejor = NULL;
    Fecha hoy = Fecha::hoy();
    for (const Examen& e : s->asignatura.examenes) {
        if (e.calificado || !e.fecha.valida()) continue;
        long d = hoy.diasHasta(e.fecha);
        if (d < 0) continue;
        if (!mejor || d < dias) {
            mejor = &e;
            dias = d;
        }
    }
    return mejor;
}

// Panel "Tu situacion" al pie del lateral: la proxima prueba y el test en curso o tu progreso.
// No se dibuja si no cabe por debajo de `arriba`.
void panelSituacion(Estado* s, HDC dc, RECT lado, int arriba) {
    const gui::Tema& tm = gui::tema();
    int x = lado.left + E(s, 16), w = lado.right - lado.left - E(s, 32);
    int alto = E(s, 160);
    RECT p = rect(x, lado.bottom - E(s, 16) - alto, w, alto);
    if (s->vista == V_ARRANCANDO || s->vista == V_ERROR || p.top < arriba) return;
    redondeado(dc, p, E(s, 14), tm.fondo, tm.borde);
    int ix = x + E(s, 16), iw = w - E(s, 32), y = p.top + E(s, 14);

    // 1) Proxima prueba (del calendario del gestor)
    texto(dc, s->f.mini, tm.textoSuave, L"PRÓXIMA PRUEBA", rect(ix, y, iw, E(s, 16)), DT_LEFT | DT_SINGLELINE);
    y += E(s, 20);
    long dias = 0;
    const Examen* e = proximaPrueba(s, dias);
    if (e) {
        std::wstring cuando = dias == 0 ? L"hoy" : dias == 1 ? L"mañana" : L"en " + std::to_wstring(dias) + L" días";
        COLORREF c = dias <= 3 ? tm.mal : dias <= 10 ? tm.medio : tm.ok;
        int cw = anchoTexto(dc, s->f.peque, cuando) + E(s, 18);
        RECT pill = rect(ix + iw - cw, y + E(s, 1), cw, E(s, 22));
        redondeado(dc, pill, E(s, 11), mezcla(tm.fondo, c, 0.2), mezcla(tm.fondo, c, 0.55));
        texto(dc, s->f.peque, mezcla(c, tm.texto, 0.2), cuando, pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.negrita, tm.texto, W(e->nombre), rect(ix, y, iw - cw - E(s, 8), E(s, 24)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        wchar_t det[96];
        swprintf(det, 96, L"%ls  ·  %d\u00A0%% de la nota", W(e->fecha.aTextoCorto()).c_str(), (int)std::lround(e->ponderacion));
        texto(dc, s->f.peque, tm.textoSuave, det, rect(ix, y + E(s, 24), iw, E(s, 18)), DT_LEFT | DT_SINGLELINE);
    } else {
        texto(dc, s->f.texto, tm.textoSuave, L"No hay pruebas pendientes", rect(ix, y, iw, E(s, 24)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    y += E(s, 50);
    redondeado(dc, rect(ix, y, iw, 1), 0, tm.borde);
    y += E(s, 12);

    // 2) Test en curso, o tu progreso en la asignatura
    if (hayTest(s) && !testTerminado(s)) {
        const js::Valor& estados = s->sesion["estados"];
        int n = (int)estados.tam(), bien = 0, mal = 0, hechas = 0;
        for (std::size_t i = 0; i < estados.tam(); ++i) {
            std::string est = estados[i].comoCadena();
            if (est == "bien") ++bien;
            if (est == "mal") ++mal;
            if (est == "bien" || est == "mal" || est == "respondida") ++hechas;
        }
        RECT zonaTest = rect(x + E(s, 4), y - E(s, 6), w - E(s, 8), p.bottom - y);
        if (encima(s, A_PESTANA, V_PREGUNTA + 100)) redondeado(dc, zonaTest, E(s, 10), tm.panelAlt);
        texto(dc, s->f.mini, tm.textoSuave, L"TEST EN CURSO", rect(ix, y, iw, E(s, 16)), DT_LEFT | DT_SINGLELINE);
        texto(dc, s->f.peque, tm.texto, std::to_wstring(hechas) + L" / " + std::to_wstring(n), rect(ix, y, iw, E(s, 16)),
              DT_RIGHT | DT_SINGLELINE);
        y += E(s, 24);
        barra(dc, rect(ix, y, iw, E(s, 7)), (double)hechas / std::max(1, n), tm.acento);
        y += E(s, 16);
        if (s->sesion["config"]["correccion"].comoCadena() == "final") {
            texto(dc, s->f.peque, tm.textoSuave, L"Corrección al entregar", rect(ix, y, iw, E(s, 20)),
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            marca(dc, ix + E(s, 8), y + E(s, 10), E(s, 8), true);
            texto(dc, s->f.negrita, tm.texto, std::to_wstring(bien), rect(ix + E(s, 22), y, E(s, 40), E(s, 20)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            marca(dc, ix + E(s, 72), y + E(s, 10), E(s, 8), false);
            texto(dc, s->f.negrita, tm.texto, std::to_wstring(mal), rect(ix + E(s, 86), y, E(s, 40), E(s, 20)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        if (s->vista != V_PREGUNTA) zona(s, zonaTest, A_PESTANA, V_PREGUNTA + 100);
    } else {
        const js::Valor& r = s->hola["resumen"];
        int prec = r["precision"].comoEntero(-1), fallos = r["mis_fallos"].comoEntero(), racha = r["racha"].comoEntero();
        int tercio = iw / 3;
        struct Dato {
            std::wstring valor, nombre;
            COLORREF color;
            int vista;
        } datos[3] = {
            {prec < 0 ? L"—" : std::to_wstring(prec) + L"\u00A0%", L"Precisión",
             prec < 0 ? tm.textoSuave : prec >= 70 ? tm.ok : prec >= 50 ? tm.medio : tm.mal, V_PROGRESO + 100},
            {std::to_wstring(racha), L"Racha", racha ? tm.ok : tm.textoSuave, V_PROGRESO + 200},
            {std::to_wstring(fallos), L"Mis fallos", fallos ? tm.medio : tm.texto, V_FALLOS + 100}};
        for (int k = 0; k < 3; ++k) {
            RECT z = rect(ix + k * tercio - E(s, 6), y - E(s, 4), tercio, E(s, 58));
            if (encima(s, A_PESTANA, datos[k].vista)) redondeado(dc, z, E(s, 10), tm.panelAlt);
            texto(dc, s->f.subtitulo, datos[k].color, datos[k].valor, rect(ix + k * tercio, y, tercio, E(s, 26)),
                  DT_LEFT | DT_SINGLELINE);
            texto(dc, s->f.mini, tm.textoSuave, datos[k].nombre, rect(ix + k * tercio, y + E(s, 28), tercio, E(s, 18)),
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            zona(s, z, A_PESTANA, datos[k].vista);
        }
    }
}

void trozoDeFondo(Estado* s, HDC dc, const RECT& r);

struct ItemNav {
    Vista v;
    Icono i;
    std::wstring nombre;
    std::wstring insignia;  // numero a la derecha (vacio = ninguno)
    COLORREF color;         // color de la insignia
    bool trabajando;        // la IA esta haciendo algo en esa pantalla
};

std::vector<ItemNav> itemsNav(Estado* s) {
    const gui::Tema& tm = gui::tema();
    const js::Valor& res = s->hola["resumen"];
    std::vector<ItemNav> items;
    int pendientesPlan = 0;
    const js::Valor& plan = s->inicio["plan"];
    for (std::size_t i = 0; i < plan.tam(); ++i) pendientesPlan += plan[i]["hecha"].comoBool() ? 0 : 1;
    items.push_back({V_HUB, I_INICIO, L"Inicio", pendientesPlan ? std::to_wstring(pendientesPlan) : L"", tm.acento, false});
    items.push_back({V_INICIO, I_TEST, L"Nuevo test", L"", tm.acento, false});
    if (hayTest(s)) {
        if (testTerminado(s)) {
            items.push_back({V_PREGUNTA, I_JUGAR, L"Revisión del test", L"", tm.acento, false});
        } else {
            std::wstring cuenta = std::to_wstring(s->sesion["respondidas"].comoEntero()) + L"/" +
                                  std::to_wstring(s->sesion["n"].comoEntero());
            items.push_back({V_PREGUNTA, I_JUGAR, s->sesion["config"]["simulacro"].comoBool() ? L"Simulacro" : L"Test en curso",
                             cuenta, tm.acento, !s->generacion.esNulo()});
        }
    }
    if (!s->resultado.esNulo())
        items.push_back({V_RESULTADO, I_RESULTADOS, L"Resultados", numero(s->resultado["nota"].comoNumero()),
                         gui::colorDeNota(s->resultado["nota"].comoNumero()), false});
    int pendientes = res["tarjetas_pendientes"].comoEntero();
    items.push_back({V_TARJETAS, I_TARJETAS, L"Tarjetas", pendientes ? std::to_wstring(pendientes) : L"", RGB(251, 146, 60),
                     s->truco.activa()});
    items.push_back({V_RESUMENES, I_RESUMEN, L"Resúmenes", L"", RGB(45, 212, 191),
                     s->tareaResumen.activa() || !s->resumenesFondo.esNulo()});
    {
        int sinLeer = s->temario.esObjeto() ? s->temario["total"].comoEntero() - s->temario["leidos"].comoEntero() : 0;
        items.push_back({V_TEMARIO, I_LIBRO, L"Temario", sinLeer > 0 ? std::to_wstring(sinLeer) : L"", RGB(96, 165, 250), false});
    }
    items.push_back({V_DESARROLLO, I_DESARROLLO, L"Desarrollo", L"", RGB(167, 139, 250),
                     s->tareaAbierta.activa() || s->tareaCorreccion.activa()});
    items.push_back({V_BUSCAR, I_BUSCAR, L"Buscar y glosario", L"", tm.acento, false});
    if (iaDisponible(s) || s->chatEnCurso) items.push_back({V_CHAT, I_DUDAS, L"Dudas", L"", tm.acento, s->chatEnCurso});
    items.push_back({V_PROGRESO, I_PROGRESO, L"Progreso", L"", tm.ok, s->tareaAnalisis.activa()});
    int fallos = res["mis_fallos"].comoEntero();
    items.push_back({V_FALLOS, I_FALLOS, L"Mis fallos", fallos ? std::to_wstring(fallos) : L"", tm.medio, false});
    return items;
}

void pintarLateral(Estado* s, HDC dc, RECT lado) {
    const gui::Tema& tm = gui::tema();
    trozoDeFondo(s, dc, lado);
    velo(dc, lado, tm.panel, tm.oscuro ? 175 : 205);
    redondeado(dc, rect(lado.right - 1, lado.top, 1, lado.bottom - lado.top), 0, tm.borde);
    int x = lado.left + E(s, 16), w = lado.right - lado.left - E(s, 32), y = lado.top + E(s, 18);
    bool listo = s->vista != V_ARRANCANDO && s->vista != V_ERROR;

    // --- El profesor
    avatar(s, dc, x + E(s, 26), y + E(s, 26), E(s, 26));
    circulo(dc, x + E(s, 46), y + E(s, 46), E(s, 6), listo && iaDisponible(s) ? tm.ok : tm.textoSuave, tm.panel, 2.0f);
    texto(dc, s->f.negrita, tm.texto, L"Profesor Universalis", rect(x + E(s, 64), y + E(s, 4), w - E(s, 64), E(s, 24)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    bool cambiar = listo && s->todas.size() > 1;
    texto(dc, s->f.peque, encima(s, A_SELECTOR) ? tm.texto : tm.textoSuave, W(s->asignatura.nombre),
          rect(x + E(s, 64), y + E(s, 28), w - E(s, 64) - (cambiar ? E(s, 20) : 0), E(s, 20)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (cambiar) {
        // Flecha: pulsando el perfil se cambia de asignatura
        int fx = x + w - E(s, 8), fy = y + E(s, 38), k = E(s, 4);
        COLORREF cf = encima(s, A_SELECTOR) ? tm.texto : tm.textoSuave;
        linea(dc, (float)(fx - k), (float)(fy - k / 2), (float)fx, (float)(fy + k / 2), cf, 1.6f);
        linea(dc, (float)fx, (float)(fy + k / 2), (float)(fx + k), (float)(fy - k / 2), cf, 1.6f);
        if (!s->ocupado) zona(s, rect(x, y, w, E(s, 56)), A_SELECTOR);
    }
    y += E(s, 62);

    // --- Bocadillo con lo que dice el profesor
    // Alto fijo (4 lineas): asi el menu de debajo no se mueve cuando cambia el mensaje.
    std::wstring m = s->mensaje.empty() ? L"…" : s->mensaje;
    SelectObject(dc, s->f.texto);
    TEXTMETRICW met;
    GetTextMetricsW(dc, &met);
    int mh = 4 * met.tmHeight;
    RECT bocadillo = rect(x, y + E(s, 8), w, mh + E(s, 24));
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush pb(gc(tm.panelAlt));
        Gdiplus::PointF pico[3] = {{(float)(x + E(s, 18)), (float)bocadillo.top + 1},
                                   {(float)(x + E(s, 26)), (float)(bocadillo.top - E(s, 9))},
                                   {(float)(x + E(s, 36)), (float)bocadillo.top + 1}};
        g.FillPolygon(&pb, pico, 3);
    }
    redondeado(dc, bocadillo, E(s, 14), tm.panelAlt);
    texto(dc, s->f.texto, tm.texto, m, rect(x + E(s, 14), bocadillo.top + E(s, 12), w - E(s, 28), mh),
          DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_EDITCONTROL);
    y = bocadillo.bottom + E(s, 18);
    if (!listo) return;

    // --- Navegacion
    texto(dc, s->f.mini, tm.textoSuave, L"ESTUDIAR", rect(x + E(s, 12), y, w, E(s, 16)), DT_LEFT | DT_SINGLELINE);
    y += E(s, 22);
    int filaAlto = E(s, 38);
    for (const ItemNav& it : itemsNav(s)) {
        RECT r = rect(x, y, w, filaAlto - E(s, 3));
        bool activa = s->vista == it.v;
        bool h = !s->ocupado && encima(s, A_PESTANA, it.v);
        if (activa) {
            redondeado(dc, r, E(s, 10), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.22 : 0.14));
            redondeado(dc, rect(r.left, r.top + E(s, 8), E(s, 3), r.bottom - r.top - E(s, 16)), E(s, 1), tm.acento);
        } else if (h) {
            redondeado(dc, r, E(s, 10), tm.panelAlt);
        }
        int cy = (r.top + r.bottom) / 2;
        icono(s, dc, it.i, x + E(s, 22), cy, E(s, 17), activa ? tm.acento : h ? tm.texto : tm.textoSuave);
        int derecha = r.right - E(s, 10);
        if (it.trabajando) {
            spinner(s, dc, derecha - E(s, 8), cy, E(s, 6));
            derecha -= E(s, 22);
        } else if (!it.insignia.empty()) {
            int iw = std::max(E(s, 22), anchoTexto(dc, s->f.mini, it.insignia) + E(s, 14));
            RECT pill = rect(derecha - iw, cy - E(s, 10), iw, E(s, 20));
            redondeado(dc, pill, E(s, 10), mezcla(tm.panel, it.color, 0.22), mezcla(tm.panel, it.color, 0.55));
            texto(dc, s->f.mini, mezcla(it.color, tm.texto, 0.25), it.insignia, pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            derecha -= iw + E(s, 8);
        }
        texto(dc, activa ? s->f.negrita : s->f.texto, activa || h ? tm.texto : mezcla(tm.texto, tm.textoSuave, 0.35), it.nombre,
              rect(x + E(s, 44), r.top, derecha - x - E(s, 44), r.bottom - r.top), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (!s->ocupado) zona(s, rect(x, y - E(s, 1), w, filaAlto), A_PESTANA, it.v);  // sin huecos entre opciones
        y += filaAlto;
    }

    // --- Situacion (si cabe)
    panelSituacion(s, dc, lado, y + E(s, 12));
}

// ------------------------------------------------------- barra de titulo

void iconoVentana(Estado* s, HDC dc, RECT r, int accion, COLORREF c) {
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, k = E(s, 5);
    float g = std::max(1.0f, (float)s->factor);
    switch (accion) {
        case A_MINIMIZAR: linea(dc, (float)cx - k, (float)cy, (float)cx + k, (float)cy, c, g); break;
        case A_MAXIMIZAR:
            if (IsZoomed(s->ventana)) {
                contorno(dc, rect(cx - k, cy - k + E(s, 2), 2 * k - E(s, 2), 2 * k - E(s, 2)), E(s, 1), c, g);
                linea(dc, (float)cx - k + E(s, 2), (float)cy - k, (float)cx + k, (float)cy - k, c, g);
                linea(dc, (float)cx + k, (float)cy - k, (float)cx + k, (float)cy + k - E(s, 2), c, g);
            } else {
                Gdiplus::Graphics gr(dc);
                gr.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Pen lapiz(gc(c), g);
                gr.DrawRectangle(&lapiz, (float)(cx - k), (float)(cy - k), (float)(2 * k), (float)(2 * k));
            }
            break;
        case A_CERRAR:
            linea(dc, (float)cx - k, (float)cy - k, (float)cx + k, (float)cy + k, c, g);
            linea(dc, (float)cx + k, (float)cy - k, (float)cx - k, (float)cy + k, c, g);
            break;
        case A_PANTALLA_COMPLETA: {
            int q = E(s, 3);
            bool dentro = s->pantallaCompleta;
            // Cuatro esquinas: hacia fuera (entrar) o hacia dentro (salir)
            for (int sx : {-1, 1}) {
                for (int sy : {-1, 1}) {
                    float ex = (float)cx + sx * k, ey = (float)cy + sy * k;
                    float dx = dentro ? -sx * q : 0, dy = dentro ? -sy * q : 0;
                    linea(dc, ex + dx, ey + dy, ex + dx - sx * q * (dentro ? -1 : 1), ey + dy, c, g);
                    linea(dc, ex + dx, ey + dy, ex + dx, ey + dy - sy * q * (dentro ? -1 : 1), c, g);
                }
            }
            break;
        }
    }
}

const wchar_t* nombreVista(Estado* s) {
    switch (s->vista) {
        case V_HUB: return L"Inicio";
        case V_INICIO: return L"Nuevo test";
        case V_PREGUNTA: return testTerminado(s) ? L"Revisión del test" : L"Test";
        case V_RESULTADO: return L"Resultados";
        case V_FALLOS: return L"Mis fallos";
        case V_PROGRESO: return L"Progreso";
        case V_CHAT: return L"Dudas";
        case V_TARJETAS: return L"Tarjetas";
        case V_RESUMENES: return L"Resúmenes";
        case V_TEMARIO: return L"Temario";
        case V_DESARROLLO: return L"Desarrollo";
        case V_BUSCAR: return L"Buscar y glosario";
        default: return L"";
    }
}

// Lo que esta haciendo la IA ahora mismo, para la barra de titulo. Devuelve false si nada.
bool actividadIA(Estado* s, std::wstring& que, int& vista, const TareaIA*& tarea) {
    tarea = NULL;
    if (s->chatEnCurso) { que = L"Respondiendo tu duda"; vista = V_CHAT; return true; }
    if (s->tareaCorreccion.activa()) { que = L"Corrigiendo tu respuesta"; vista = V_DESARROLLO; tarea = &s->tareaCorreccion; return true; }
    if (s->tareaAbierta.activa()) { que = L"Redactando una pregunta"; vista = V_DESARROLLO; tarea = &s->tareaAbierta; return true; }
    if (s->tareaResumen.activa()) { que = L"Escribiendo un resumen"; vista = V_RESUMENES; tarea = &s->tareaResumen; return true; }
    if (s->truco.activa()) { que = L"Pensando un truco"; vista = V_TARJETAS; tarea = &s->truco; return true; }
    if (s->tareaAnalisis.activa()) { que = L"Analizando tu progreso"; vista = V_PROGRESO; tarea = &s->tareaAnalisis; return true; }
    if (!s->resumenesFondo.esNulo()) {
        wchar_t b[96];
        swprintf(b, 96, L"Resumiendo temas · %d de %d", s->resumenesFondo["hechos"].comoEntero() + 1,
                 s->resumenesFondo["total"].comoEntero());
        que = b;
        vista = V_RESUMENES;
        return true;
    }
    if (!s->generacion.esNulo()) {
        wchar_t b[96];
        swprintf(b, 96, L"Redactando preguntas · %d de %d · %d %%", s->generacion["listas"].comoEntero(),
                 s->generacion["total"].comoEntero(), s->generacion["progreso"].comoEntero());
        que = b;
        vista = V_PREGUNTA;
        return true;
    }
    if (s->reserva["estado"].comoCadena() == "trabajando") {
        que = L"Preparando preguntas · " + W(s->reserva["tema"]);
        vista = V_INICIO;
        return true;
    }
    return false;
}

void pintarTitulo(Estado* s, HDC dc, RECT barraTitulo) {
    const gui::Tema& tm = gui::tema();
    // Barra oscura en los dos temas (su color sale de los Ajustes), con textos claros.
    const COLORREF fondo = gui::colorBarraSuperior(), tinta = gui::textoBarraSuperior(), suave = RGB(150, 156, 168);
    const COLORREF encimaFondo = mezcla(fondo, RGB(255, 255, 255), 0.13);
    HBRUSH b = CreateSolidBrush(fondo);
    FillRect(dc, &barraTitulo, b);
    DeleteObject(b);
    redondeado(dc, rect(barraTitulo.left, barraTitulo.bottom - 1, barraTitulo.right - barraTitulo.left, 1), 0,
               RGB(28, 30, 36));
    int h = barraTitulo.bottom - barraTitulo.top;
    int x = barraTitulo.left + E(s, 10);
    bool listo = s->vista != V_ARRANCANDO && s->vista != V_ERROR;

    // Volver
    bool puedeVolver = listo && (!s->historial.empty() || s->vista != V_HUB || !s->zoom.empty());
    RECT atras = rect(x, (h - E(s, 34)) / 2, E(s, 34), E(s, 34));
    if (puedeVolver) {
        if (encima(s, A_ATRAS)) redondeado(dc, atras, E(s, 8), encimaFondo);
        int cx = (atras.left + atras.right) / 2, cy = (atras.top + atras.bottom) / 2, k = E(s, 6);
        linea(dc, (float)cx + k * 0.4f, (float)cy - k, (float)cx - k * 0.6f, (float)cy, tinta, (float)E(s, 2));
        linea(dc, (float)cx - k * 0.6f, (float)cy, (float)cx + k * 0.4f, (float)cy + k, tinta, (float)E(s, 2));
        zona(s, atras, A_ATRAS);
    }
    x = atras.right + E(s, 8);
    // Logotipo y titulo
    redondeado(dc, rect(x, (h - E(s, 28)) / 2, E(s, 28), E(s, 28)), E(s, 8), tm.acento);
    texto(dc, s->f.negrita, RGB(255, 255, 255), L"U", rect(x, (h - E(s, 28)) / 2, E(s, 28), E(s, 28)),
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    x += E(s, 38);
    int finTitulo = std::max(x + E(s, 120), (int)barraTitulo.left + anchoLateral(s));
    texto(dc, s->f.negrita, tinta, L"Profesor", rect(x, 0, finTitulo - x, h / 2 + E(s, 3)), DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    texto(dc, s->f.mini, suave, W(s->asignatura.nombre), rect(x, h / 2 + E(s, 2), finTitulo - x - E(s, 8), h / 2),
          DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    // Pantalla actual
    if (listo) {
        int px = barraTitulo.left + anchoLateral(s) + E(s, 28);
        texto(dc, s->f.subtitulo, tinta, nombreVista(s), rect(px, 0, E(s, 300), h), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Botones de ventana (derecha)
    int bw = E(s, 46);
    int bx = barraTitulo.right;
    std::vector<int> botones;
    if (s->pantallaCompleta) botones = {A_CERRAR, A_PANTALLA_COMPLETA};
    else botones = {A_CERRAR, A_MAXIMIZAR, A_MINIMIZAR, A_PANTALLA_COMPLETA};
    for (int accion : botones) {
        bool salir = accion == A_PANTALLA_COMPLETA && s->pantallaCompleta;
        int ancho = salir ? E(s, 190) : bw;
        bx -= ancho;
        RECT r = rect(bx, 0, ancho, h - 1);
        bool hov = encima(s, accion);
        if (salir) {  // «Salir de pantalla completa»: un boton con texto, que se vea bien
            RECT p = rect(bx + E(s, 6), (h - E(s, 32)) / 2, ancho - E(s, 12), E(s, 32));
            redondeado(dc, p, E(s, 16), hov ? encimaFondo : mezcla(fondo, RGB(255, 255, 255), 0.07), mezcla(fondo, tm.acento, 0.6));
            iconoVentana(s, dc, rect(p.left + E(s, 4), p.top, E(s, 30), p.bottom - p.top), accion, tinta);
            texto(dc, s->f.peque, tinta, L"Salir de pantalla completa", rect(p.left + E(s, 32), p.top, p.right - p.left - E(s, 36), p.bottom - p.top),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            zona(s, p, accion);
            continue;
        }
        if (hov) {
            HBRUSH hb = CreateSolidBrush(accion == A_CERRAR ? RGB(196, 43, 28) : encimaFondo);
            FillRect(dc, &r, hb);
            DeleteObject(hb);
        }
        iconoVentana(s, dc, r, accion, tinta);
        zona(s, r, accion);
    }
    if (!listo) return;

    // Estado de la IA: lo que esta haciendo, o si esta lista. Pulsando se va a esa pantalla.
    {
        const js::Valor& ia = s->hola["ia"];
        std::wstring t;
        COLORREF c;
        int accion = A_NADA, dato = 0;
        bool pulso = false;
        std::wstring que;
        int vista = V_HUB;
        const TareaIA* tarea = NULL;
        if (!ia["disponible"].comoBool()) {
            bool instalado = ia["instalado"].comoBool();
            t = !instalado ? L"Sin IA  ·  modo rápido" : iaExterna(s) ? L"IA sin conexión  ·  Reintentar" : L"IA apagada  ·  Iniciar";
            c = suave;
            if (instalado) accion = A_ARRANCAR_IA;
        } else if (actividadIA(s, que, vista, tarea)) {
            t = que;
            if (tarea && tarea->inicio) t += L"  ·  " + tiempo((int)((GetTickCount64() - tarea->inicio) / 1000));
            c = RGB(167, 139, 250);
            pulso = true;
            if (vista != s->vista) {
                accion = A_PESTANA;
                dato = vista;
            }
        } else if (s->iaFase == "cargando") {
            t = L"Cargando " + W(ia["modelo"]) + L"…";
            c = tm.medio;
            pulso = true;
        } else if (s->reserva["estado"].comoCadena() == "pausada") {
            t = L"IA lista  ·  reserva en pausa con batería";  // la reserva sigue al enchufar el cargador
            c = tm.ok;
        } else {
            t = L"IA lista  ·  " + W(ia["modelo"]);
            c = tm.ok;
        }
        int tw = std::min(anchoTexto(dc, s->f.peque, t) + E(s, 44), E(s, 330));  // el texto largo se corta con «…»
        RECT r = rect(bx - E(s, 14) - tw, (h - E(s, 30)) / 2, tw, E(s, 30));
        int tope = r.left - E(s, 10);  // hasta donde llegan los botones de la derecha
        if (r.left > barraTitulo.left + anchoLateral(s) + E(s, 420)) {
            int px = r.left - E(s, 10) - pintarPomodoro(s, dc, r.left - E(s, 10), h) - E(s, 8);
            tope = px;
            if (px > barraTitulo.left + anchoLateral(s) + E(s, 330)) {
                px -= pintarChipMusica(s, dc, px, h) + E(s, 8);
                tope = px;
                if (px > barraTitulo.left + anchoLateral(s) + E(s, 240)) tope = px - pintarChipEnfoque(s, dc, px, h) - E(s, 8);
            }
        }
        // Caja de la paleta de comandos tras el nombre de la pantalla: en una columna fija, para que no
        // baile al cambiar de pantalla (solo se aparta si el nombre es muy largo)
        int finNombre = barraTitulo.left + anchoLateral(s) + E(s, 28) + anchoTexto(dc, s->f.subtitulo, nombreVista(s)) + E(s, 24);
        finNombre = std::max(finNombre, (int)barraTitulo.left + anchoLateral(s) + E(s, 170));
        pintarCajaComandos(s, dc, finNombre, tope - E(s, 8), h);
        if (r.left > barraTitulo.left + anchoLateral(s) + E(s, 200)) {
            bool hov = accion != A_NADA && encima(s, accion, dato);
            redondeado(dc, r, E(s, 15), hov ? encimaFondo : RGB(16, 17, 22), mezcla(RGB(16, 17, 22), c, 0.45));
            int cx = r.left + E(s, 16), cy = (r.top + r.bottom) / 2;
            if (pulso) {
                double fase = (s->animacion % 20) / 20.0;
                circulo(dc, cx, cy, E(s, 4) + (int)(E(s, 4) * fase), mezcla(RGB(16, 17, 22), c, 0.6 * (1 - fase)));
            }
            circulo(dc, cx, cy, E(s, 4), c);
            texto(dc, s->f.peque, tinta, t, rect(cx + E(s, 12), r.top, r.right - cx - E(s, 22), r.bottom - r.top),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (accion != A_NADA) zona(s, r, accion, dato);
        }
    }
}

// --------------------------------------------------------------- general

void pintarEspera(Estado* s, HDC dc, RECT r, const std::wstring& t) {
    const gui::Tema& tm = gui::tema();
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2 - E(s, 30);
    spinner(s, dc, cx, cy, E(s, 26));
    texto(dc, s->f.subtitulo, tm.texto, t, rect(r.left, cy + E(s, 44), r.right - r.left, E(s, 30)),
          DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void pintarError(Estado* s, HDC dc, RECT r) {
    const gui::Tema& tm = gui::tema();
    int w = std::min(E(s, 560), (int)(r.right - r.left) - E(s, 40));
    int x = (r.left + r.right - w) / 2;
    int h = altoTexto(dc, s->f.texto, s->error, w - E(s, 48));
    int alto = h + E(s, 150);
    int y = (r.top + r.bottom - alto) / 2;
    redondeado(dc, rect(x, y, w, alto), E(s, 18), tm.panel, mezcla(tm.borde, tm.mal, 0.5));
    marca(dc, x + w / 2, y + E(s, 36), E(s, 18), false);
    texto(dc, s->f.texto, tm.texto, s->error, rect(x + E(s, 24), y + E(s, 70), w - E(s, 48), h), DT_CENTER | DT_WORDBREAK);
    boton(s, dc, rect(x + w / 2 - E(s, 90), y + alto - E(s, 58), E(s, 180), E(s, 40)), L"Reintentar", A_REINTENTAR, 0, PRIMARIO);
}

void pintarZoom(Estado* s, HDC dc, RECT r) {
    // Fondo oscurecido sobre todo lo de debajo de la barra de titulo
    {
        Gdiplus::Graphics g(dc);
        Gdiplus::SolidBrush velo(Gdiplus::Color(215, 8, 10, 16));
        g.FillRectangle(&velo, (int)r.left, (int)r.top, (int)(r.right - r.left), (int)(r.bottom - r.top));
    }
    RECT marco = r;
    InflateRect(&marco, -E(s, 60), -E(s, 50));
    marco.bottom -= E(s, 30);
    RECT lienzo = marco;
    Gdiplus::Image* im = imagen(s, s->zoom);
    if (im) {
        double esc = std::min((double)(marco.right - marco.left) / im->GetWidth(), (double)(marco.bottom - marco.top) / im->GetHeight());
        esc = std::min(esc, 2.5);
        int w = (int)(im->GetWidth() * esc), h = (int)(im->GetHeight() * esc);
        lienzo = rect((marco.left + marco.right - w) / 2, (marco.top + marco.bottom - h) / 2, w, h);
        RECT fondo = lienzo;
        InflateRect(&fondo, E(s, 10), E(s, 10));
        redondeado(dc, fondo, E(s, 14), RGB(255, 255, 255));
        imagenAjustada(im, dc, lienzo);
    }
    texto(dc, s->f.texto, RGB(230, 235, 245), s->zoomPie + L"   ·   clic o Esc para cerrar",
          rect(r.left, lienzo.bottom + E(s, 18), r.right - r.left, E(s, 24)), DT_CENTER | DT_SINGLELINE);
    zona(s, r, A_CERRAR_ZOOM);
}

// Columna central del contenido (la misma para el pintado y para colocar el EDIT).
void columna(Estado* s, int izquierda, int derecha, int& x, int& ancho) {
    // El mismo ancho en todas las pantallas: al cambiar de pestaña el título y el contenido no saltan de sitio
    int maximo = E(s, 1100);
    maximo = (int)(maximo * gui::anchuraEstilo());
    if (s->enfoque) maximo = std::min(maximo, s->vista == V_RESUMENES ? E(s, 1160) : E(s, 900));  // lectura comoda

    ancho = std::min((derecha - izquierda) - 2 * E(s, 40), maximo);
    x = izquierda + ((derecha - izquierda) - ancho) / 2;
}

void colocarEdit(Estado* s) {
    if (campoEnPanelLectura(s)) {  // dentro del panel del lector: lo coloca su pintado
        RECT c = s->lecturaCampo;
        if (c.right > c.left) MoveWindow(s->edit, c.left, c.top, c.right - c.left, c.bottom - c.top, TRUE);
        return;
    }
    RECT cliente;
    GetClientRect(s->ventana, &cliente);
    int x, ancho;
    columna(s, anchoLateral(s), cliente.right, x, ancho);
    RECT pie = {x, cliente.bottom - E(s, 84), x + ancho, cliente.bottom};
    RECT campo = campoChat(s, pie);
    MoveWindow(s->edit, campo.left + E(s, 20), campo.top + E(s, 13), campo.right - campo.left - E(s, 34),
               campo.bottom - campo.top - E(s, 22), TRUE);
}

// Fondo: degradado vertical y halos suaves del color de acento y violeta. Se calcula una
// vez por tamano de ventana y se copia en cada pintado.
void prepararFondo(Estado* s, HDC dc, int w, int h) {
    if (s->fondo && s->fondoAncho == w && s->fondoAlto == h) return;
    const gui::Tema& tm = gui::tema();
    if (s->fondo) DeleteObject(s->fondo);
    s->fondo = CreateCompatibleBitmap(dc, std::max(1, w), std::max(1, h));
    s->fondoAncho = w;
    s->fondoAlto = h;
    HDC m = CreateCompatibleDC(dc);
    HGDIOBJ viejo = SelectObject(m, s->fondo);
    {
        Gdiplus::Graphics g(m);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        COLORREF arriba = tm.oscuro ? mezcla(tm.fondo, RGB(255, 255, 255), 0.015) : tm.fondo;
        COLORREF abajo = tm.oscuro ? mezcla(tm.fondo, RGB(0, 0, 0), 0.45) : mezcla(tm.fondo, tm.acento, 0.035);
        Gdiplus::LinearGradientBrush base(Gdiplus::PointF(0, -1), Gdiplus::PointF(0, (float)h + 1), gc(arriba), gc(abajo));
        g.FillRectangle(&base, 0, 0, w, h);
        float fw = (float)w, fh = (float)h;
        // Intensidad de los halos segun los Ajustes (0 = fondo liso)
        const double fuerza[] = {0.0, 0.45, 1.0, 1.7};
        double k = fuerza[std::max(0, std::min(3, gui::estilo().fondo))];
        if (k > 0) {
            halo(g, fw * 0.80f, -fh * 0.10f, fw * 0.50f, tm.acento, std::min(255, (int)((tm.oscuro ? 58 : 46) * k)));
            halo(g, fw * 0.12f, fh * 1.08f, fw * 0.42f, RGB(139, 92, 246), std::min(255, (int)((tm.oscuro ? 40 : 26) * k)));
            halo(g, fw * 0.50f, fh * 0.55f, fw * 0.30f, RGB(45, 212, 191), std::min(255, (int)((tm.oscuro ? 10 : 8) * k)));
        }
    }
    SelectObject(m, viejo);
    DeleteDC(m);
}

// Copia un trozo del fondo (para repintar debajo del lateral y del pie).
void trozoDeFondo(Estado* s, HDC dc, const RECT& r) {
    HDC m = CreateCompatibleDC(dc);
    HGDIOBJ viejo = SelectObject(m, s->fondo);
    BitBlt(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, m, r.left, r.top, SRCCOPY);
    SelectObject(m, viejo);
    DeleteDC(m);
}

// --------------------------------------------------------- panel «Hacer un test»
// En pantalla completa y en el modo enfoque no hay menu lateral: este panel (boton «Test» de la
// capsula o Ctrl+T) deja empezar, seguir o configurar un test sin salir.

// Tema que se esta leyendo en el Temario, para ofrecer un test solo de ese tema.
bool temaActual(Estado* s, std::string& ruta, std::wstring& nombre) {
    if (s->vista == V_TEMARIO && !s->lecturaRuta.empty()) {
        ruta = s->lecturaRuta;
        nombre = W(s->lectura["tema"]);
        return !nombre.empty();
    }
    return false;
}

void pintarPanelTest(Estado* s, HDC dc, RECT cliente) {
    if (!s->panelTest) return;
    const gui::Tema& tm = gui::tema();
    velo(dc, cliente, RGB(0, 0, 0), tm.oscuro ? 90 : 55);
    zona(s, cliente, A_CERRAR_PANEL_TEST);  // un clic fuera lo cierra
    struct Fila {
        Icono icono;
        std::wstring titulo, detalle;
        int accion;
        COLORREF color;
    };
    std::vector<Fila> filas;
    const js::Valor& res = s->hola["resumen"];
    int n = s->config["n"].comoEntero(10);
    std::string modo = cfg(s, "modo", "mixto");
    std::wstring nombreModo = modo == "ia" ? L"solo IA" : modo == "mixto" ? L"mixto" : L"rápido, sin IA";
    bool enCurso = hayTest(s) && !testTerminado(s);
    if (enCurso)
        filas.push_back({I_JUGAR, L"Seguir con el test",
                         std::to_wstring(s->sesion["respondidas"].comoEntero()) + L" de " + std::to_wstring(s->sesion["n"].comoEntero()) +
                             L" respondidas",
                         A_PT_SEGUIR, tm.ok});
    filas.push_back({I_TEST, L"Test de todo el temario",
                     std::to_wstring(n) + L" preguntas · modo " + nombreModo + L" · dificultad " + W(cfg(s, "dificultad", "Intermedia")),
                     A_PT_RAPIDO, tm.acento});
    std::string ruta;
    std::wstring tema;
    if (temaActual(s, ruta, tema))
        filas.push_back({I_DIANA, L"Test de «" + tema + L"»", L"El tema que estás leyendo", A_PT_TEMA, RGB(20, 184, 166)});
    int repasar = res["repasar"].comoEntero(), fallos = res["mis_fallos"].comoEntero();
    if (repasar > 0)
        filas.push_back({I_REPETIR, L"Repaso de lo que toca hoy", plural(repasar, L"pregunta en su día de repaso", L"preguntas en su día de repaso"),
                         A_PT_REPASO, tm.medio});
    if (fallos > 0)
        filas.push_back({I_FALLOS, L"Test de mis fallos", plural(fallos, L"pregunta guardada", L"preguntas guardadas"), A_PT_FALLOS, tm.mal});
    filas.push_back({I_RELOJ, L"Simulacro de examen", L"20 preguntas · 1 min cada una · los fallos restan", A_PT_SIMULACRO,
                     RGB(139, 92, 246)});
    filas.push_back({I_LIBRO, L"Configurar un test…", L"Temas, formatos, corrección al final, tiempo…", A_PT_CONFIGURAR,
                     tm.textoSuave});

    int w = std::min(E(s, 620), (int)cliente.right - E(s, 40)), filaAlto = E(s, 64);
    int alto = E(s, 64) + E(s, 58) + (int)filas.size() * filaAlto + E(s, 12);
    int x = (cliente.right - w) / 2;
    int y = std::max(E(s, 10), std::min(E(s, 70), (int)cliente.bottom - alto - E(s, 10)));
    RECT panel = rect(x, y, w, alto);
    redondeado(dc, panel, E(s, 18), tm.panel, mezcla(tm.borde, tm.acento, 0.35));
    zona(s, panel, A_PT_CANTIDAD, -1);  // un clic en el panel (fuera de sus botones) no lo cierra
    glifo(s, dc, I_TEST, x + E(s, 30), y + E(s, 32), E(s, 20), tm.acento);
    texto(dc, s->f.subtitulo, tm.texto, L"Hacer un test", rect(x + E(s, 52), y + E(s, 16), w - E(s, 160), E(s, 32)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    tecla(s, dc, x + w - E(s, 96), y + E(s, 21), L"Ctrl");
    tecla(s, dc, x + w - E(s, 54), y + E(s, 21), L"T");
    // Ajustes rapidos: numero de preguntas y modo (los mismos de «Nuevo test»)
    int fy = y + E(s, 64);
    int px = x + E(s, 20);
    texto(dc, s->f.peque, tm.textoSuave, L"Preguntas", rect(px, fy, E(s, 80), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    px += E(s, 84);
    for (int c : {5, 10, 15, 20}) px += chip(s, dc, px, fy, std::to_wstring(c), n == c, A_PT_CANTIDAD, c) + E(s, 6);
    px += E(s, 14);
    const char* modos[] = {"rapido", "mixto", "ia"};
    const wchar_t* nombres[] = {L"Rápido", L"Mixto", L"Solo IA"};
    for (int i = 0; i < 3; ++i) {
        if (px + E(s, 90) > x + w) break;
        px += chip(s, dc, px, fy, nombres[i], modo == modos[i], A_PT_MODO, i) + E(s, 6);
    }
    fy += E(s, 50);
    redondeado(dc, rect(x + E(s, 16), fy, w - E(s, 32), 1), 0, tm.borde);
    fy += E(s, 8);
    for (const Fila& f : filas) {
        RECT r = rect(x + E(s, 10), fy, w - E(s, 20), filaAlto - E(s, 4));
        bool h = encima(s, f.accion);
        bool principal = f.accion == (enCurso ? A_PT_SEGUIR : A_PT_RAPIDO);
        if (principal) redondeado(dc, r, E(s, 12), mezcla(tm.panel, f.color, h ? 0.24 : 0.14), mezcla(tm.borde, f.color, 0.5));
        else if (h) redondeado(dc, r, E(s, 12), tm.panelAlt);
        int cx = r.left + E(s, 28), cy = (r.top + r.bottom) / 2;
        circulo(dc, cx, cy, E(s, 18), mezcla(tm.panel, f.color, 0.22));
        glifo(s, dc, f.icono, cx, cy, E(s, 16), f.color);
        texto(dc, principal ? s->f.negrita : s->f.texto, tm.texto, f.titulo, rect(cx + E(s, 30), r.top + E(s, 9), r.right - cx - E(s, 70), E(s, 24)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.mini, tm.textoSuave, f.detalle, rect(cx + E(s, 30), r.top + E(s, 33), r.right - cx - E(s, 70), E(s, 18)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.texto, h ? tm.texto : tm.textoSuave, L"→", rect(r.right - E(s, 40), r.top, E(s, 30), r.bottom - r.top),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        zona(s, r, f.accion);
        fy += filaAlto;
    }
}

// Acciones del panel. Devuelve true si la ha atendido.
bool panelTestAccion(Estado* s, int accion, int dato) {
    switch (accion) {
        case A_PANEL_TEST: s->panelTest = !s->panelTest; return true;
        case A_CERRAR_PANEL_TEST: s->panelTest = false; return true;
        case A_PT_CANTIDAD:
            if (dato > 0) s->config["n"] = dato;
            return true;
        case A_PT_MODO: {
            const char* modos[] = {"rapido", "mixto", "ia"};
            s->config["modo"] = modos[std::max(0, std::min(2, dato))];
            return true;
        }
        default: break;
    }
    if (accion < A_PT_RAPIDO || accion > A_PT_REPASO) return false;
    s->panelTest = false;
    switch (accion) {
        case A_PT_SEGUIR: abrirVista(s, V_PREGUNTA); break;
        case A_PT_RAPIDO: {
            js::Valor c = configParaEnviar(s);
            c["temas"] = js::Valor::array();  // todos
            empezarCon(s, c, L"Preparando tu test…");
            break;
        }
        case A_PT_TEMA: {
            std::string ruta;
            std::wstring tema;
            if (!temaActual(s, ruta, tema)) break;
            js::Valor c = configParaEnviar(s);
            js::Valor t = js::Valor::array();
            t.agregar(ruta);
            c["temas"] = t;
            empezarCon(s, c, L"Preparando un test de «" + tema + L"»…");
            break;
        }
        case A_PT_FALLOS: ejecutar(s, A_TEST_FALLOS, 0); break;
        case A_PT_SIMULACRO: ejecutar(s, A_SIMULACRO, 0); break;
        case A_PT_REPASO: ejecutar(s, A_REPASO, 0); break;
        case A_PT_CONFIGURAR: abrirVista(s, V_INICIO); break;
        default: break;
    }
    return true;
}

void pintar(Estado* s, HDC dc) {
    const gui::Tema& tm = gui::tema();
    RECT cliente;
    GetClientRect(s->ventana, &cliente);
    s->zonas.clear();
    s->listaCarril = s->listaPulgar = s->lecturaListaRect = RECT();  // solo los pone el Temario
    prepararFondo(s, dc, cliente.right, cliente.bottom);
    trozoDeFondo(s, dc, cliente);
    if (s->enfoque) velo(dc, cliente, tm.fondo, tm.oscuro ? 175 : 150);  // enfoque: fondo apagado, sin halos llamativos
    pintarAmbiente(s, dc, cliente);  // lluvia, olas, brasas... si suena musica de fondo

    int titulo = altoTitulo(s);
    RECT barraTitulo = {0, 0, cliente.right, titulo};
    RECT lado = {0, titulo, anchoLateral(s), cliente.bottom};
    bool conPie = s->vista == V_INICIO || s->vista == V_PREGUNTA || s->vista == V_RESULTADO || s->vista == V_CHAT ||
                  tienePie(s, s->vista);
    int altoPie = !conPie ? 0 : s->vista == V_CHAT ? E(s, 84) : E(s, 76);
    RECT cuerpo = {lado.right, titulo + (s->enfoque ? E(s, 56) : 0), cliente.right, cliente.bottom - altoPie};  // sitio para la capsula
    s->cuerpo = cuerpo;

    int cx0, ancho;
    columna(s, cuerpo.left, cuerpo.right - anchoPanelLectura(s), cx0, ancho);
    Area a = {cx0, cuerpo.top + E(s, 28) - s->scroll, ancho};
    s->altoVisible = cuerpo.bottom - cuerpo.top;

    int alto = 0;
    if (s->ocupado) {
        pintarEspera(s, dc, cuerpo, s->cargando);
    } else {
        switch (s->vista) {
            case V_ARRANCANDO: pintarEspera(s, dc, cuerpo, s->cargando); break;
            case V_ERROR: pintarError(s, dc, cuerpo); break;
            case V_INICIO: alto = pintarInicio(s, dc, a); break;
            case V_PREGUNTA: alto = pintarPregunta(s, dc, a); break;
            case V_RESULTADO: alto = pintarResultado(s, dc, a); break;
            case V_FALLOS: alto = pintarFallos(s, dc, a); break;
            case V_PROGRESO: alto = pintarProgreso(s, dc, a); break;
            case V_CHAT: alto = pintarChat(s, dc, a); break;
            case V_HUB: alto = pintarHub(s, dc, a); break;
            case V_TARJETAS: alto = pintarTarjetas(s, dc, a); break;
            case V_RESUMENES: alto = pintarResumenes(s, dc, a); break;
            case V_DESARROLLO: alto = pintarDesarrollo(s, dc, a); break;
            case V_BUSCAR: alto = pintarBuscar(s, dc, a); break;
            case V_TEMARIO: alto = pintarTemario(s, dc, a); break;
        }
    }
    s->altoContenido = alto + E(s, 40);
    int maxScroll = std::max(0, s->altoContenido - s->altoVisible);
    if (s->irACorreccion && s->vista == V_PREGUNTA) {
        s->irACorreccion = false;
        int objetivo = std::min(maxScroll, s->finCorreccion + E(s, 52) - s->altoVisible);
        if (objetivo > s->scroll) {
            s->scroll = objetivo;
            InvalidateRect(s->ventana, NULL, FALSE);
        }
    }
    if (s->chatAlFinal && s->vista == V_CHAT) {
        s->chatAlFinal = false;
        if (s->scroll != maxScroll) {
            s->scroll = maxScroll;
            InvalidateRect(s->ventana, NULL, FALSE);
        }
    }
    for (Zona& z : s->zonas) {
        RECT visible;
        if (!IntersectRect(&visible, &z.r, &cuerpo)) z.accion = A_NADA;
        else z.r = visible;
    }

    // Barra de desplazamiento: fina; al pasar el raton se ensancha y se puede arrastrar o pulsar en el carril
    s->barraCarril = s->barraPulgar = RECT();
    if (s->altoContenido > s->altoVisible && !s->ocupado) {
        int pista = s->altoVisible - E(s, 16);
        int largo = std::max(E(s, 40), pista * s->altoVisible / s->altoContenido);
        int pos = (pista - largo) * s->scroll / std::max(1, maxScroll);
        s->barraCarril = rect(cuerpo.right - E(s, 16), cuerpo.top + E(s, 8), E(s, 16), pista);
        bool activa = s->barraArrastre == 1 || s->barraEncima == 1;
        int bw = activa ? E(s, 8) : E(s, 5);
        s->barraPulgar = rect(cuerpo.right - E(s, 7) - bw / 2 - E(s, 1), cuerpo.top + E(s, 8) + pos, bw, largo);
        if (activa) redondeado(dc, rect(s->barraPulgar.left, s->barraCarril.top, bw, pista), bw / 2, mezcla(tm.fondo, tm.textoSuave, 0.12));
        redondeado(dc, s->barraPulgar, bw / 2, mezcla(tm.fondo, tm.textoSuave, activa ? 0.7 : 0.45));
        s->barraPulgar.left = s->barraCarril.left;  // para agarrarla basta con acertar en el carril
        s->barraPulgar.right = s->barraCarril.right;
    }

    if (conPie) {
        RECT pie = {cuerpo.left, cuerpo.bottom, cliente.right, cliente.bottom};
        // Pie translucido: deja ver los halos del fondo
        trozoDeFondo(s, dc, pie);
        velo(dc, pie, tm.fondo, tm.oscuro ? 150 : 170);
        if (s->vista != V_CHAT) redondeado(dc, rect(pie.left, pie.top, pie.right - pie.left, 1), 0, tm.borde);
        RECT interior = {a.x, pie.top, a.x + a.w, pie.bottom};
        if (!s->ocupado) {
            if (s->vista == V_INICIO) pieInicio(s, dc, interior);
            else if (s->vista == V_PREGUNTA) piePregunta(s, dc, interior);
            else if (s->vista == V_RESULTADO) pieResultado(s, dc, interior);
            else if (s->vista == V_CHAT) pieChat(s, dc, interior);
            else if (s->vista == V_TARJETAS) pieTarjetas(s, dc, interior);
            else if (s->vista == V_RESUMENES) pieResumenes(s, dc, interior);
            else if (s->vista == V_DESARROLLO) pieDesarrollo(s, dc, interior);
        }
    }
    if (!s->enfoque) {
        pintarLateral(s, dc, lado);
        pintarToast(s, dc, cuerpo);
        pintarSelector(s, dc, lado);
    }
    if (!s->zoom.empty()) {
        s->zonas.clear();
        pintarZoom(s, dc, {0, titulo, cliente.right, cliente.bottom});
    }
    if (!s->enfoque) pintarTitulo(s, dc, barraTitulo);
    pintarCapsulaEnfoque(s, dc, cliente);
    pintarPanelMusica(s, dc, cliente);
    pintarPanelTest(s, dc, cliente);
    pintarDescanso(s, dc, {0, titulo, cliente.right, cliente.bottom});
    pintarAtajos(s, dc, cliente);
    pintarComandos(s, dc, cliente);
    colocarRespuesta(s);
    colocarBusqueda(s);
    // Con una capa encima (paleta, ayuda, descanso) el campo del chat se esconde: si no, quedaria por encima
    bool chat = (s->vista == V_CHAT || campoEnPanelLectura(s)) && !hayCapa(s) && !s->panelTest;
    if ((IsWindowVisible(s->edit) != FALSE) != chat) ShowWindow(s->edit, chat ? SW_SHOW : SW_HIDE);
}

// ================================================================ acciones

void desplazar(Estado* s, int delta) {
    int maximo = std::max(0, s->altoContenido - s->altoVisible);
    int nuevo = std::max(0, std::min(maximo, s->scroll + delta));
    if (nuevo != s->scroll) {
        s->scroll = nuevo;
        InvalidateRect(s->ventana, NULL, FALSE);
    }
}

void alternarPantallaCompleta(Estado* s) {
    HWND w = s->ventana;
    if (!s->pantallaCompleta) {
        s->colocacion.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(w, &s->colocacion);
        MONITORINFO mi = MONITORINFO();
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi);
        s->pantallaCompleta = true;
        SetWindowLongPtrW(w, GWL_STYLE, (GetWindowLongPtrW(w, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowPos(w, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        s->mensaje = L"Pantalla completa. Para volver, pulsa «Salir» arriba a la derecha.";
    } else {
        s->pantallaCompleta = false;
        SetWindowLongPtrW(w, GWL_STYLE, (GetWindowLongPtrW(w, GWL_STYLE) & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(w, &s->colocacion);
        SetWindowPos(w, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    colocarEdit(s);
    InvalidateRect(w, NULL, FALSE);
}

void ejecutar(Estado* s, int accion, int dato) {
    HWND w = s->ventana;
    // Los botones de ventana funcionan siempre, aunque el profesor este ocupado.
    switch (accion) {
        case A_CERRAR: PostMessageW(w, WM_CLOSE, 0, 0); return;
        case A_MINIMIZAR: ShowWindow(w, SW_MINIMIZE); return;
        case A_MAXIMIZAR: ShowWindow(w, IsZoomed(w) ? SW_RESTORE : SW_MAXIMIZE); return;
        case A_PANTALLA_COMPLETA:
            if (s->enfoque) alternarEnfoque(s);
            else alternarPantallaCompleta(s);
            return;
        case A_CERRAR_ZOOM:
            s->zoom.clear();
            InvalidateRect(w, NULL, FALSE);
            return;
        default: break;
    }
    if (comandosAccion(s, accion, dato)) {  // paleta de comandos y ayuda
        InvalidateRect(w, NULL, FALSE);
        return;
    }
    if (enfoqueAccion(s, accion, dato)) return;  // enfoque y musica: tambien con el profesor ocupado
    if (!s->ocupado && panelTestAccion(s, accion, dato)) {
        InvalidateRect(w, NULL, FALSE);
        return;
    }
    if (s->ocupado && accion != A_CANCELAR_DUDA) return;
    const js::Valor& p = s->pregunta;
    int indice = p["indice"].comoEntero(), n = s->sesion["n"].comoEntero(1);
    switch (accion) {
        case A_PESTANA: abrirVista(s, (Vista)(dato % 100)); break;  // +100: otra zona hacia la misma vista
        case A_ATRAS: volver(s); break;
        case A_MODO: {
            const char* modos[] = {"rapido", "mixto", "ia"};
            s->config["modo"] = modos[dato];
            if (dato > 0 && s->hola["ia"]["disponible"].comoBool() && s->iaFase.empty()) {
                s->iaFase = "cargando";
                enviar(s, "precalentar");
            }
            break;
        }
        case A_CANTIDAD: s->config["n"] = dato; break;
        case A_DIFICULTAD: {
            const char* d[] = {"Básica", "Intermedia", "Avanzada"};
            s->config["dificultad"] = d[dato];
            break;
        }
        case A_TIPO: {
            const char* t[] = {"variado", "opcion", "vf"};
            s->config["tipo"] = t[dato];
            break;
        }
        case A_FORMATO: {
            // Marca o desmarca un formato; siempre queda al menos uno y «todos» se guarda como lista vacia
            const char* claves[] = {"definicion", "completar", "aplicacion", "relacion", "razonamiento", "falsa"};
            const js::Valor& actuales = s->config["formatos"];
            bool marcados[6];
            int cuantos = 0;
            for (int i = 0; i < 6; ++i) {
                marcados[i] = actuales.tam() == 0;
                for (std::size_t j = 0; j < actuales.tam(); ++j)
                    if (actuales[j].comoCadena() == claves[i]) marcados[i] = true;
            }
            if (dato < 0 || dato >= 6) break;
            marcados[dato] = !marcados[dato];
            for (bool m : marcados) cuantos += m ? 1 : 0;
            if (cuantos == 0) {
                s->mensaje = L"Deja al menos un tipo de pregunta.";
                break;
            }
            js::Valor lista = js::Valor::array();
            if (cuantos < 6)
                for (int i = 0; i < 6; ++i)
                    if (marcados[i]) lista.agregar(claves[i]);
            s->config["formatos"] = lista;
            break;
        }
        case A_CORRECCION: s->config["correccion"] = dato ? "final" : "inmediata"; break;
        case A_PENALIZAR: s->config["penalizar"] = !s->config["penalizar"].comoBool(); break;
        case A_REVISION: s->config["revision"] = !s->config["revision"].comoBool(); break;
        case A_REPETIR_TEST: {
            js::Valor c = js::Valor::objeto();
            c["repetir"] = s->sesion["token"].comoCadena();
            c["correccion"] = "inmediata";
            empezarCon(s, c, L"Preparando las preguntas que fallaste…");
            break;
        }
        case A_RESERVA: {
            s->config["reserva"] = !s->config["reserva"].comoBool(true);
            js::Valor p = js::Valor::objeto();
            p["activa"] = s->config["reserva"].comoBool();
            enviar(s, "reserva", p);
            break;
        }
        case A_AUTOGUARDAR: s->config["auto_guardar"] = !s->config["auto_guardar"].comoBool(true); break;
        case A_MODELO: s->config["modelo"] = s->hola["ia"]["modelos"][(std::size_t)dato].comoCadena(); break;
        case A_TEMA:
            if (dato >= 0 && dato < (int)s->temas.size()) s->temas[(std::size_t)dato] = !s->temas[(std::size_t)dato];
            break;
        case A_TEMA_GRUPO: {  // un tema entero: si estan todos sus apartados se quitan, si no se ponen
            const js::Valor& archivos = s->hola["archivos"];
            std::vector<FilaTema> filas = filasPorTema(archivos);
            std::wstring grupo;
            for (const FilaTema& f : filas)
                if (f.indice < 0 && f.grupo_n == dato) grupo = f.grupo;
            bool todos = true;
            for (std::size_t j = 0; j < archivos.tam() && j < s->temas.size(); ++j)
                if (W(archivos[j]["grupo"]) == grupo && !s->temas[j]) todos = false;
            for (std::size_t j = 0; j < archivos.tam() && j < s->temas.size(); ++j)
                if (W(archivos[j]["grupo"]) == grupo) s->temas[j] = !todos;
            break;
        }
        case A_TODOS: {
            bool todos = std::all_of(s->temas.begin(), s->temas.end(), [](bool b) { return b; });
            std::fill(s->temas.begin(), s->temas.end(), !todos);
            break;
        }
        case A_ABRIR_TEMARIO: gui::abrirEnSistema(w, W(carpetaDeAsignatura(s->asignatura, true))); break;
        case A_EMPEZAR: empezar(s, false); break;
        case A_PRACTICAR_TEMA: empezar(s, false, dato); break;
        case A_TEST_FALLOS: empezar(s, true); break;
        case A_N_FALLOS: s->nFallos = dato; break;
        case A_LIMITE: s->config["limite"] = dato; break;
        case A_CONTINUAR: enviar(s, "continuar", js::Valor::objeto(), true, L"Recuperando tu test…"); break;
        case A_OPCION:
            if (p["corregida"].comoBool() || testTerminado(s)) break;
            if (esExamen(s)) {
                s->seleccion = dato;
                responder(s, dato);
            } else if (s->seleccion == dato) {
                responder(s, dato);  // segundo clic sobre la misma opcion = comprobar
            } else {
                s->seleccion = dato;
            }
            break;
        case A_COMPROBAR:
            if (s->seleccion >= 0) responder(s, s->seleccion);
            break;
        case A_DUDOSA:
            if (p["corregida"].comoBool() || testTerminado(s)) break;
            s->dudosa = !s->dudosa;
            if (esExamen(s) && s->seleccion >= 0) responder(s, s->seleccion);  // en examen se guarda ya
            break;
        case A_SIGUIENTE:
            if (indice + 1 < n) {
                pedirPregunta(s, indice + 1);
            } else {
                const js::Valor& e = s->sesion["estados"];
                for (std::size_t i = 0; i < e.tam(); ++i) {
                    if (e[i].comoCadena() == "sin" || e[i].comoCadena() == "preparando") {
                        pedirPregunta(s, (int)i);
                        break;
                    }
                }
            }
            break;
        case A_ANTERIOR:
            if (indice > 0) pedirPregunta(s, indice - 1);
            break;
        case A_IR: pedirPregunta(s, dato); break;
        case A_TERMINAR: terminarTest(s, true); break;
        case A_ABRIR_FUENTE:
            if (!gui::abrirEnSistema(w, W(p["archivo"]))) s->mensaje = L"No he podido abrir el documento.";
            break;
        case A_ZOOM:
            s->zoom = W(p["imagen"]["archivo"]);
            s->zoomPie = W(p["tema"]) + L"  ·  " + W(p["imagen"]["pie"]);
            break;
        case A_GUARDAR: {
            js::Valor q = js::Valor::objeto();
            q["token"] = s->sesion["token"].comoCadena();
            q["indice"] = indice;
            q["guardar"] = !p["guardada"].comoBool();
            enviar(s, "guardar", q);
            break;
        }
        case A_DESCARTAR:
            if (gui::preguntar(w, L"¿Marcar esta pregunta como errónea? No volverá a salir en tus tests.")) {
                js::Valor q = js::Valor::objeto();
                q["token"] = s->sesion["token"].comoCadena();
                q["indice"] = indice;
                enviar(s, "descartar", q);
            }
            break;
        case A_DUDA_PREGUNTA:
            if (!exigirIa(s)) break;
            s->contextoPregunta.clear();
            s->contextoIndice = indice;
            enviarDuda(s, dato ? L"La he acertado, pero quiero entenderla a fondo. ¿Me explicas por qué es esa la respuesta y por qué no las otras?"
                               : L"No entiendo bien esta pregunta. ¿Me la explicas paso a paso?");
            break;
        case A_VER:
            if (s->resultado.esNulo()) break;
            pedirPregunta(s, s->resultado["preguntas"][(std::size_t)dato]["indice"].comoEntero(dato));
            s->mensaje = L"Repasa la explicación con calma.";
            irAVista(s, V_PREGUNTA);
            break;
        case A_QUITAR_FALLO: {
            js::Valor q = js::Valor::objeto();
            q["pregunta"] = s->fallos[(std::size_t)dato]["id"].comoCadena();
            enviar(s, "quitar_fallo", q);
            break;
        }
        case A_QUITAR_ACERTADAS: {
            js::Valor q = js::Valor::objeto();
            q["token"] = s->sesion["token"].comoCadena();
            q["acertadas"] = true;
            enviar(s, "quitar_fallo", q);
            break;
        }
        case A_RELLENAR: {
            js::Valor q = js::Valor::objeto();
            q["token"] = s->sesion["token"].comoCadena();
            enviar(s, "rellenar", q, true, L"Completando con preguntas rápidas…");
            break;
        }
        case A_ARRANCAR_IA:
            enviar(s, "arrancar_ia", js::Valor::objeto(), true,
                   iaExterna(s) ? L"Conectando con " + servidorIa(s) + L"…" : std::wstring(L"Arrancando Ollama…"));
            break;
        case A_ENVIAR: {
            int largo = GetWindowTextLengthW(s->edit);
            std::vector<wchar_t> b((std::size_t)largo + 1);
            GetWindowTextW(s->edit, b.data(), largo + 1);
            std::wstring duda = b.data();
            if (!duda.empty() && !s->chatEnCurso) {
                SetWindowTextW(s->edit, L"");
                enviarDuda(s, duda);
            }
            SetFocus(s->edit);
            break;
        }
        case A_SUGERENCIA:
            if (dato == 0) {
                s->contextoPregunta.clear();
                s->contextoIndice = indice;
            }
            if (dato >= 10) enviarDuda(s, W(s->inicio["sugerencias"][(std::size_t)(dato - 10)]));
            else enviarDuda(s, kSugerencias[dato]);
            break;
        case A_SEGUIMIENTO: enviarDuda(s, kSeguimientoTexto[dato]); break;
        case A_QUITAR_CONTEXTO:
            s->contextoIndice = -1;
            s->contextoPregunta.clear();
            break;
        case A_NUEVA_CONVERSACION:
            if (!s->chatEnCurso && gui::preguntar(w, L"¿Empezar una conversación nueva? La actual deja de mostrarse."))
                enviar(s, "nueva_conversacion");
            break;
        case A_COPIAR:
            if (dato >= 0 && dato < (int)s->chat.size() && copiar(w, sinFormato(s->chat[(std::size_t)dato].texto)))
                s->mensaje = L"Respuesta copiada al portapapeles.";
            break;
        case A_FUENTE: {
            std::size_t m = (std::size_t)dato / 16, k = (std::size_t)dato % 16;
            if (m < s->chat.size()) gui::abrirEnSistema(w, W(s->chat[m].fuentes[k]["archivo"]));
            break;
        }
        case A_CANCELAR_DUDA: enviar(s, "cancelar_duda"); break;
        case A_REINTENTAR: arrancarMotor(s); break;
        default:
            if (!estudioAccion(s, accion, dato) && !temarioAccion(s, accion, dato)) herramientasAccion(s, accion, dato);
            break;
    }
    InvalidateRect(w, NULL, FALSE);
}

void teclado(Estado* s, WPARAM tecla) {
    if (comandosTecla(s, tecla)) return;  // Ctrl+K, F1 y todo lo que se teclea con la paleta abierta
    if (s->pantallaDescanso) {
        if (tecla == VK_ESCAPE || tecla == VK_RETURN) ejecutar(s, tecla == VK_ESCAPE ? A_DESCANSO_CERRAR : A_DESCANSO_ESTUDIAR, 0);
        return;
    }
    if (tecla == VK_F11) {
        if (s->enfoque) alternarEnfoque(s);
        else alternarPantallaCompleta(s);
        return;
    }
    if (tecla == VK_F9) {  // modo enfoque
        alternarEnfoque(s);
        return;
    }
    if (tecla == 'M' && (GetKeyState(VK_CONTROL) & 0x8000)) {  // Ctrl+M: musica si / no
        if (musica::sonando()) musica::parar();
        else musica::tocar(musica::ultimo());
        if (musica::sonando()) SetTimer(s->ventana, TIMER_ANIMACION, 110, NULL);
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }
    if (tecla == 'T' && (GetKeyState(VK_CONTROL) & 0x8000) && s->vista != V_ARRANCANDO && s->vista != V_ERROR) {
        ejecutar(s, A_PANEL_TEST, 0);  // Ctrl+T: hacer un test desde cualquier pantalla
        return;
    }
    if (s->panelTest) {
        if (tecla == VK_ESCAPE) ejecutar(s, A_CERRAR_PANEL_TEST, 0);
        else if (tecla == VK_RETURN) ejecutar(s, hayTest(s) && !testTerminado(s) ? A_PT_SEGUIR : A_PT_RAPIDO, 0);
        return;  // con el panel abierto, el teclado no llega a la pantalla de debajo
    }
    if ((tecla == VK_ESCAPE || tecla == 'C') && s->vista == V_TEMARIO && !s->panelMusica && s->zoom.empty() &&
        temarioTecla(s, tecla))
        return;
    if (tecla == VK_ESCAPE) {
        if (s->panelMusica) ejecutar(s, A_MUSICA_CERRAR, 0);
        else if (s->selector) ejecutar(s, A_CERRAR_SELECTOR, 0);
        else if (!s->zoom.empty()) ejecutar(s, A_CERRAR_ZOOM, 0);
        else if (s->vista == V_CHAT) volver(s);
        // La pantalla completa y el modo enfoque no se quitan con Esc: solo con su boton «Salir»
        return;
    }
    if (tecla == VK_BACK || (tecla == VK_LEFT && (GetKeyState(VK_MENU) & 0x8000))) {
        volver(s);
        return;
    }
    if (tecla == 'F' && (GetKeyState(VK_CONTROL) & 0x8000) && !s->ocupado && s->vista != V_ARRANCANDO && s->vista != V_ERROR) {
        abrirVista(s, V_BUSCAR);  // Ctrl+F: buscar en los apuntes desde cualquier pantalla
        return;
    }
    if (s->vista == V_TEMARIO && !s->ocupado && temarioTecla(s, tecla)) return;
    if (estudioTecla(s, tecla)) return;
    if (s->vista == V_PREGUNTA && !s->ocupado && s->zoom.empty()) {
        const js::Valor& p = s->pregunta;
        int opciones = (int)p["opciones"].tam();
        bool vf = p["tipo"].comoCadena() == "vf";
        int elegida = -1;
        if (tecla >= '1' && tecla <= '4') elegida = (int)(tecla - '1');
        else if (tecla >= VK_NUMPAD1 && tecla <= VK_NUMPAD4) elegida = (int)(tecla - VK_NUMPAD1);
        else if (vf && tecla == 'V') elegida = 0;
        else if (vf && tecla == 'F') elegida = 1;
        else if (!vf && tecla >= 'A' && tecla <= 'D') elegida = (int)(tecla - 'A');
        if (elegida >= 0 && elegida < opciones && !p["corregida"].comoBool() && !p["preparando"].comoBool() &&
            !testTerminado(s)) {
            if (esExamen(s)) ejecutar(s, A_OPCION, elegida);
            else s->seleccion = elegida;
            InvalidateRect(s->ventana, NULL, FALSE);
            return;
        }
        if (tecla == 'S' && !p["preparando"].comoBool()) {  // «No estoy seguro»
            ejecutar(s, A_DUDOSA, 0);
            return;
        }
        if (tecla == VK_RETURN) {
            if (!p["corregida"].comoBool() && !esExamen(s) && s->seleccion >= 0 && !testTerminado(s)) ejecutar(s, A_COMPROBAR, 0);
            else if (p["indice"].comoEntero() + 1 < s->sesion["n"].comoEntero()) ejecutar(s, A_SIGUIENTE, 0);
            else if (!testTerminado(s) && todasRespondidas(s)) ejecutar(s, A_TERMINAR, 0);
            return;
        }
        if (tecla == VK_RIGHT) ejecutar(s, A_SIGUIENTE, 0);
        if (tecla == VK_LEFT) ejecutar(s, A_ANTERIOR, 0);
    }
    if (tecla == VK_NEXT) desplazar(s, s->altoVisible * 8 / 10);
    if (tecla == VK_PRIOR) desplazar(s, -s->altoVisible * 8 / 10);
    if (tecla == VK_DOWN) desplazar(s, E(s, 60));
    if (tecla == VK_UP) desplazar(s, -E(s, 60));
}

// ================================================================ ventana

int CALLBACK fuenteEncontrada(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp) {
    *(bool*)lp = true;
    return 0;
}

bool hayFuente(const wchar_t* nombre) {
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, nombre, LF_FACESIZE - 1);
    bool hay = false;
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, fuenteEncontrada, (LPARAM)&hay, 0);
    ReleaseDC(NULL, dc);
    return hay;
}

void crearFuentes(Estado* s) {
    HDC pantalla = GetDC(NULL);
    int ppp = GetDeviceCaps(pantalla, LOGPIXELSY);
    ReleaseDC(NULL, pantalla);
    s->factor = (ppp / 96.0) * (gui::escalaInterfaz() / 100.0);
    // La letra elegida en los Ajustes; si no, Segoe UI Variable (Windows 11) o Segoe UI.
    bool variable = hayFuente(L"Segoe UI Variable Display");
    const wchar_t* display = variable ? L"Segoe UI Variable Display" : L"Segoe UI";
    const wchar_t* cuerpo = variable ? L"Segoe UI Variable Text" : L"Segoe UI";
    const std::wstring& elegida = gui::estilo().fuente;
    if (!elegida.empty() && hayFuente(elegida.c_str())) display = cuerpo = elegida.c_str();
    double tamano = gui::estilo().texto / 100.0;
    auto fuente = [&](int px, int peso, const wchar_t* cara) {
        return CreateFontW(-E(s, (int)std::lround(px * tamano)), 0, 0, 0, peso, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, cara);
    };
    s->f.titulo = fuente(27, FW_SEMIBOLD, display);
    s->f.subtitulo = fuente(17, FW_SEMIBOLD, display);
    s->f.pregunta = fuente(20, FW_SEMIBOLD, display);
    s->f.texto = fuente(15, FW_NORMAL, cuerpo);
    s->f.negrita = fuente(15, FW_SEMIBOLD, cuerpo);
    s->f.peque = fuente(13, FW_NORMAL, cuerpo);
    s->f.grande = fuente(42, FW_BOLD, display);
    s->f.mini = fuente(12, FW_NORMAL, cuerpo);
    s->f.iniciales = fuente(72, FW_BOLD, display);
    s->f.tarjeta = fuente(23, FW_SEMIBOLD, display);
    s->f.enorme = fuente(56, FW_BOLD, display);
    s->f.lectura = fuente(s->lecturaTam, FW_NORMAL, cuerpo);
}

void borrarFuentes(Estado* s) {
    for (HFONT f : {s->f.titulo, s->f.subtitulo, s->f.pregunta, s->f.texto, s->f.negrita, s->f.peque, s->f.grande,
                    s->f.mini, s->f.iniciales, s->f.tarjeta, s->f.enorme, s->f.lectura}) {
        if (f) DeleteObject(f);
    }
    s->f = Fuentes();
}

// Campos de texto: en el chat, Intro envia (Mayus+Intro salta de linea); en la respuesta de
// desarrollo, Intro salta de linea y Ctrl+Intro entrega. Ctrl+A selecciona todo en los dos.
LRESULT CALLBACK procEdit(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    WNDPROC original = (WNDPROC)GetPropW(h, L"original");
    bool chat = GetDlgCtrlID(h) == ID_CHAT;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    // Los atajos «de cualquier pantalla» funcionan aunque se este escribiendo: Ctrl+K (paleta), Ctrl+T
    // (test), Ctrl+M (musica), Ctrl+F (buscar), F1 (atajos), F9 (enfoque), Alt+← y Alt+0…9 (secciones)
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    if (msg == WM_KEYDOWN && !alt && ((ctrl && (wp == 'K' || wp == 'T' || wp == 'M' || wp == 'F')) || wp == VK_F1 || wp == VK_F9)) {
        SendMessageW(GetParent(h), WM_KEYDOWN, wp, 0);
        return 0;
    }
    if (msg == WM_SYSKEYDOWN && (wp == VK_LEFT || (wp >= '0' && wp <= '9'))) {
        SendMessageW(GetParent(h), WM_SYSKEYDOWN, wp, lp);
        return 0;
    }
    if (msg == WM_SYSCHAR && wp >= '0' && wp <= '9') return 0;  // sin pitido
    // Ctrl+K, Ctrl+T y Ctrl+F no escriben nada; Ctrl+M tampoco (llega como un Intro con Ctrl pulsado)
    if (msg == WM_CHAR && (wp == 11 || wp == 20 || wp == 6 || (wp == 13 && ctrl && (GetKeyState('M') & 0x8000)))) return 0;
    if (msg == WM_KEYDOWN && wp == 'A' && ctrl) {
        SendMessageW(h, EM_SETSEL, 0, -1);
        return 0;
    }
    if (msg == WM_CHAR && (wp == 1 || (wp == 10 && !chat))) return 0;  // Ctrl+A y Ctrl+Intro no escriben nada
    if (GetDlgCtrlID(h) == ID_BUSQUEDA) {
        if (msg == WM_KEYDOWN && wp == VK_RETURN) {
            PostMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(ID_BUSQUEDA, 0xBEEF), 0);
            return 0;
        }
        if (msg == WM_CHAR && (wp == L'\r' || wp == 27)) return 0;
        if (msg == WM_KEYDOWN && (wp == VK_ESCAPE || wp == VK_F11)) {
            PostMessageW(GetParent(h), WM_KEYDOWN, wp, 0);
            return 0;
        }
        if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) InvalidateRect(GetParent(h), NULL, FALSE);
        return CallWindowProcW(original, h, msg, wp, lp);
    }
    if (!chat) {
        if (msg == WM_KEYDOWN && wp == VK_RETURN && ctrl) {
            PostMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(ID_RESPUESTA, 0xBEEF), 0);
            return 0;
        }
        if (msg == WM_KEYDOWN && (wp == VK_ESCAPE || wp == VK_F11)) {
            PostMessageW(GetParent(h), WM_KEYDOWN, wp, 0);
            return 0;
        }
        if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) InvalidateRect(GetParent(h), NULL, FALSE);
        return CallWindowProcW(original, h, msg, wp, lp);
    }
    if (msg == WM_KEYDOWN && wp == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        PostMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(ID_CHAT, 0xBEEF), 0);
        return 0;
    }
    if (msg == WM_KEYDOWN && (wp == VK_ESCAPE || wp == VK_F11)) {
        PostMessageW(GetParent(h), WM_KEYDOWN, wp, 0);
        return 0;
    }
    if (msg == WM_CHAR && (wp == L'\r' || wp == 27) && !(GetKeyState(VK_SHIFT) & 0x8000)) return 0;
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) InvalidateRect(GetParent(h), NULL, FALSE);
    return CallWindowProcW(original, h, msg, wp, lp);
}

// Arrastrando una barra de desplazamiento: lleva el contenido a donde este el raton.
void moverBarra(Estado* s, int y) {
    bool lista = s->barraArrastre == 2;
    RECT carril = lista ? s->listaCarril : s->barraCarril;
    RECT pulgar = lista ? s->listaPulgar : s->barraPulgar;
    int recorrido = (int)(carril.bottom - carril.top) - (int)(pulgar.bottom - pulgar.top);
    int maximo = lista ? s->lecturaListaMax : std::max(0, s->altoContenido - s->altoVisible);
    if (recorrido <= 0 || maximo <= 0) return;
    int pos = std::max(0, std::min(recorrido, y - s->barraAgarre - (int)carril.top));
    int nuevo = (int)((long long)pos * maximo / recorrido);
    if (!lista) {
        desplazar(s, nuevo - s->scroll);
    } else if (nuevo != s->lecturaListaScroll) {
        s->lecturaListaScroll = nuevo;
        InvalidateRect(s->ventana, NULL, FALSE);
    }
}

// Barras de desplazamiento (la de la derecha y la de la lista del Temario): se agarran y se
// arrastran, o se pulsa en su carril para saltar ahi. true si el raton es suyo.
bool barraRaton(Estado* s, UINT msg, POINT pt) {
    if (msg == WM_LBUTTONDOWN) {
        if (hayCapa(s) || !s->zoom.empty() || s->panelTest || s->ocupado) return false;
        for (int cual = 2; cual >= 1; --cual) {
            RECT carril = cual == 1 ? s->barraCarril : s->listaCarril;
            RECT pulgar = cual == 1 ? s->barraPulgar : s->listaPulgar;
            if (IsRectEmpty(&carril) || !PtInRect(&carril, pt)) continue;
            s->barraAgarre = PtInRect(&pulgar, pt) ? (int)(pt.y - pulgar.top) : (int)(pulgar.bottom - pulgar.top) / 2;
            s->barraArrastre = cual;
            SetCapture(s->ventana);
            moverBarra(s, pt.y);
            InvalidateRect(s->ventana, NULL, FALSE);
            return true;
        }
        return false;
    }
    if (msg == WM_MOUSEMOVE) {
        if (s->barraArrastre) {
            moverBarra(s, pt.y);
            return true;
        }
        int encima = 0;
        if (!hayCapa(s) && s->zoom.empty()) {
            if (!IsRectEmpty(&s->barraCarril) && PtInRect(&s->barraCarril, pt)) encima = 1;
            else if (!IsRectEmpty(&s->listaCarril) && PtInRect(&s->listaCarril, pt)) encima = 2;
        }
        if (encima != s->barraEncima) {
            s->barraEncima = encima;
            InvalidateRect(s->ventana, NULL, FALSE);
        }
        return false;
    }
    if (msg == WM_LBUTTONUP && s->barraArrastre) {
        s->barraArrastre = 0;
        ReleaseCapture();
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    return false;
}

LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    Estado* s = (Estado*)GetWindowLongPtrW(w, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        s = (Estado*)((CREATESTRUCTW*)lp)->lpCreateParams;
        s->ventana = w;
        SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)s);
        crearFuentes(s);
    }
    if (!s) return DefWindowProcW(w, msg, wp, lp);

    switch (msg) {
        case WM_NCCALCSIZE:
            // Sin barra de titulo de Windows: se conservan los bordes laterales e inferior
            // (redimensionar y sombra) y la parte de arriba es toda nuestra.
            if (wp && !s->pantallaCompleta) {
                NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lp;
                LONG arriba = p->rgrc[0].top;
                DefWindowProcW(w, msg, wp, lp);
                p->rgrc[0].top = arriba;
                if (IsZoomed(w)) p->rgrc[0].top += GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            LRESULT r = DefWindowProcW(w, msg, wp, lp);
            if (r != HTCLIENT || s->pantallaCompleta) return r;
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            ScreenToClient(w, &pt);
            if (!IsZoomed(w) && pt.y < E(s, 5)) return HTTOP;
            if (pt.y < altoTitulo(s)) {
                for (const Zona& z : s->zonas) {
                    if (z.accion != A_NADA && PtInRect(&z.r, pt)) return HTCLIENT;
                }
                return HTCAPTION;  // arrastrar la ventana; doble clic maximiza
            }
            return HTCLIENT;
        }
        case WM_CREATE: {
            s->edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 10, 10,
                                      w, (HMENU)(INT_PTR)ID_CHAT, GetModuleHandleW(NULL), NULL);
            SendMessageW(s->edit, WM_SETFONT, (WPARAM)s->f.texto, TRUE);
            SendMessageW(s->edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Escribe tu duda y pulsa Intro…");
            SetPropW(s->edit, L"original", (HANDLE)SetWindowLongPtrW(s->edit, GWLP_WNDPROC, (LONG_PTR)procEdit));
            s->respuesta = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                                           ES_WANTRETURN, 0, 0, 10, 10, w, (HMENU)(INT_PTR)ID_RESPUESTA, GetModuleHandleW(NULL), NULL);
            SendMessageW(s->respuesta, WM_SETFONT, (WPARAM)s->f.texto, TRUE);
            SendMessageW(s->respuesta, EM_SETLIMITTEXT, 6000, 0);
            SetPropW(s->respuesta, L"original", (HANDLE)SetWindowLongPtrW(s->respuesta, GWLP_WNDPROC, (LONG_PTR)procEdit));
            gui::tematizarDesplazamiento(s->respuesta);
            s->busqueda = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, w,
                                          (HMENU)(INT_PTR)ID_BUSQUEDA, GetModuleHandleW(NULL), NULL);
            SendMessageW(s->busqueda, WM_SETFONT, (WPARAM)s->f.texto, TRUE);
            SendMessageW(s->busqueda, EM_SETCUEBANNER, TRUE, (LPARAM)L"Busca un concepto en tus apuntes…");
            SetPropW(s->busqueda, L"original", (HANDLE)SetWindowLongPtrW(s->busqueda, GWLP_WNDPROC, (LONG_PTR)procEdit));
            std::wstring retrato = W(rutas::unir(rutas::directorioDelEjecutable(), "assets/profesor.png"));
            s->retrato = Gdiplus::Image::FromFile(retrato.c_str());
            SetWindowPos(w, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            arrancarMotor(s);
            return 0;
        }
        case WM_MOTOR: {
            if (wp == 1) {
                if (!s->cerrando) {
                    s->vista = V_ERROR;
                    s->ocupado = false;
                    s->error = L"El motor del profesor se ha detenido inesperadamente. Puedes reintentarlo; si "
                               L"vuelve a pasar, revisa datos\\profesor\\motor.log.";
                    InvalidateRect(w, NULL, FALSE);
                }
                return 0;
            }
            std::string* texto = (std::string*)lp;
            js::Valor v;
            std::string error;
            if (texto && js::Valor::parsear(*texto, v, error) && v.esObjeto()) {
                if (v.contiene("evento")) alRecibirEvento(s, v);
                else alRecibirRespuesta(s, v);
            }
            delete texto;
            return 0;
        }
        case WM_TIMER:
            if (wp == TIMER_ANIMACION) {
                ++s->animacion;
                // Rapido para fundidos y giros; despacio si solo se espera a la IA (que usa la CPU).
                bool fluido = animandoTest(s);  // animaciones del test: a 60 fps mientras duran
                bool rapido = s->ocupado || s->vista == V_ARRANCANDO || s->anterior || s->pantallaDescanso || fluido ||
                              (s->volteada && GetTickCount64() - s->volteo < 320);
                bool lento = s->chatEnCurso || animandoEstudio(s) || animandoHerramientas(s) || !s->generacion.esNulo() || s->comandos ||
                             s->reserva["estado"].comoCadena() == "trabajando" ||
                             s->iaFase == "cargando" || musica::sonando() ||
                             (s->vista == V_PREGUNTA && s->pregunta["preparando"].comoBool());
                if (rapido) SetTimer(w, TIMER_ANIMACION, s->anterior || s->volteada || fluido ? 15 : 50, NULL);
                else if (hayAmbiente(s)) SetTimer(w, TIMER_ANIMACION, GetForegroundWindow() == w ? 40 : 250, NULL);
                else if (lento) SetTimer(w, TIMER_ANIMACION, musica::sonando() && !s->chatEnCurso ? 220 : 110, NULL);
                else KillTimer(w, TIMER_ANIMACION);
            }
            if (wp == TIMER_BUSQUEDA) tickBusqueda(s);
            if (wp == TIMER_POMODORO) tickPomodoro(s);
            if (wp == TIMER_ARRASTRE) temarioTick(s);
            // Con limite de tiempo, al llegar a cero se entrega el test solo.
            if (wp == TIMER_RELOJ && !s->entregando && !s->ocupado && hayTest(s) && !testTerminado(s) &&
                segundosRestantes(s) == 0) {
                s->entregando = true;
                s->mensaje = L"¡Se acabó el tiempo! Entrego tu test tal como está.";
                terminarTest(s, false);
            }
            InvalidateRect(w, NULL, FALSE);
            return 0;
        case WM_SIZE:
            colocarEdit(s);
            desplazar(s, 0);
            InvalidateRect(w, NULL, FALSE);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)lp;
            mm->ptMinTrackSize.x = E(s, 1000);
            mm->ptMinTrackSize.y = E(s, 680);
            return 0;
        }
        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            if (barraRaton(s, msg, pt)) return 0;
            if (temarioRaton(s, msg, pt)) return 0;
            if (s->enfoque) {  // la capsula del enfoque se despierta al mover el raton
                bool dormida = GetTickCount64() - s->movido >= 2500;
                s->movido = GetTickCount64();
                if (dormida) InvalidateRect(w, NULL, FALSE);
            }
            int accion = A_NADA, dato = -1;
            for (auto it = s->zonas.rbegin(); it != s->zonas.rend(); ++it) {
                if (it->accion != A_NADA && PtInRect(&it->r, pt)) {
                    accion = it->accion;
                    dato = it->dato;
                    break;
                }
            }
            if (accion != s->hoverAccion || dato != s->hoverDato) {
                s->hoverAccion = accion;
                s->hoverDato = dato;
                InvalidateRect(w, NULL, FALSE);
            }
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, w, 0};
            TrackMouseEvent(&t);
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (s->barraArrastre && (HWND)lp != w) {  // otra ventana se llevo el raton: se suelta la barra
                s->barraArrastre = 0;
                InvalidateRect(w, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
        case WM_NCMOUSEMOVE:
            if (s->barraEncima && !s->barraArrastre) {
                s->barraEncima = 0;
                InvalidateRect(w, NULL, FALSE);
            }
            if (s->hoverAccion != A_NADA) {
                s->hoverAccion = A_NADA;
                s->hoverDato = -1;
                InvalidateRect(w, NULL, FALSE);
            }
            if (msg == WM_NCMOUSEMOVE) break;
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT && (s->barraEncima || s->barraArrastre)) {
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            if (LOWORD(lp) == HTCLIENT && s->hoverAccion == A_NADA) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(w, &pt);
                LPCWSTR cursor = temarioCursor(s, pt);
                if (cursor) {
                    SetCursor(LoadCursorW(NULL, cursor));
                    return TRUE;
                }
            }
            if (LOWORD(lp) == HTCLIENT && s->hoverAccion != A_NADA && s->hoverAccion != A_CERRAR &&
                s->hoverAccion != A_MINIMIZAR && s->hoverAccion != A_MAXIMIZAR) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            if (s->vista != V_CHAT) SetFocus(w);
            if (barraRaton(s, msg, pt)) return 0;
            temarioRaton(s, msg, pt);
            return 0;
        }
        case WM_LBUTTONUP: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            if (barraRaton(s, msg, pt)) return 0;
            if (temarioRaton(s, msg, pt)) return 0;
            for (auto it = s->zonas.rbegin(); it != s->zonas.rend(); ++it) {
                if (it->accion != A_NADA && PtInRect(&it->r, pt)) {
                    ejecutar(s, it->accion, it->dato);
                    break;
                }
            }
            return 0;
        }
        case WM_XBUTTONUP:
            if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) volver(s);  // boton "atras" del raton
            return TRUE;
        case WM_MOUSEWHEEL:
            if (s->comandos) {
                comandosTecla(s, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? VK_UP : VK_DOWN);
                return 0;
            }
            if (s->zoom.empty() && temarioRueda(s, GET_WHEEL_DELTA_WPARAM(wp), false, GET_KEYSTATE_WPARAM(wp))) return 0;
            if (s->zoom.empty() && !hayCapa(s)) desplazar(s, -GET_WHEEL_DELTA_WPARAM(wp) * E(s, 90) / WHEEL_DELTA);
            return 0;
        case 0x020E:  // WM_MOUSEHWHEEL: rueda lateral o el panel tactil, para mover de lado las paginas ampliadas
            if (s->zoom.empty() && temarioRueda(s, GET_WHEEL_DELTA_WPARAM(wp), true, GET_KEYSTATE_WPARAM(wp))) return 0;
            break;
        case WM_CHAR:
            if (comandosCaracter(s, (wchar_t)wp)) return 0;
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // Alt+1…9 y Alt+0: las secciones del menú lateral, en su orden
            // (en el orden en que se ven: sin IA no esta «Dudas» y con un test abierto aparecen sus entradas)
            if (msg == WM_SYSKEYDOWN && wp >= '0' && wp <= '9' && !hayCapa(s) && s->vista != V_ARRANCANDO) {
                if (wp == '0') {
                    ejecutar(s, A_PESTANA, V_FALLOS);
                } else {
                    std::vector<ItemNav> items = itemsNav(s);
                    std::size_t k = (std::size_t)(wp - '1');
                    if (k < items.size() && items[k].v != V_FALLOS) ejecutar(s, A_PESTANA, items[k].v);
                }
                return 0;
            }
            if (msg == WM_SYSKEYDOWN && wp != VK_LEFT) break;
            teclado(s, wp);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == ID_CHAT && HIWORD(wp) == 0xBEEF) ejecutar(s, A_ENVIAR, 0);
            if (LOWORD(wp) == ID_RESPUESTA && HIWORD(wp) == 0xBEEF) ejecutar(s, A_D_ENTREGAR, 0);
            if (LOWORD(wp) == ID_RESPUESTA && HIWORD(wp) == EN_CHANGE) InvalidateRect(w, NULL, FALSE);  // cuenta de palabras
            if (LOWORD(wp) == ID_BUSQUEDA && HIWORD(wp) == EN_CHANGE) {
                SetTimer(w, TIMER_BUSQUEDA, 300, NULL);  // busca cuando dejas de teclear
                InvalidateRect(w, NULL, FALSE);
            }
            if (LOWORD(wp) == ID_BUSQUEDA && HIWORD(wp) == 0xBEEF) tickBusqueda(s);
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            SetTextColor(dc, gui::tema().texto);
            SetBkColor(dc, gui::tema().panel);
            return (LRESULT)gui::brochaPanel();
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(w, &ps);
            RECT r;
            GetClientRect(w, &r);
            if (!s->marco || s->marcoAncho != r.right || s->marcoAlto != r.bottom) {
                if (s->marco) DeleteObject(s->marco);
                if (s->anterior) DeleteObject(s->anterior);
                s->anterior = NULL;
                s->marco = CreateCompatibleBitmap(dc, std::max(1L, r.right), std::max(1L, r.bottom));
                s->marcoAncho = r.right;
                s->marcoAlto = r.bottom;
            }
            HDC mem = CreateCompatibleDC(dc);
            HGDIOBJ viejo = SelectObject(mem, s->marco);
            pintar(s, mem);
            BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            // Fundido: la pantalla anterior se desvanece encima de la nueva.
            ULONGLONG pasado = GetTickCount64() - s->transicion;
            if (s->anterior && pasado < 200) {
                double t = pasado / 200.0;
                HDC ant = CreateCompatibleDC(dc);
                HGDIOBJ va = SelectObject(ant, s->anterior);
                BLENDFUNCTION mezclaAlfa = {AC_SRC_OVER, 0, (BYTE)(255 * (1 - t) * (1 - t)), 0};
                AlphaBlend(dc, 0, 0, r.right, r.bottom, ant, 0, 0, r.right, r.bottom, mezclaAlfa);
                SelectObject(ant, va);
                DeleteDC(ant);
            } else if (s->anterior) {
                DeleteObject(s->anterior);
                s->anterior = NULL;
            }
            SelectObject(mem, viejo);
            DeleteDC(mem);
            EndPaint(w, &ps);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            s->cerrando = true;
            KillTimer(w, TIMER_ANIMACION);
            KillTimer(w, TIMER_RELOJ);
            KillTimer(w, TIMER_POMODORO);
            KillTimer(w, TIMER_BUSQUEDA);
            KillTimer(w, TIMER_ENFOQUE);
            musica::parar();
            s->motor.detener();
            if (s->fondo) DeleteObject(s->fondo);
            if (s->marco) DeleteObject(s->marco);
            if (s->anterior) DeleteObject(s->anterior);
            s->fondo = s->marco = s->anterior = NULL;
            for (auto& par : s->imagenes) delete par.second;
            s->imagenes.clear();
            soltarHojas(s);
            delete s->retratoCache;
            s->retratoCache = NULL;
            delete s->retratoPeque;
            s->retratoPeque = NULL;
            delete s->retrato;
            s->retrato = NULL;
            borrarFuentes(s);
            return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

}  // namespace ui

void abrir(HWND padre, const Asignatura& asignatura, const std::vector<Asignatura>& todas) {
    using namespace ui;
    Gdiplus::GdiplusStartupInput entrada;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &entrada, NULL);
    {
        HINSTANCE inst = GetModuleHandleW(NULL);
        WNDCLASSEXW clase;
        ZeroMemory(&clase, sizeof(clase));
        clase.cbSize = sizeof(clase);
        clase.style = CS_DBLCLKS;
        clase.lpfnWndProc = proc;
        clase.hInstance = inst;
        clase.lpszClassName = kClase;
        clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
        clase.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
        clase.hIconSm = clase.hIcon;
        RegisterClassExW(&clase);

        Estado estado;
        estado.asignatura = asignatura;
        // Solo las asignaturas con algo en su Temario: las demas no tienen nada que estudiar aqui.
        for (const Asignatura& a : todas) {
            std::string temario = carpetaDeAsignatura(a, true);
            WIN32_FIND_DATAW datos;
            HANDLE h = FindFirstFileW(gui::aW(rutas::unir(temario, "*")).c_str(), &datos);
            bool algo = false;
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring n = datos.cFileName;
                    if (n != L"." && n != L"..") algo = true;
                } while (!algo && FindNextFileW(h, &datos));
                FindClose(h);
            }
            if (algo || a.codigo == asignatura.codigo) estado.todas.push_back(a);
        }
        HDC pantalla = GetDC(NULL);
        double factor = GetDeviceCaps(pantalla, LOGPIXELSY) / 96.0 * gui::escalaInterfaz() / 100.0;
        ReleaseDC(NULL, pantalla);
        RECT trabajo;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &trabajo, 0);
        int ancho = std::min((int)(1320 * factor), (int)(trabajo.right - trabajo.left));
        int alto = std::min((int)(900 * factor), (int)(trabajo.bottom - trabajo.top));
        std::wstring titulo = L"Profesor · " + gui::aW(asignatura.nombre);
        HWND w = CreateWindowExW(0, kClase, titulo.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                 trabajo.left + (trabajo.right - trabajo.left - ancho) / 2,
                                 trabajo.top + (trabajo.bottom - trabajo.top - alto) / 2, ancho, alto, padre, NULL, inst,
                                 &estado);
        if (w) {
            if (padre) EnableWindow(padre, FALSE);
            ShowWindow(w, SW_SHOWMAXIMIZED);  // se abre a pantalla completa de trabajo
            MSG m;
            while (IsWindow(w) && GetMessageW(&m, NULL, 0, 0) > 0) {
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
            if (padre) {
                EnableWindow(padre, TRUE);
                SetForegroundWindow(padre);
            }
        } else {
            gui::avisarError(padre, L"No se pudo abrir el profesor.");
        }
    }
    Gdiplus::GdiplusShutdown(token);
}

}  // namespace profesor
