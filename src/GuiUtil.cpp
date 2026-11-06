#include "Gui.hpp"

#include <shellapi.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include <gdiplus.h>

#include <map>

#include "Modelo.hpp"
#include "Rutas.hpp"

namespace gui {

// ---------------------------------------------------------------- conversion

std::wstring aW(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (n <= 0) return std::wstring();
    std::wstring salida((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &salida[0], n);
    return salida;
}

std::string aUtf8(const std::wstring& texto) {
    if (texto.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, texto.c_str(), (int)texto.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return std::string();
    std::string salida((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, texto.c_str(), (int)texto.size(), &salida[0], n, NULL, NULL);
    return salida;
}

// --------------------------------------------------------------------- tema

namespace {
Tema g_tema;
bool g_temaListo = false;
HBRUSH g_brochaFondo = NULL;
HBRUSH g_brochaPanel = NULL;
HBRUSH g_brochaAlt = NULL;

int g_escala = 100;
Estilo g_estilo;
HFONT g_fuenteBase = NULL;
HFONT g_fuenteNegrita = NULL;
HFONT g_fuenteTitulo = NULL;

void borrarFuentes() {
    if (g_fuenteBase) DeleteObject(g_fuenteBase);
    if (g_fuenteNegrita) DeleteObject(g_fuenteNegrita);
    if (g_fuenteTitulo) DeleteObject(g_fuenteTitulo);
    g_fuenteBase = NULL;
    g_fuenteNegrita = NULL;
    g_fuenteTitulo = NULL;
}
struct Paleta {
    const wchar_t* nombre;
    COLORREF tinte;
    double oscuro, claro;  // cuanto tinte se mezcla en cada tema
};

const Paleta PALETAS[] = {
    {L"Grafito", RGB(128, 128, 128), 0.0, 0.0},
    {L"Medianoche", RGB(46, 84, 196), 0.13, 0.07},
    {L"Océano", RGB(14, 150, 168), 0.12, 0.07},
    {L"Bosque", RGB(38, 150, 86), 0.11, 0.06},
    {L"Ciruela", RGB(132, 72, 196), 0.13, 0.07},
    {L"Vino", RGB(176, 44, 82), 0.12, 0.06},
    {L"Arena", RGB(190, 130, 60), 0.11, 0.09},
    {L"Carbón", RGB(0, 0, 0), 0.0, 0.0},  // negro puro en oscuro (pantallas OLED), blanco puro en claro
    {L"Sepia", RGB(176, 124, 64), 0.10, 0.13},   // papel envejecido: lo mas comodo para leer mucho rato
    {L"Pizarra", RGB(84, 104, 132), 0.16, 0.09},
    {L"Rosa", RGB(219, 39, 119), 0.11, 0.05},
};
const int N_PALETAS = (int)(sizeof(PALETAS) / sizeof(PALETAS[0]));
const int PALETA_CARBON = 7;
}  // namespace

const Estilo& estilo() { return g_estilo; }

void ponerEstilo(const Estilo& e) {
    bool fuentes = e.fuente != g_estilo.fuente || e.texto != g_estilo.texto;
    g_estilo = e;
    if (g_estilo.paleta < 0 || g_estilo.paleta >= N_PALETAS) g_estilo.paleta = 0;
    g_estilo.texto = g_estilo.texto < 85 ? 85 : g_estilo.texto > 135 ? 135 : g_estilo.texto;
    g_estilo.esquinas = g_estilo.esquinas < 0 ? 0 : g_estilo.esquinas > 200 ? 200 : g_estilo.esquinas;
    if (fuentes) borrarFuentes();
}

int numPaletas() { return N_PALETAS; }

const wchar_t* nombrePaleta(int paleta) {
    return paleta >= 0 && paleta < N_PALETAS ? PALETAS[paleta].nombre : PALETAS[0].nombre;
}

COLORREF muestraPaleta(int paleta, bool oscuro) {
    if (paleta == PALETA_CARBON) return oscuro ? RGB(0, 0, 0) : RGB(255, 255, 255);
    const Paleta& p = PALETAS[paleta >= 0 && paleta < N_PALETAS ? paleta : 0];
    return mezclar(oscuro ? RGB(22, 24, 30) : RGB(243, 244, 247), p.tinte, (oscuro ? p.oscuro : p.claro) * 2.2);
}

bool sistemaEnOscuro() {
    DWORD valor = 1, tam = sizeof(valor);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &valor, &tam) != ERROR_SUCCESS)
        return false;
    return valor == 0;
}

int radioEstilo(int radio) { return radio * g_estilo.esquinas / 100; }

double anchuraEstilo() { return g_estilo.anchura == 2 ? 10.0 : g_estilo.anchura == 1 ? 1.3 : 1.0; }

int escalaInterfaz() { return g_escala; }

int escalar(int medida) { return (medida * g_escala) / 100; }

void ponerTema(bool oscuro, COLORREF acento, int escala) {
    if (escala < 100) escala = 100;
    if (escala > 150) escala = 150;
    if (escala != g_escala) {
        g_escala = escala;
        borrarFuentes();
    }
    g_tema.oscuro = oscuro;
    // Grises neutros (sin el tinte azul marino de antes) y contrastes suaves entre capas.
    if (oscuro) {
        g_tema.fondo = RGB(12, 13, 17);
        g_tema.panel = RGB(22, 24, 30);
        g_tema.panelAlt = RGB(32, 35, 43);
        g_tema.texto = RGB(237, 239, 243);
        g_tema.textoSuave = RGB(146, 152, 166);
        g_tema.borde = RGB(42, 45, 55);
        g_tema.acento = RGB(99, 140, 255);
        g_tema.seleccion = RGB(42, 58, 102);
        g_tema.textoSeleccion = RGB(255, 255, 255);
        g_tema.ok = RGB(52, 211, 153);
        g_tema.medio = RGB(251, 191, 36);
        g_tema.mal = RGB(248, 113, 113);
    } else {
        g_tema.fondo = RGB(243, 244, 247);
        g_tema.panel = RGB(255, 255, 255);
        g_tema.panelAlt = RGB(239, 241, 246);
        g_tema.texto = RGB(20, 23, 31);
        g_tema.textoSuave = RGB(98, 105, 120);
        g_tema.borde = RGB(225, 228, 235);
        g_tema.acento = RGB(59, 100, 230);
        g_tema.seleccion = RGB(214, 225, 255);
        g_tema.textoSeleccion = RGB(16, 18, 24);
        g_tema.ok = RGB(16, 160, 106);
        g_tema.medio = RGB(196, 132, 10);
        g_tema.mal = RGB(220, 53, 69);
    }

    // Paleta: un tinte de color en el fondo, los paneles y los bordes
    const Paleta& pal = PALETAS[g_estilo.paleta];
    if (g_estilo.paleta == PALETA_CARBON) {
        if (oscuro) {
            g_tema.fondo = RGB(0, 0, 0);
            g_tema.panel = RGB(13, 13, 15);
            g_tema.panelAlt = RGB(24, 24, 28);
            g_tema.borde = RGB(36, 36, 42);
        } else {
            g_tema.fondo = RGB(255, 255, 255);
            g_tema.panel = RGB(255, 255, 255);
            g_tema.panelAlt = RGB(244, 245, 248);
            g_tema.borde = RGB(218, 221, 228);
        }
    } else if (oscuro ? pal.oscuro > 0 : pal.claro > 0) {
        double k = oscuro ? pal.oscuro : pal.claro;
        g_tema.fondo = mezclar(g_tema.fondo, pal.tinte, k * (oscuro ? 0.8 : 1.4));
        g_tema.panel = mezclar(g_tema.panel, pal.tinte, k * (oscuro ? 1.0 : 0.35));
        g_tema.panelAlt = mezclar(g_tema.panelAlt, pal.tinte, k * (oscuro ? 1.1 : 1.5));
        g_tema.borde = mezclar(g_tema.borde, pal.tinte, k * 1.4);
        g_tema.textoSuave = mezclar(g_tema.textoSuave, pal.tinte, k * 0.8);
    }
    if (g_estilo.contraste) {
        g_tema.textoSuave = mezclar(g_tema.textoSuave, g_tema.texto, 0.45);
        g_tema.borde = mezclar(g_tema.borde, g_tema.textoSuave, 0.30);
        g_tema.texto = oscuro ? RGB(255, 255, 255) : RGB(0, 0, 0);
    }

    // color de acento elegido por el usuario (0 = el del tema)
    if (acento != 0) {
        g_tema.acento = acento;
        g_tema.seleccion = mezclar(g_tema.panel, acento, oscuro ? 0.45 : 0.30);
    }

    if (g_brochaFondo) DeleteObject(g_brochaFondo);
    if (g_brochaPanel) DeleteObject(g_brochaPanel);
    if (g_brochaAlt) DeleteObject(g_brochaAlt);
    g_brochaFondo = CreateSolidBrush(g_tema.fondo);
    g_brochaPanel = CreateSolidBrush(g_tema.panel);
    g_brochaAlt = CreateSolidBrush(g_tema.panelAlt);
    g_temaListo = true;
}

const Tema& tema() {
    if (!g_temaListo) ponerTema(true);
    return g_tema;
}

HBRUSH brochaFondo() { tema(); return g_brochaFondo; }
HBRUSH brochaPanel() { tema(); return g_brochaPanel; }
HBRUSH brochaAlt()   { tema(); return g_brochaAlt; }

COLORREF mezclar(COLORREF a, COLORREF b, double t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t);
    int z = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t);
    return RGB(r, g, z);
}

COLORREF colorDeAsignatura(const std::string& colorHex, std::size_t indice) {
    unsigned long rgb = 0;
    if (!colorDesdeTexto(colorHex, rgb)) {
        colorDesdeTexto(colorDePaleta(indice), rgb);
    }
    COLORREF color = RGB((int)((rgb >> 16) & 0xFF), (int)((rgb >> 8) & 0xFF), (int)(rgb & 0xFF));
    return tema().oscuro ? color : mezclar(color, RGB(15, 30, 50), 0.38);
}

COLORREF colorDeNivel(int nivel) {
    const bool oscuro = tema().oscuro;
    switch (nivel) {
        case NIVEL_SUSPENSO:      return oscuro ? RGB(255, 92, 92)  : RGB(206, 45, 45);
        case NIVEL_APROBADO:      return oscuro ? RGB(255, 159, 67) : RGB(196, 106, 12);
        case NIVEL_BIEN:          return oscuro ? RGB(247, 209, 84) : RGB(176, 143, 16);
        case NIVEL_NOTABLE:       return oscuro ? RGB(62, 207, 142) : RGB(20, 148, 92);
        default:                  return oscuro ? RGB(120, 170, 255) : RGB(30, 106, 226);
    }
}

COLORREF colorDeNota(double nota) {
    return colorDeNivel(nivelDeNota(nota));
}

COLORREF colorBarraSuperior() {
    const Tema& t = tema();
    switch (g_estilo.barra) {
        case 1: return RGB(30, 32, 39);                                          // grafito
        case 2: return mezclar(RGB(8, 9, 12), t.acento, 0.36);                   // tinte del acento
        case 3: return mezclar(RGB(6, 7, 9), PALETAS[g_estilo.paleta].tinte,     // tinte de la paleta
                               g_estilo.paleta == 0 || g_estilo.paleta == PALETA_CARBON ? 0.12 : 0.34);
        default: return t.oscuro ? RGB(0, 0, 0) : RGB(20, 23, 31);              // negra
    }
}

COLORREF textoBarraSuperior() { return RGB(242, 244, 248); }

// ------------------------------------------------------------------ fuentes

HFONT fuenteInterfaz() {
    if (!g_fuenteBase) {
        NONCLIENTMETRICSW metricas;
        ZeroMemory(&metricas, sizeof(metricas));
        metricas.cbSize = sizeof(metricas);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metricas), &metricas, 0)) {
            LOGFONTW lf = metricas.lfMessageFont;
            wcsncpy(lf.lfFaceName, g_estilo.fuente.empty() ? L"Segoe UI" : g_estilo.fuente.c_str(), LF_FACESIZE - 1);
            lf.lfFaceName[LF_FACESIZE - 1] = 0;
            lf.lfQuality = CLEARTYPE_QUALITY;
            lf.lfHeight = (LONG)((double)lf.lfHeight * g_escala / 100.0 * g_estilo.texto / 100.0);
            g_fuenteBase = CreateFontIndirectW(&lf);
        }
        if (!g_fuenteBase) g_fuenteBase = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }
    return g_fuenteBase;
}

static HFONT variante(int peso, double factor) {
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    if (!GetObjectW(fuenteInterfaz(), sizeof(lf), &lf)) return fuenteInterfaz();
    lf.lfWeight = peso;
    lf.lfHeight = (LONG)(lf.lfHeight * factor);
    HFONT f = CreateFontIndirectW(&lf);
    return f ? f : fuenteInterfaz();
}

HFONT fuenteNegrita() {
    if (!g_fuenteNegrita) g_fuenteNegrita = variante(FW_SEMIBOLD, 1.0);
    return g_fuenteNegrita;
}

HFONT fuenteTitulo() {
    if (!g_fuenteTitulo) g_fuenteTitulo = variante(FW_SEMIBOLD, 1.25);
    return g_fuenteTitulo;
}

static BOOL CALLBACK ponerFuenteEnHijo(HWND hijo, LPARAM lp) {
    SendMessageW(hijo, WM_SETFONT, (WPARAM)(HFONT)lp, TRUE);
    return TRUE;
}

void aplicarFuente(HWND ventana) {
    EnumChildWindows(ventana, ponerFuenteEnHijo, (LPARAM)fuenteInterfaz());
}

// ------------------------------------------------------------------ controles

HWND crearEtiqueta(HWND padre, const std::wstring& texto, int id) {
    return CreateWindowExW(0, L"STATIC", texto.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10,
                           10, padre, (HMENU)(INT_PTR)id, NULL, NULL);
}

// Los botones se subclasean para poder resaltarlos al pasar el raton por encima.
namespace {

const int BOTON_DESTACADO = 1;
const int BOTON_ENCIMA = 2;
const int BOTON_EN_BARRA = 4;
WNDPROC g_procBoton = NULL;

LRESULT CALLBACK procBoton(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    LONG_PTR estado = GetWindowLongPtrW(h, GWLP_USERDATA);

    if (mensaje == WM_MOUSEMOVE && !(estado & BOTON_ENCIMA)) {
        SetWindowLongPtrW(h, GWLP_USERDATA, estado | BOTON_ENCIMA);
        TRACKMOUSEEVENT seguimiento;
        ZeroMemory(&seguimiento, sizeof(seguimiento));
        seguimiento.cbSize = sizeof(seguimiento);
        seguimiento.dwFlags = TME_LEAVE;
        seguimiento.hwndTrack = h;
        TrackMouseEvent(&seguimiento);
        InvalidateRect(h, NULL, TRUE);
    } else if (mensaje == WM_MOUSELEAVE) {
        SetWindowLongPtrW(h, GWLP_USERDATA, estado & ~(LONG_PTR)BOTON_ENCIMA);
        InvalidateRect(h, NULL, TRUE);
    }
    return CallWindowProcW(g_procBoton, h, mensaje, wp, lp);
}

}  // namespace

HWND crearBoton(HWND padre, const std::wstring& texto, int id, bool destacado) {
    HWND b = CreateWindowExW(0, L"BUTTON", texto.c_str(),
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10,
                             padre, (HMENU)(INT_PTR)id, NULL, NULL);
    if (b) {
        SetWindowLongPtrW(b, GWLP_USERDATA, destacado ? BOTON_DESTACADO : 0);
        WNDPROC anterior = (WNDPROC)SetWindowLongPtrW(b, GWLP_WNDPROC, (LONG_PTR)procBoton);
        if (anterior && anterior != procBoton) g_procBoton = anterior;
    }
    return b;
}

void botonEnBarra(HWND boton) {
    if (!boton) return;
    LONG_PTR estado = GetWindowLongPtrW(boton, GWLP_USERDATA);
    SetWindowLongPtrW(boton, GWLP_USERDATA, estado | BOTON_EN_BARRA);
}

// ------------------------------------------------------------------ iconos

// Coordenadas en fracciones del tamano (-0.5 .. 0.5 alrededor del centro), como los del Profesor.
void dibujarIcono(HDC dc, Icono icono, int cx, int cy, int tam, COLORREF c, int alfa) {
    if (icono == ICONO_NINGUNO) return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float t = (float)tam, x = (float)cx, y = (float)cy;
    const float grosor = t / 11.0f > 1.5f ? t / 11.0f : 1.5f;
    Gdiplus::Color color((BYTE)alfa, GetRValue(c), GetGValue(c), GetBValue(c));
    Gdiplus::Pen p(color, grosor);
    p.SetLineJoin(Gdiplus::LineJoinRound);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    Gdiplus::SolidBrush relleno(color);
    auto L = [&](float x1, float y1, float x2, float y2) { g.DrawLine(&p, x + x1 * t, y + y1 * t, x + x2 * t, y + y2 * t); };
    auto C = [&](float ox, float oy, float r) { g.DrawEllipse(&p, x + (ox - r) * t, y + (oy - r) * t, 2 * r * t, 2 * r * t); };
    auto P = [&](float ox, float oy, float r) { g.FillEllipse(&relleno, x + (ox - r) * t, y + (oy - r) * t, 2 * r * t, 2 * r * t); };
    auto R = [&](float x1, float y1, float w, float h, float radio) {
        Gdiplus::GraphicsPath camino;
        float r = radio * t * 2, X = x + x1 * t, Y = y + y1 * t, W = w * t, H = h * t;
        camino.AddArc(X, Y, r, r, 180, 90);
        camino.AddArc(X + W - r, Y, r, r, 270, 90);
        camino.AddArc(X + W - r, Y + H - r, r, r, 0, 90);
        camino.AddArc(X, Y + H - r, r, r, 90, 90);
        camino.CloseFigure();
        g.DrawPath(&p, &camino);
    };
    auto poligono = [&](const float* v, int n, bool rellenar) {
        std::vector<Gdiplus::PointF> pts;
        for (int k = 0; k < n; ++k) pts.push_back(Gdiplus::PointF(x + v[2 * k] * t, y + v[2 * k + 1] * t));
        if (rellenar) g.FillPolygon(&relleno, pts.data(), n);
        else g.DrawPolygon(&p, pts.data(), n);
    };

    switch (icono) {
        case ICONO_EXPORTAR:  // bandeja con flecha hacia arriba
            L(-0.38f, 0.10f, -0.38f, 0.40f); L(-0.38f, 0.40f, 0.38f, 0.40f); L(0.38f, 0.40f, 0.38f, 0.10f);
            L(0.0f, 0.18f, 0.0f, -0.40f); L(-0.18f, -0.22f, 0.0f, -0.40f); L(0.18f, -0.22f, 0.0f, -0.40f);
            break;
        case ICONO_TEMA: {  // circulo medio relleno: claro / oscuro
            C(0.0f, 0.0f, 0.38f);
            Gdiplus::GraphicsPath mitad;
            mitad.AddPie(x - 0.38f * t, y - 0.38f * t, 0.76f * t, 0.76f * t, 90, 180);
            g.FillPath(&relleno, &mitad);
            break;
        }
        case ICONO_AJUSTES:  // engranaje: anillo, dientes y eje
            C(0.0f, 0.0f, 0.26f);
            C(0.0f, 0.0f, 0.09f);
            for (int k = 0; k < 8; ++k) {
                float a = (float)(k * 3.14159265 / 4.0);
                L(0.28f * cosf(a), 0.28f * sinf(a), 0.42f * cosf(a), 0.42f * sinf(a));
            }
            break;
        case ICONO_MAS:
            L(-0.30f, 0.0f, 0.30f, 0.0f); L(0.0f, -0.30f, 0.0f, 0.30f);
            break;
        case ICONO_EDITAR: {  // lapiz en diagonal
            const float v[] = {-0.40f, 0.40f, -0.30f, 0.10f, 0.18f, -0.38f, 0.38f, -0.18f, -0.10f, 0.30f};
            poligono(v, 5, false);
            L(0.06f, -0.26f, 0.26f, -0.06f);
            break;
        }
        case ICONO_DUPLICAR:
            R(-0.16f, -0.40f, 0.56f, 0.62f, 0.10f);
            L(-0.40f, -0.16f, -0.40f, 0.38f); L(-0.40f, 0.38f, 0.12f, 0.38f);
            break;
        case ICONO_BORRAR:  // papelera
            L(-0.38f, -0.26f, 0.38f, -0.26f);
            L(-0.12f, -0.26f, -0.10f, -0.40f); L(-0.10f, -0.40f, 0.10f, -0.40f); L(0.10f, -0.40f, 0.12f, -0.26f);
            L(-0.28f, -0.26f, -0.22f, 0.40f); L(-0.22f, 0.40f, 0.22f, 0.40f); L(0.22f, 0.40f, 0.28f, -0.26f);
            L(-0.08f, -0.10f, -0.08f, 0.24f); L(0.08f, -0.10f, 0.08f, 0.24f);
            break;
        case ICONO_CARPETA:
            L(-0.42f, -0.30f, -0.12f, -0.30f); L(-0.12f, -0.30f, 0.0f, -0.18f); L(0.0f, -0.18f, 0.42f, -0.18f);
            L(0.42f, -0.18f, 0.42f, 0.34f); L(0.42f, 0.34f, -0.42f, 0.34f); L(-0.42f, 0.34f, -0.42f, -0.30f);
            break;
        case ICONO_LIBRO:  // libro abierto
            L(0.0f, -0.28f, 0.0f, 0.38f);
            L(0.0f, -0.28f, -0.42f, -0.36f); L(-0.42f, -0.36f, -0.42f, 0.30f); L(-0.42f, 0.30f, 0.0f, 0.38f);
            L(0.0f, -0.28f, 0.42f, -0.36f); L(0.42f, -0.36f, 0.42f, 0.30f); L(0.42f, 0.30f, 0.0f, 0.38f);
            break;
        case ICONO_MATRAZ: {  // matraz de laboratorio (practicas)
            const float v[] = {-0.12f, -0.40f, -0.12f, -0.08f, -0.38f, 0.34f, -0.34f, 0.42f,
                               0.34f, 0.42f, 0.38f, 0.34f, 0.12f, -0.08f, 0.12f, -0.40f};
            poligono(v, 8, false);
            L(-0.20f, -0.40f, 0.20f, -0.40f);
            L(-0.24f, 0.14f, 0.24f, 0.14f);
            break;
        }
        case ICONO_ESTRELLA: {
            float v[20];
            for (int k = 0; k < 10; ++k) {
                double a = k * 3.14159265 / 5 - 3.14159265 / 2;
                float r = (k % 2 == 0) ? 0.44f : 0.19f;
                v[2 * k] = (float)cos(a) * r;
                v[2 * k + 1] = (float)sin(a) * r + 0.03f;
            }
            poligono(v, 10, false);
            break;
        }
        case ICONO_BIRRETE: {  // gorro de graduacion (el Profesor)
            const float v[] = {0.0f, -0.34f, 0.46f, -0.12f, 0.0f, 0.10f, -0.46f, -0.12f};
            poligono(v, 4, false);
            L(-0.26f, -0.02f, -0.26f, 0.22f);
            g.DrawArc(&p, x - 0.26f * t, y + 0.02f * t, 0.52f * t, 0.36f * t, 0, 180);
            L(0.26f, -0.02f, 0.26f, 0.22f);
            L(0.40f, -0.10f, 0.40f, 0.26f);
            P(0.40f, 0.30f, 0.05f);
            break;
        }
        case ICONO_CALENDARIO:  // calendario con una marca
            R(-0.40f, -0.32f, 0.80f, 0.74f, 0.10f);
            L(-0.40f, -0.08f, 0.40f, -0.08f);
            L(-0.18f, -0.44f, -0.18f, -0.22f); L(0.18f, -0.44f, 0.18f, -0.22f);
            L(-0.14f, 0.16f, -0.03f, 0.27f); L(-0.03f, 0.27f, 0.17f, 0.05f);
            break;
        case ICONO_AYUDA:  // interrogacion en un circulo
            C(0.0f, 0.0f, 0.42f);
            g.DrawArc(&p, x - 0.14f * t, y - 0.26f * t, 0.28f * t, 0.26f * t, 180, 230);
            L(0.08f, -0.03f, 0.0f, 0.06f);
            L(0.0f, 0.06f, 0.0f, 0.10f);
            P(0.0f, 0.24f, 0.045f);
            break;
        case ICONO_AGENDA:  // lista con vinetas
            P(-0.30f, -0.26f, 0.06f); L(-0.12f, -0.26f, 0.38f, -0.26f);
            P(-0.30f, 0.0f, 0.06f); L(-0.12f, 0.0f, 0.38f, 0.0f);
            P(-0.30f, 0.26f, 0.06f); L(-0.12f, 0.26f, 0.24f, 0.26f);
            break;
        case ICONO_NOTAS:  // grafica de barras con tendencia
            L(-0.40f, 0.40f, 0.40f, 0.40f);
            L(-0.24f, 0.26f, -0.24f, 0.08f); L(0.0f, 0.26f, 0.0f, -0.06f); L(0.24f, 0.26f, 0.24f, -0.20f);
            L(-0.30f, -0.12f, -0.02f, -0.30f); L(-0.02f, -0.30f, 0.32f, -0.42f);
            break;
        case ICONO_FICHA:  // tarjeta de identidad de la asignatura
            R(-0.42f, -0.30f, 0.84f, 0.62f, 0.10f);
            C(-0.18f, -0.04f, 0.10f);
            L(-0.30f, 0.20f, -0.06f, 0.20f);
            L(0.06f, -0.10f, 0.30f, -0.10f); L(0.06f, 0.06f, 0.30f, 0.06f); L(0.06f, 0.20f, 0.22f, 0.20f);
            break;
        case ICONO_FLECHA:
            L(-0.34f, 0.0f, 0.32f, 0.0f); L(0.10f, -0.22f, 0.32f, 0.0f); L(0.10f, 0.22f, 0.32f, 0.0f);
            break;
        case ICONO_INFO:  // «i» en un circulo
            C(0.0f, 0.0f, 0.42f);
            P(0.0f, -0.20f, 0.055f);
            L(0.0f, -0.04f, 0.0f, 0.24f);
            break;
        case ICONO_AVISO: {  // triangulo con exclamacion
            const float v[] = {0.0f, -0.40f, 0.44f, 0.36f, -0.44f, 0.36f};
            poligono(v, 3, false);
            L(0.0f, -0.12f, 0.0f, 0.10f);
            P(0.0f, 0.23f, 0.05f);
            break;
        }
        case ICONO_TEXTO:  // «T» grande y «t» pequena: tipo y tamano de letra
            L(-0.42f, -0.30f, 0.06f, -0.30f); L(-0.18f, -0.30f, -0.18f, 0.36f);
            L(0.10f, 0.0f, 0.42f, 0.0f); L(0.26f, 0.0f, 0.26f, 0.36f);
            break;
        case ICONO_VENTANA:  // ventana con su barra de titulo
            R(-0.42f, -0.34f, 0.84f, 0.68f, 0.08f);
            L(-0.42f, -0.14f, 0.42f, -0.14f);
            P(-0.30f, -0.24f, 0.035f); P(-0.20f, -0.24f, 0.035f);
            break;
        case ICONO_ANTERIOR:
            L(0.10f, -0.26f, -0.12f, 0.0f); L(-0.12f, 0.0f, 0.10f, 0.26f);
            break;
        case ICONO_SIGUIENTE:
            L(-0.10f, -0.26f, 0.12f, 0.0f); L(0.12f, 0.0f, -0.10f, 0.26f);
            break;
        default:
            break;
    }
}

// Imagenes decorativas de assets/iconos, ya cargadas (sin imagen si no existen: no se vuelve a buscar).
// Lo ideal es un PNG con fondo transparente; si viene con fondo liso, la forma se saca de la luminosidad.
struct ImagenDecorativa {
    Gdiplus::Bitmap* imagen = NULL;
    int mascara = 0;  // 0 transparencia, 1 dibujo oscuro sobre fondo claro, 2 dibujo claro sobre fondo oscuro
};

static const ImagenDecorativa& imagenDecorativa(const std::wstring& nombre) {
    static std::map<std::wstring, ImagenDecorativa> cache;
    std::map<std::wstring, ImagenDecorativa>::iterator it = cache.find(nombre);
    if (it != cache.end()) return it->second;
    ImagenDecorativa& d = cache[nombre];
    std::wstring ruta = aW(rutas::unir(rutas::directorioDelEjecutable(), "assets/iconos/")) + nombre + L".png";
    if (GetFileAttributesW(ruta.c_str()) == INVALID_FILE_ATTRIBUTES) return d;
    d.imagen = Gdiplus::Bitmap::FromFile(ruta.c_str());
    if (!d.imagen || d.imagen->GetLastStatus() != Gdiplus::Ok || !d.imagen->GetWidth()) {
        delete d.imagen;
        d.imagen = NULL;
        return d;
    }
    Gdiplus::Color esquina;
    d.imagen->GetPixel(0, 0, &esquina);
    if (esquina.GetA() > 200) {
        int luz = esquina.GetR() * 299 + esquina.GetG() * 587 + esquina.GetB() * 114;
        d.mascara = luz > 127500 ? 1 : 2;
    }
    return d;
}

// La forma del icono ya reducida a dw x dh: blanca con la transparencia de la mascara. Reducir la
// imagen de 512 px en cada pintado es caro, asi que se hace una vez por nombre y tamaño.
static Gdiplus::Bitmap* formaEscalada(const std::wstring& nombre, const ImagenDecorativa& d, int dw, int dh) {
    static std::map<std::wstring, Gdiplus::Bitmap*> cache;
    std::wstring clave = nombre + L"|" + std::to_wstring(dw) + L"x" + std::to_wstring(dh);
    std::map<std::wstring, Gdiplus::Bitmap*>::iterator it = cache.find(clave);
    if (it != cache.end()) return it->second;
    if (cache.size() > 400) {  // muchos tamaños distintos (cambios de escala): se empieza de nuevo
        for (auto& par : cache) delete par.second;
        cache.clear();
    }
    Gdiplus::Bitmap* forma = new Gdiplus::Bitmap(dw, dh, PixelFormat32bppPARGB);
    Gdiplus::ColorMatrix matriz = {{{0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0},
                                    {0, 0, 0, 1, 0},
                                    {1, 1, 1, 0, 1}}};
    if (d.mascara) {  // alfa = luminosidad (o su inversa) en vez de la transparencia del fichero
        const float s = d.mascara == 1 ? -1.0f : 1.0f;
        matriz.m[0][3] = 0.299f * s;
        matriz.m[1][3] = 0.587f * s;
        matriz.m[2][3] = 0.114f * s;
        matriz.m[3][3] = 0;
        matriz.m[4][3] = d.mascara == 1 ? 1.0f : 0;
    }
    Gdiplus::ImageAttributes atributos;
    atributos.SetColorMatrix(&matriz);
    {
        Gdiplus::Graphics g(forma);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(d.imagen, Gdiplus::Rect(0, 0, dw, dh), 0, 0, (INT)d.imagen->GetWidth(), (INT)d.imagen->GetHeight(),
                    Gdiplus::UnitPixel, &atributos);
    }
    cache[clave] = forma;
    return forma;
}

bool hayDecoracion(const std::wstring& nombre) { return imagenDecorativa(nombre).imagen != NULL; }

void decoracion(HDC dc, const std::wstring& nombre, const RECT& r, COLORREF color, int alfa, Icono reserva) {
    const int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    const ImagenDecorativa& d = imagenDecorativa(nombre);
    if (!d.imagen) {
        dibujarIcono(dc, reserva, (r.left + r.right) / 2, (r.top + r.bottom) / 2, w < h ? w : h, color, alfa);
        return;
    }
    const double iw = d.imagen->GetWidth(), ih = d.imagen->GetHeight();
    const double k = w / iw < h / ih ? w / iw : h / ih;  // encajar sin deformar, centrada
    const int dw = (int)(iw * k), dh = (int)(ih * k);
    if (dw <= 0 || dh <= 0) return;
    // La forma hace de mascara: se pinta entera del color pedido.
    const float R = GetRValue(color) / 255.0f, G = GetGValue(color) / 255.0f, B = GetBValue(color) / 255.0f;
    const float A = alfa / 255.0f;
    Gdiplus::ColorMatrix matriz = {{{0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0},
                                    {0, 0, 0, 0, 0},
                                    {0, 0, 0, A, 0},
                                    {R, G, B, 0, 1}}};
    Gdiplus::ImageAttributes atributos;
    atributos.SetColorMatrix(&matriz);
    Gdiplus::Graphics g(dc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);  // ya tiene su tamaño: copia exacta
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.DrawImage(formaEscalada(nombre, d, dw, dh), Gdiplus::Rect(r.left + (w - dw) / 2, r.top + (h - dh) / 2, dw, dh), 0, 0, dw, dh,
                Gdiplus::UnitPixel, &atributos);
}

// ------------------------------------------------ botones con icono + ayuda

namespace {

HWND g_ayuda = NULL;  // control de tooltips compartido

void crearAyudaSiHaceFalta(HWND padre) {
    if (g_ayuda) return;
    g_ayuda = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
                              WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, padre, NULL,
                              GetModuleHandleW(NULL), NULL);
}


}  // namespace

void ponerAyuda(HWND control, const std::wstring& ayuda) {
    if (!control || ayuda.empty()) return;
    HWND padre = GetParent(control);
    crearAyudaSiHaceFalta(padre);
    if (!g_ayuda) return;
    SendMessageW(g_ayuda, TTM_SETMAXTIPWIDTH, 0, escalar(340));
    TTTOOLINFOW info;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = padre;
    info.uId = (UINT_PTR)control;
    info.lpszText = const_cast<LPWSTR>(ayuda.c_str());
    SendMessageW(g_ayuda, TTM_ADDTOOLW, 0, (LPARAM)&info);
}

HWND crearBotonIcono(HWND padre, Icono icono, int id, const std::wstring& ayuda) {
    HWND b = crearBoton(padre, L"", id, false);
    if (!b) return NULL;
    botonEnBarra(b);
    SetPropW(b, L"icono", (HANDLE)(INT_PTR)icono);
    ponerAyuda(b, ayuda);
    return b;
}

HWND crearBotonConIcono(HWND padre, Icono icono, const std::wstring& texto, int id, bool destacado,
                        const std::wstring& ayuda) {
    HWND b = crearBoton(padre, texto, id, destacado);
    if (!b) return NULL;
    SetPropW(b, L"icono", (HANDLE)(INT_PTR)icono);
    ponerAyuda(b, ayuda);
    return b;
}

void cambiarIcono(HWND boton, Icono icono) {
    if (!boton) return;
    SetPropW(boton, L"icono", (HANDLE)(INT_PTR)icono);
    InvalidateRect(boton, NULL, TRUE);
}

void dibujarBoton(const DRAWITEMSTRUCT* di) {
    if (!di || di->CtlType != ODT_BUTTON) return;

    const Tema& t = tema();
    const LONG_PTR estado = GetWindowLongPtrW(di->hwndItem, GWLP_USERDATA);
    const bool destacado = (estado & BOTON_DESTACADO) != 0;
    const bool encima = (estado & BOTON_ENCIMA) != 0;
    const bool enBarra = (estado & BOTON_EN_BARRA) != 0;
    const bool pulsado = (di->itemState & ODS_SELECTED) != 0;
    const bool desactivado = (di->itemState & ODS_DISABLED) != 0;
    const bool foco = (di->itemState & ODS_FOCUS) != 0;

    // 1. borrar el rectangulo completo con el fondo que hay detras: asi las
    //    esquinas que quedan fuera del redondeo no muestran restos.
    if (enBarra) {
        HBRUSH fondoBarra = CreateSolidBrush(colorBarraSuperior());
        FillRect(di->hDC, &di->rcItem, fondoBarra);
        DeleteObject(fondoBarra);
    } else {
        FillRect(di->hDC, &di->rcItem, brochaFondo());
    }

    wchar_t etiqueta[128] = {};
    GetWindowTextW(di->hwndItem, etiqueta, 127);
    const bool peligro = wcsstr(etiqueta, L"Borrar") != NULL;
    COLORREF base = enBarra ? mezclar(colorBarraSuperior(), RGB(255, 255, 255), 0.08) : t.panel;
    if (peligro) base = mezclar(t.panel, t.mal, 0.08);
    COLORREF fondo = destacado ? (peligro ? t.mal : t.acento) : base;
    COLORREF claro = RGB(255, 255, 255);
    if (!t.oscuro && !enBarra) claro = RGB(0, 0, 0);
    if (encima) fondo = mezclar(fondo, claro, 0.12);
    if (pulsado) fondo = mezclar(fondo, claro, 0.22);

    COLORREF texto = destacado ? RGB(255, 255, 255) : (enBarra ? RGB(236, 239, 245) : t.texto);
    // El acento es personalizable: un amarillo claro necesita texto oscuro.
    if (destacado && (GetRValue(fondo) * 299 + GetGValue(fondo) * 587 +
                      GetBValue(fondo) * 114) > 155000) texto = RGB(15, 25, 40);
    if (peligro) texto = destacado ? RGB(255, 255, 255) : t.mal;
    COLORREF borde = destacado ? mezclar(fondo, claro, 0.15)
                               : (enBarra ? mezclar(fondo, claro, 0.18) : t.borde);
    if (encima && !destacado) borde = t.acento;
    if (foco) borde = t.acento;
    if (desactivado && enBarra) {  // en la barra oscura: apagado pero sin destacar en blanco
        fondo = mezclar(colorBarraSuperior(), RGB(255, 255, 255), 0.05);
        texto = mezclar(colorBarraSuperior(), RGB(236, 239, 245), 0.45);
        borde = mezclar(colorBarraSuperior(), RGB(255, 255, 255), 0.12);
    } else if (desactivado) {
        fondo = mezclar(t.fondo, t.panelAlt, 0.4);
        texto = mezclar(t.textoSuave, t.fondo, 0.35);
        borde = t.borde;
    }

    HBRUSH brocha = CreateSolidBrush(fondo);
    HPEN lapiz = CreatePen(PS_SOLID, 1, borde);
    HGDIOBJ brochaVieja = SelectObject(di->hDC, brocha);
    HGDIOBJ lapizViejo = SelectObject(di->hDC, lapiz);
    RoundRect(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom,
              escalar(12), escalar(12));
    SelectObject(di->hDC, brochaVieja);
    SelectObject(di->hDC, lapizViejo);
    DeleteObject(brocha);
    DeleteObject(lapiz);

    if (foco && !desactivado) {
        RECT focoRect = di->rcItem;
        InflateRect(&focoRect, -4, -4);
        SetTextColor(di->hDC, texto);
        SetBkColor(di->hDC, fondo);
        DrawFocusRect(di->hDC, &focoRect);
    }

    // Icono y texto se centran juntos; sin texto, el icono va solo en el centro.
    Icono icono = (Icono)(INT_PTR)GetPropW(di->hwndItem, L"icono");
    HGDIOBJ fuenteVieja = SelectObject(di->hDC, destacado ? fuenteNegrita() : fuenteInterfaz());
    RECT r = di->rcItem;
    InflateRect(&r, -escalar(10), 0);
    if (icono != ICONO_NINGUNO) {
        const int tam = escalar(enBarra ? 20 : 17), hueco = escalar(8);
        const int cy = (di->rcItem.top + di->rcItem.bottom) / 2;
        if (!etiqueta[0]) {
            dibujarIcono(di->hDC, icono, (di->rcItem.left + di->rcItem.right) / 2, cy, tam, texto);
            SelectObject(di->hDC, fuenteVieja);
            return;
        }
        SIZE medida = {0, 0};
        GetTextExtentPoint32W(di->hDC, etiqueta, (int)wcslen(etiqueta), &medida);
        int total = tam + hueco + medida.cx;
        if (total > r.right - r.left) {  // en una ventana estrecha: el icono solo, mejor que «Du…»
            dibujarIcono(di->hDC, icono, (di->rcItem.left + di->rcItem.right) / 2, cy, tam, texto);
            SelectObject(di->hDC, fuenteVieja);
            return;
        }
        int x = (r.left + r.right - total) / 2;
        if (x < r.left) x = r.left;
        dibujarIcono(di->hDC, icono, x + tam / 2, cy, tam, texto);
        r.left = x + tam + hueco;
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, texto);
        DrawTextW(di->hDC, etiqueta, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(di->hDC, fuenteVieja);
        return;
    }

    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, texto);
    DrawTextW(di->hDC, etiqueta, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(di->hDC, fuenteVieja);
}

// ------------------------------------------------------------------ dibujo

void panelRedondeado(HDC dc, const RECT& r, COLORREF fondo, COLORREF borde, int radio) {
    HBRUSH brocha = CreateSolidBrush(fondo);
    HPEN lapiz = CreatePen(PS_SOLID, 1, borde);
    HGDIOBJ bv = SelectObject(dc, brocha);
    HGDIOBJ lv = SelectObject(dc, lapiz);
    radio = radioEstilo(radio);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radio, radio);
    SelectObject(dc, bv);
    SelectObject(dc, lv);
    DeleteObject(brocha);
    DeleteObject(lapiz);
}

void marcoAlrededorDe(HDC dc, HWND control, COLORREF borde, int radio, int margen) {
    if (!control) return;
    RECT r;
    GetWindowRect(control, &r);
    HWND padre = GetParent(control);
    MapWindowPoints(NULL, padre, (POINT*)&r, 2);
    InflateRect(&r, margen, margen);

    HPEN lapiz = CreatePen(PS_SOLID, 1, borde);
    HGDIOBJ bv = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ lv = SelectObject(dc, lapiz);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radio, radio);
    SelectObject(dc, bv);
    SelectObject(dc, lv);
    DeleteObject(lapiz);
}

void barraProgresoDibujada(HDC dc, const RECT& r, double porcentaje, COLORREF color,
                           COLORREF fondo) {
    if (porcentaje < 0) porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;
    int radio = (r.bottom - r.top);

    HBRUSH bFondo = CreateSolidBrush(fondo);
    HGDIOBJ bv = SelectObject(dc, bFondo);
    HGDIOBJ lv = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + 1, radio, radio);
    SelectObject(dc, bv);
    DeleteObject(bFondo);

    int ancho = (int)((r.right - r.left) * (porcentaje / 100.0));
    if (ancho > 2) {
        HBRUSH bColor = CreateSolidBrush(color);
        HGDIOBJ bv2 = SelectObject(dc, bColor);
        RoundRect(dc, r.left, r.top, r.left + ancho + 1, r.bottom + 1, radio, radio);
        SelectObject(dc, bv2);
        DeleteObject(bColor);
    }
    SelectObject(dc, lv);
}

void textoSeccion(HDC dc, const RECT& r, const std::wstring& texto) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, tema().textoSuave);
    HGDIOBJ fv = SelectObject(dc, fuenteNegrita());
    RECT rr = r;
    DrawTextW(dc, texto.c_str(), -1, &rr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, fv);
}

void tituloConIcono(HDC dc, const RECT& r, Icono icono, const std::wstring& texto, const std::wstring& pista,
                    COLORREF color, COLORREF colorPista) {
    const Tema& t = tema();
    const int lado = escalar(24);
    const int cy = (r.top + r.bottom) / 2;
    RECT cuadro = {r.left, cy - lado / 2, r.left + lado, cy + lado / 2};
    COLORREF fondo = mezclar(t.fondo, color, t.oscuro ? 0.22 : 0.14);
    panelRedondeado(dc, cuadro, fondo, fondo, escalar(8));
    dibujarIcono(dc, icono, cuadro.left + lado / 2, cy, escalar(15), color);

    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ fv = SelectObject(dc, fuenteNegrita());
    RECT rt = r;
    rt.left = cuadro.right + escalar(10);
    SIZE medida = {0, 0};
    GetTextExtentPoint32W(dc, texto.c_str(), (int)texto.size(), &medida);
    SetTextColor(dc, t.texto);
    DrawTextW(dc, texto.c_str(), -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (!pista.empty()) {
        RECT rp = r;
        rp.left = rt.left + medida.cx + escalar(16);
        SelectObject(dc, fuenteInterfaz());
        SIZE mp = {0, 0};
        GetTextExtentPoint32W(dc, pista.c_str(), (int)pista.size(), &mp);
        if (mp.cx <= rp.right - rp.left) {  // la pista entera o nada: cortada no se entiende
            SetTextColor(dc, colorPista != CLR_INVALID ? colorPista : mezclar(t.textoSuave, t.fondo, 0.25));
            DrawTextW(dc, pista.c_str(), -1, &rp, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    SelectObject(dc, fv);
}

int pastilla(HDC dc, int x, int y, int alto, const std::wstring& texto, COLORREF color) {
    HGDIOBJ fv = SelectObject(dc, fuenteNegrita());
    SIZE medida;
    ZeroMemory(&medida, sizeof(medida));
    GetTextExtentPoint32W(dc, texto.c_str(), (int)texto.size(), &medida);

    int ancho = medida.cx + 22;
    RECT r;
    r.left = x;
    r.top = y;
    r.right = x + ancho;
    r.bottom = y + alto;

    COLORREF fondo = mezclar(tema().panel, color, tema().oscuro ? 0.22 : 0.16);
    HBRUSH brocha = CreateSolidBrush(fondo);
    HPEN lapiz = CreatePen(PS_SOLID, 1, mezclar(fondo, color, 0.5));
    HGDIOBJ bv = SelectObject(dc, brocha);
    HGDIOBJ lv = SelectObject(dc, lapiz);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, alto, alto);
    SelectObject(dc, bv);
    SelectObject(dc, lv);
    DeleteObject(brocha);
    DeleteObject(lapiz);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, texto.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, fv);
    return ancho;
}

void alturaFilasLista(HWND lista, int alto) {
    if (!lista) return;
    // Truco clasico: una lista de imagenes de 1 pixel de ancho fija la altura de fila.
    HIMAGELIST imagenes = ImageList_Create(1, alto, ILC_COLOR32, 1, 1);
    if (imagenes) {
        HIMAGELIST anterior = (HIMAGELIST)SendMessageW(lista, LVM_SETIMAGELIST,
                                                     LVSIL_SMALL, (LPARAM)imagenes);
        if (anterior) ImageList_Destroy(anterior);
    }
}

// --- cabecera de las listas: se subclasea para poder pintarla con el tema ---

namespace {

WNDPROC g_procCabecera = NULL;

LRESULT CALLBACK procCabeceraLista(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    if (mensaje == WM_ERASEBKGND) return 1;
    if (mensaje == WM_PAINT || mensaje == WM_PRINTCLIENT) {
        PAINTSTRUCT ps;
        HDC dc = mensaje == WM_PAINT ? BeginPaint(h, &ps) : (HDC)wp;
        RECT cliente;
        GetClientRect(h, &cliente);

        const Tema& t = tema();
        HBRUSH fondo = CreateSolidBrush(mezclar(t.panel, t.fondo, 0.5));
        FillRect(dc, &cliente, fondo);
        DeleteObject(fondo);

        HGDIOBJ fuenteVieja = SelectObject(dc, fuenteInterfaz());
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, t.textoSuave);

        int n = (int)SendMessageW(h, HDM_GETITEMCOUNT, 0, 0);
        for (int i = 0; i < n; ++i) {
            RECT ri;
            if (!SendMessageW(h, HDM_GETITEMRECT, (WPARAM)i, (LPARAM)&ri)) continue;

            wchar_t buf[128];
            buf[0] = L'\0';
            HDITEMW hdi;
            ZeroMemory(&hdi, sizeof(hdi));
            hdi.mask = HDI_TEXT;
            hdi.pszText = buf;
            hdi.cchTextMax = 127;
            SendMessageW(h, HDM_GETITEMW, (WPARAM)i, (LPARAM)&hdi);

            RECT texto = ri;
            texto.left += 8;
            texto.right -= 4;
            DrawTextW(dc, buf, -1, &texto,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            HPEN lapiz = CreatePen(PS_SOLID, 1, t.borde);
            HGDIOBJ viejo = SelectObject(dc, lapiz);
            MoveToEx(dc, ri.left, ri.bottom - 1, NULL);
            LineTo(dc, ri.right, ri.bottom - 1);
            SelectObject(dc, viejo);
            DeleteObject(lapiz);
        }

        SelectObject(dc, fuenteVieja);
        if (mensaje == WM_PAINT) EndPaint(h, &ps);
        return 0;
    }
    return CallWindowProcW(g_procCabecera, h, mensaje, wp, lp);
}

}  // namespace

void tematizarLista(HWND lista) {
    if (!lista) return;
    const Tema& t = tema();
    SendMessageW(lista, LVM_SETBKCOLOR, 0, (LPARAM)t.panel);
    SendMessageW(lista, LVM_SETTEXTBKCOLOR, 0, (LPARAM)t.panel);
    SendMessageW(lista, LVM_SETTEXTCOLOR, 0, (LPARAM)t.texto);

    tematizarDesplazamiento(lista);  // barras de desplazamiento oscuras

    HWND cabecera = (HWND)SendMessageW(lista, LVM_GETHEADER, 0, 0);
    if (cabecera) {
        WNDPROC anterior =
            (WNDPROC)SetWindowLongPtrW(cabecera, GWLP_WNDPROC, (LONG_PTR)procCabeceraLista);
        if (anterior && anterior != procCabeceraLista) g_procCabecera = anterior;
        InvalidateRect(cabecera, NULL, TRUE);
    }
    InvalidateRect(lista, NULL, TRUE);
}

HWND crearLista(HWND padre, int id) {
    HWND lista = CreateWindowExW(0, L"SysListView32", L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                                     LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                                 0, 0, 10, 10, padre, (HMENU)(INT_PTR)id, NULL, NULL);
    if (lista) {
        SendMessageW(lista, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                     LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        tematizarLista(lista);
    }
    return lista;
}

void columna(HWND lista, int indice, const std::wstring& titulo, int ancho) {
    LVCOLUMNW col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.iSubItem = indice;
    col.cx = ancho;
    col.pszText = const_cast<LPWSTR>(titulo.c_str());
    SendMessageW(lista, LVM_INSERTCOLUMNW, (WPARAM)indice, (LPARAM)&col);
}

void limpiarLista(HWND lista) {
    SendMessageW(lista, LVM_DELETEALLITEMS, 0, 0);
}

int anadirFila(HWND lista, const std::wstring& primerTexto, LPARAM datos) {
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = (int)SendMessageW(lista, LVM_GETITEMCOUNT, 0, 0);
    item.iSubItem = 0;
    item.pszText = const_cast<LPWSTR>(primerTexto.c_str());
    item.lParam = datos;
    return (int)SendMessageW(lista, LVM_INSERTITEMW, 0, (LPARAM)&item);
}

void ponerTexto(HWND lista, int fila, int col, const std::wstring& texto) {
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = fila;
    item.iSubItem = col;
    item.pszText = const_cast<LPWSTR>(texto.c_str());
    SendMessageW(lista, LVM_SETITEMW, 0, (LPARAM)&item);
}

int filaSeleccionada(HWND lista) {
    return (int)SendMessageW(lista, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
}

void seleccionarFila(HWND lista, int fila) {
    if (fila < 0) return;
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_STATE;
    item.state = LVIS_SELECTED | LVIS_FOCUSED;
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(lista, LVM_SETITEMSTATE, (WPARAM)fila, (LPARAM)&item);
    SendMessageW(lista, LVM_ENSUREVISIBLE, (WPARAM)fila, FALSE);
}

LPARAM datosDeFila(HWND lista, int fila) {
    if (fila < 0) return -1;
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = fila;
    if (!SendMessageW(lista, LVM_GETITEMW, 0, (LPARAM)&item)) return -1;
    return item.lParam;
}

// ------------------------------------------- integracion con Windows (oscuro)

void modoOscuroDelSistema(bool oscuro) {
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    // Ordinal 135: SetPreferredAppMode(int) en Win10 1903+, AllowDarkModeForApp(bool) en 1809.
    typedef int(WINAPI * FnModo)(int);
    FnModo modo = (FnModo)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    if (modo) modo(oscuro ? 2 : 3);  // 2 = forzar oscuro, 3 = forzar claro
    FreeLibrary(ux);
}

void barraDeTituloOscura(HWND ventana) {
    if (!ventana) return;
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    typedef HRESULT(WINAPI * FnAtributo)(HWND, DWORD, LPCVOID, DWORD);
    FnAtributo poner = (FnAtributo)(void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (poner) {
        BOOL activado = tema().oscuro ? TRUE : FALSE;
        if (FAILED(poner(ventana, 20, &activado, sizeof(activado)))) {
            poner(ventana, 19, &activado, sizeof(activado));  // builds antiguos de Win10
        }
    }
    FreeLibrary(dwm);
}

void tematizarControlNativo(HWND control, const wchar_t* claseTema) {
    if (!control) return;
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;

    typedef bool(WINAPI * FnPermitir)(HWND, bool);
    FnPermitir permitir = (FnPermitir)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(133));
    if (permitir) permitir(control, tema().oscuro);

    typedef HRESULT(WINAPI * FnTema)(HWND, LPCWSTR, LPCWSTR);
    FnTema ponerTemaVentana = (FnTema)(void*)GetProcAddress(ux, "SetWindowTheme");
    if (ponerTemaVentana) ponerTemaVentana(control, claseTema, NULL);
    FreeLibrary(ux);
}

void tematizarDesplazamiento(HWND control) {
    tematizarControlNativo(control, tema().oscuro ? L"DarkMode_Explorer" : L"Explorer");
}

// ------------------------------------------------- cuadros de dialogo propios

namespace {

const wchar_t* kClaseCuadro = L"GestorAsignaturasCuadro";

struct EstadoCuadro {
    std::wstring texto;
    Icono icono = ICONO_INFO;
    COLORREF colorIcono = 0;
    int resultado = 0;         // 1 = aceptar/si, 0 = cancelar/no
    bool dosBotones = false;
};

const int ID_CUADRO_SI = 1;
const int ID_CUADRO_NO = 2;

LRESULT CALLBACK procCuadro(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    EstadoCuadro* e = reinterpret_cast<EstadoCuadro*>(GetWindowLongPtrW(h, GWLP_USERDATA));

    switch (mensaje) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT cliente;
            GetClientRect(h, &cliente);
            const Tema& t = tema();

            HBRUSH fondo = CreateSolidBrush(t.fondo);
            FillRect(dc, &cliente, fondo);
            DeleteObject(fondo);

            if (e) {
                const int lado = escalar(46);
                RECT ri = {24, 24, 24 + lado, 24 + lado};
                {
                    Gdiplus::Graphics g(dc);
                    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    COLORREF c = mezclar(t.fondo, e->colorIcono, t.oscuro ? 0.22 : 0.15);
                    Gdiplus::SolidBrush b(Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c)));
                    g.FillEllipse(&b, (float)ri.left, (float)ri.top, (float)lado, (float)lado);
                }
                dibujarIcono(dc, e->icono, ri.left + lado / 2, ri.top + lado / 2, escalar(26), e->colorIcono);

                std::wstring titulo = e->texto, resto;
                std::size_t corte = titulo.find(L'\n');
                if (corte != std::wstring::npos) {
                    resto = titulo.substr(corte + 1);
                    titulo = titulo.substr(0, corte);
                    while (!resto.empty() && (resto[0] == L'\n' || resto[0] == L'\r')) resto.erase(0, 1);
                }
                RECT rt = {24 + lado + 20, 26, cliente.right - 24, cliente.bottom - 76};
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, t.texto);
                HGDIOBJ fv = SelectObject(dc, fuenteNegrita());
                RECT medida = rt;
                DrawTextW(dc, titulo.c_str(), -1, &medida, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
                DrawTextW(dc, titulo.c_str(), -1, &rt, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
                if (!resto.empty()) {
                    rt.top = medida.bottom + escalar(8);
                    SetTextColor(dc, t.textoSuave);
                    SelectObject(dc, fuenteInterfaz());
                    DrawTextW(dc, resto.c_str(), -1, &rt, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
                }
                SelectObject(dc, fv);
            }
            EndPaint(h, &ps);
            return 0;
        }

        case WM_DRAWITEM:
            dibujarBoton((DRAWITEMSTRUCT*)lp);
            return TRUE;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == ID_CUADRO_SI || id == ID_CUADRO_NO || id == IDCANCEL) {
                if (e) e->resultado = (id == ID_CUADRO_SI) ? 1 : 0;
                DestroyWindow(h);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(h);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(h, mensaje, wp, lp);
}

void registrarClaseCuadro() {
    static bool hecho = false;
    if (hecho) return;
    hecho = true;
    WNDCLASSEXW clase;
    ZeroMemory(&clase, sizeof(clase));
    clase.cbSize = sizeof(clase);
    clase.lpfnWndProc = procCuadro;
    clase.hInstance = GetModuleHandleW(NULL);
    clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
    clase.lpszClassName = kClaseCuadro;
    RegisterClassExW(&clase);
}

// Bucle modal compartido por los cuadros propios.
void bucleModal(HWND ventana, HWND padre) {
    if (padre) EnableWindow(padre, FALSE);
    MSG msg;
    while (IsWindow(ventana)) {
        BOOL r = GetMessageW(&msg, NULL, 0, 0);
        if (r == 0) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (r == -1) break;
        if (!IsDialogMessageW(ventana, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (padre) {
        EnableWindow(padre, TRUE);
        SetActiveWindow(padre);
        SetForegroundWindow(padre);
    }
}

// Centra un rectangulo de tamano dado sobre el padre.
void centrarSobre(HWND padre, int ancho, int alto, int& x, int& y) {
    RECT r;
    x = CW_USEDEFAULT;
    y = CW_USEDEFAULT;
    if (padre && GetWindowRect(padre, &r)) {
        x = r.left + ((r.right - r.left) - ancho) / 2;
        y = r.top + ((r.bottom - r.top) - alto) / 3;
    }
}

int mostrarCuadro(HWND padre, const std::wstring& titulo, const std::wstring& texto,
                  Icono icono, COLORREF colorIcono, bool dosBotones,
                  const std::wstring& textoSi, const std::wstring& textoNo) {
    registrarClaseCuadro();

    const int anchoCliente = escalar(480);
    const int anchoTexto = anchoCliente - 24 - escalar(46) - 20 - 24;
    int altoTexto = 0;
    {
        HDC dc = GetDC(NULL);
        HGDIOBJ fv = SelectObject(dc, fuenteNegrita());
        RECT r = {0, 0, anchoTexto, 0};
        DrawTextW(dc, texto.c_str(), -1, &r, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        altoTexto = r.bottom + escalar(16);  // margen por la separacion entre titulo y resto
        SelectObject(dc, fv);
        ReleaseDC(NULL, dc);
    }
    if (altoTexto < escalar(56)) altoTexto = escalar(56);
    const int altoCliente = 26 + altoTexto + 24 + 52;

    RECT deseado;
    deseado.left = 0;
    deseado.top = 0;
    deseado.right = anchoCliente;
    deseado.bottom = altoCliente;
    DWORD estilo = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&deseado, estilo, FALSE);

    int x, y;
    centrarSobre(padre, deseado.right - deseado.left, deseado.bottom - deseado.top, x, y);

    HWND ventana = CreateWindowExW(WS_EX_DLGMODALFRAME, kClaseCuadro, titulo.c_str(), estilo, x, y,
                                   deseado.right - deseado.left, deseado.bottom - deseado.top,
                                   padre, NULL, GetModuleHandleW(NULL), NULL);
    if (!ventana) return 0;

    EstadoCuadro estado;
    estado.texto = texto;
    estado.icono = icono;
    estado.colorIcono = colorIcono;
    estado.dosBotones = dosBotones;
    SetWindowLongPtrW(ventana, GWLP_USERDATA, (LONG_PTR)&estado);

    int anchoBoton = escalar(120);
    int yBoton = altoCliente - 52;
    if (dosBotones) {
        crearBoton(ventana, textoSi, ID_CUADRO_SI, true);
        crearBoton(ventana, textoNo, ID_CUADRO_NO, false);
        MoveWindow(GetDlgItem(ventana, ID_CUADRO_SI), anchoCliente - 24 - anchoBoton * 2 - 8,
                   yBoton, anchoBoton, 34, TRUE);
        MoveWindow(GetDlgItem(ventana, ID_CUADRO_NO), anchoCliente - 24 - anchoBoton, yBoton,
                   anchoBoton, 34, TRUE);
    } else {
        crearBoton(ventana, textoSi, ID_CUADRO_SI, true);
        MoveWindow(GetDlgItem(ventana, ID_CUADRO_SI), anchoCliente - 24 - anchoBoton, yBoton,
                   anchoBoton, 34, TRUE);
    }

    aplicarFuente(ventana);
    barraDeTituloOscura(ventana);
    ShowWindow(ventana, SW_SHOW);
    UpdateWindow(ventana);
    // En las preguntas de borrar el foco va a Cancelar: un Enter despistado no borra nada.
    SetFocus(GetDlgItem(ventana, dosBotones && textoSi == L"Borrar" ? ID_CUADRO_NO : ID_CUADRO_SI));
    bucleModal(ventana, padre);
    return estado.resultado;
}

}  // namespace

void avisar(HWND padre, const std::wstring& texto) {
    mostrarCuadro(padre, L"Universalis", texto, ICONO_INFO, tema().acento, false, L"Vale", L"");
}

void avisarError(HWND padre, const std::wstring& texto) {
    mostrarCuadro(padre, L"Aviso", texto, ICONO_AVISO, tema().mal, false, L"Vale", L"");
}

bool preguntar(HWND padre, const std::wstring& texto) {
    // Las preguntas de borrar se ven en rojo y el boton dice lo que va a pasar.
    if (texto.compare(0, 7, L"¿Borrar") == 0)
        return mostrarCuadro(padre, L"Borrar", texto, ICONO_BORRAR, tema().mal, true, L"Borrar", L"Cancelar") == 1;
    return mostrarCuadro(padre, L"Confirmar", texto, ICONO_AYUDA, tema().medio, true, L"Sí", L"Cancelar") == 1;
}

// ------------------------------------------------------ selector de color

namespace {

const wchar_t* kClaseColores = L"GestorAsignaturasColores";

const COLORREF kMuestras[] = {
    RGB(91, 157, 255),  RGB(62, 207, 142),  RGB(242, 193, 78),  RGB(255, 107, 107),
    RGB(181, 123, 255), RGB(77, 208, 225),  RGB(255, 159, 104), RGB(156, 204, 101),
    RGB(41, 121, 255),  RGB(0, 176, 116),   RGB(255, 214, 0),   RGB(229, 57, 53),
    RGB(142, 68, 216),  RGB(0, 172, 193),   RGB(255, 112, 67),  RGB(124, 179, 66),
    RGB(140, 190, 255), RGB(129, 230, 190), RGB(250, 224, 150), RGB(255, 168, 168),
    RGB(213, 184, 255), RGB(160, 231, 240), RGB(255, 200, 170), RGB(205, 231, 165),
};

struct EstadoColores {
    COLORREF elegido = 0;
    int seleccion = -1;
    bool aceptado = false;
};

const int COLUMNAS = 6;
const int LADO = 46;
const int SEP = 10;
const int BORDE = 22;

RECT rectMuestra(int indice) {
    int fila = indice / COLUMNAS;
    int col = indice % COLUMNAS;
    RECT r;
    r.left = BORDE + col * (LADO + SEP);
    r.top = BORDE + fila * (LADO + SEP);
    r.right = r.left + LADO;
    r.bottom = r.top + LADO;
    return r;
}

LRESULT CALLBACK procColores(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    EstadoColores* e = reinterpret_cast<EstadoColores*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    const int total = (int)(sizeof(kMuestras) / sizeof(kMuestras[0]));

    switch (mensaje) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT cliente;
            GetClientRect(h, &cliente);
            const Tema& t = tema();

            HBRUSH fondo = CreateSolidBrush(t.fondo);
            FillRect(dc, &cliente, fondo);
            DeleteObject(fondo);

            for (int i = 0; i < total; ++i) {
                RECT r = rectMuestra(i);
                bool elegido = (e && e->seleccion == i);

                HBRUSH b = CreateSolidBrush(kMuestras[i]);
                HPEN p = CreatePen(PS_SOLID, elegido ? 3 : 1,
                                   elegido ? t.texto : mezclar(kMuestras[i], t.fondo, 0.4));
                HGDIOBJ bv = SelectObject(dc, b);
                HGDIOBJ pv = SelectObject(dc, p);
                RoundRect(dc, r.left, r.top, r.right, r.bottom, 12, 12);
                SelectObject(dc, bv);
                SelectObject(dc, pv);
                DeleteObject(b);
                DeleteObject(p);
            }
            EndPaint(h, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT p;
            p.x = (int)(short)LOWORD(lp);
            p.y = (int)(short)HIWORD(lp);
            for (int i = 0; i < total; ++i) {
                RECT r = rectMuestra(i);
                if (PtInRect(&r, p)) {
                    if (e) {
                        e->seleccion = i;
                        e->elegido = kMuestras[i];
                        e->aceptado = true;
                    }
                    DestroyWindow(h);
                    return 0;
                }
            }
            return 0;
        }

        case WM_DRAWITEM:
            dibujarBoton((DRAWITEMSTRUCT*)lp);
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wp) == ID_CUADRO_NO || LOWORD(wp) == IDCANCEL) {
                DestroyWindow(h);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(h);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(h, mensaje, wp, lp);
}

void registrarClaseColores() {
    static bool hecho = false;
    if (hecho) return;
    hecho = true;
    WNDCLASSEXW clase;
    ZeroMemory(&clase, sizeof(clase));
    clase.cbSize = sizeof(clase);
    clase.lpfnWndProc = procColores;
    clase.hInstance = GetModuleHandleW(NULL);
    clase.hCursor = LoadCursorW(NULL, IDC_HAND);
    clase.lpszClassName = kClaseColores;
    RegisterClassExW(&clase);
}

}  // namespace

bool elegirColor(HWND padre, COLORREF actual, COLORREF& elegido) {
    registrarClaseColores();
    const int total = (int)(sizeof(kMuestras) / sizeof(kMuestras[0]));
    const int filas = (total + COLUMNAS - 1) / COLUMNAS;

    const int anchoCliente = BORDE * 2 + COLUMNAS * LADO + (COLUMNAS - 1) * SEP;
    const int altoCliente = BORDE * 2 + filas * LADO + (filas - 1) * SEP + 52;

    RECT deseado;
    deseado.left = 0;
    deseado.top = 0;
    deseado.right = anchoCliente;
    deseado.bottom = altoCliente;
    DWORD estilo = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&deseado, estilo, FALSE);

    int x, y;
    centrarSobre(padre, deseado.right - deseado.left, deseado.bottom - deseado.top, x, y);

    HWND ventana = CreateWindowExW(WS_EX_DLGMODALFRAME, kClaseColores, L"Elegir color", estilo, x,
                                   y, deseado.right - deseado.left, deseado.bottom - deseado.top,
                                   padre, NULL, GetModuleHandleW(NULL), NULL);
    if (!ventana) return false;

    EstadoColores estado;
    estado.elegido = actual;
    for (int i = 0; i < total; ++i) {
        if (kMuestras[i] == actual) estado.seleccion = i;
    }
    SetWindowLongPtrW(ventana, GWLP_USERDATA, (LONG_PTR)&estado);

    crearBoton(ventana, L"Cancelar", ID_CUADRO_NO, false);
    MoveWindow(GetDlgItem(ventana, ID_CUADRO_NO), anchoCliente - BORDE - 108,
               altoCliente - 44, 108, 32, TRUE);

    aplicarFuente(ventana);
    barraDeTituloOscura(ventana);
    ShowWindow(ventana, SW_SHOW);
    UpdateWindow(ventana);
    bucleModal(ventana, padre);

    if (estado.aceptado) elegido = estado.elegido;
    return estado.aceptado;
}

// --------------------------------------------------------------------- shell

bool abrirEnSistema(HWND padre, const std::wstring& ruta) {
    if (ruta.empty()) return false;
    HINSTANCE r = ShellExecuteW(padre, L"open", ruta.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

// ---------------------------------------------------------------- formulario

namespace {

const wchar_t* kClaseFormulario = L"GestorAsignaturasFormulario";

struct EstadoFormulario {
    std::vector<Campo>* campos = NULL;
    std::vector<HWND> controles;
    std::vector<int> marcados;      // para las casillas dibujadas a mano
    std::vector<COLORREF> colores;  // para los campos de color
    bool aceptado = false;
    int scrollY = 0;
    std::wstring subtitulo;         // cabecera dibujada (vacia: sin cabecera)
    Icono icono = ICONO_NINGUNO;
    int altoCabecera = 0;
};

const int ID_ACEPTAR = 1;
const int ID_CANCELAR = 2;
const int ID_PRIMER_CAMPO = 100;
const int ID_PRIMER_AUXILIAR = 300;

bool textoAFecha(const std::wstring& texto, SYSTEMTIME& st) {
    int a = 0, m = 0, d = 0;
    if (swscanf(texto.c_str(), L"%d-%d-%d", &a, &m, &d) < 3) return false;
    ZeroMemory(&st, sizeof(st));
    st.wYear = (WORD)a;
    st.wMonth = (WORD)m;
    st.wDay = (WORD)d;
    return true;
}

std::wstring fechaATexto(const SYSTEMTIME& st) {
    wchar_t buf[32];
    _snwprintf(buf, 31, L"%04d-%02d-%02d", (int)st.wYear, (int)st.wMonth, (int)st.wDay);
    buf[31] = L'\0';
    return buf;
}

std::wstring colorATextoW(COLORREF c) {
    wchar_t buf[16];
    _snwprintf(buf, 15, L"#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    buf[15] = L'\0';
    return buf;
}

COLORREF textoAColor(const std::wstring& texto, COLORREF porDefecto) {
    unsigned long rgb = 0;
    if (!colorDesdeTexto(aUtf8(texto), rgb)) return porDefecto;
    return RGB((int)((rgb >> 16) & 0xFF), (int)((rgb >> 8) & 0xFF), (int)(rgb & 0xFF));
}

bool leerValores(HWND ventana, EstadoFormulario* estado) {
    std::vector<Campo>& campos = *estado->campos;
    for (size_t i = 0; i < campos.size(); ++i) {
        HWND control = estado->controles[i];
        Campo& campo = campos[i];

        if (campo.tipo == Campo::Accion) continue;
        if (campo.tipo == Campo::Casilla) {
            campo.valor = estado->marcados[i] ? L"1" : L"0";
            continue;
        }
        if (campo.tipo == Campo::Color) {
            campo.valor = colorATextoW(estado->colores[i]);
            continue;
        }
        if (campo.tipo == Campo::Fecha) {
            SYSTEMTIME st;
            ZeroMemory(&st, sizeof(st));
            if (SendMessageW(control, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                campo.valor = fechaATexto(st);
            }
            continue;
        }

        int largo = GetWindowTextLengthW(control);
        std::wstring texto((size_t)largo + 1, L'\0');
        GetWindowTextW(control, &texto[0], largo + 1);
        texto.resize((size_t)largo);
        campo.valor = texto;

        if (campo.tipo == Campo::Numero) {
            std::wstring limpio = texto;
            for (size_t k = 0; k < limpio.size(); ++k) {
                if (limpio[k] == L',') limpio[k] = L'.';
            }
            wchar_t* fin = NULL;
            double valor = wcstod(limpio.c_str(), &fin);
            bool valido = !limpio.empty() && fin && *fin == L'\0';
            if (!valido || valor < campo.minimo || valor > campo.maximo) {
                wchar_t aviso[256];
                _snwprintf(aviso, 255, L"El campo «%s» debe ser un número entre %g y %g.",
                           campo.etiqueta.c_str(), campo.minimo, campo.maximo);
                aviso[255] = L'\0';
                avisarError(ventana, aviso);
                SetFocus(control);
                return false;
            }
            campo.valor = limpio;
        }
    }
    return true;
}

void dibujarCasilla(const DRAWITEMSTRUCT* di, bool marcada, const std::wstring& etiqueta) {
    const Tema& t = tema();
    FillRect(di->hDC, &di->rcItem, brochaFondo());

    int lado = 16;
    RECT caja;
    caja.left = di->rcItem.left + 1;
    caja.top = di->rcItem.top + (di->rcItem.bottom - di->rcItem.top - lado) / 2;
    caja.right = caja.left + lado;
    caja.bottom = caja.top + lado;

    HBRUSH brocha = CreateSolidBrush(marcada ? t.acento : t.panel);
    HPEN lapiz = CreatePen(PS_SOLID, 1, marcada ? t.acento : t.borde);
    HGDIOBJ bv = SelectObject(di->hDC, brocha);
    HGDIOBJ lv = SelectObject(di->hDC, lapiz);
    RoundRect(di->hDC, caja.left, caja.top, caja.right, caja.bottom, 4, 4);

    if (marcada) {
        HPEN tick = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        SelectObject(di->hDC, tick);
        MoveToEx(di->hDC, caja.left + 4, caja.top + 8, NULL);
        LineTo(di->hDC, caja.left + 7, caja.top + 11);
        LineTo(di->hDC, caja.left + 12, caja.top + 5);
        SelectObject(di->hDC, lapiz);
        DeleteObject(tick);
    }

    SelectObject(di->hDC, bv);
    SelectObject(di->hDC, lv);
    DeleteObject(brocha);
    DeleteObject(lapiz);

    RECT texto = di->rcItem;
    texto.left = caja.right + 10;
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, t.texto);
    HGDIOBJ fv = SelectObject(di->hDC, fuenteInterfaz());
    DrawTextW(di->hDC, etiqueta.c_str(), -1, &texto, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(di->hDC, fv);
}

void dibujarMuestraColor(const DRAWITEMSTRUCT* di, COLORREF color) {
    const Tema& t = tema();
    HBRUSH brocha = CreateSolidBrush(color);
    HPEN lapiz = CreatePen(PS_SOLID, 1, t.borde);
    HGDIOBJ bv = SelectObject(di->hDC, brocha);
    HGDIOBJ lv = SelectObject(di->hDC, lapiz);
    RoundRect(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, 6, 6);
    SelectObject(di->hDC, bv);
    SelectObject(di->hDC, lv);
    DeleteObject(brocha);
    DeleteObject(lapiz);
}

LRESULT CALLBACK procFormulario(HWND ventana, UINT mensaje, WPARAM wp, LPARAM lp) {
    EstadoFormulario* estado =
        reinterpret_cast<EstadoFormulario*>(GetWindowLongPtrW(ventana, GWLP_USERDATA));

    switch (mensaje) {
        case WM_MOUSEWHEEL:
            SendMessageW(ventana, WM_VSCROLL,
                         GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            return 0;
        case WM_VSCROLL: {
            if (!estado) return 0;
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
            GetScrollInfo(ventana, SB_VERT, &si);
            int anterior = si.nPos;
            switch (LOWORD(wp)) {
                case SB_LINEUP: si.nPos -= escalar(44); break;
                case SB_LINEDOWN: si.nPos += escalar(44); break;
                case SB_PAGEUP: si.nPos -= si.nPage; break;
                case SB_PAGEDOWN: si.nPos += si.nPage; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: si.nPos = si.nTrackPos; break;
                default: return 0;
            }
            si.fMask = SIF_POS; SetScrollInfo(ventana, SB_VERT, &si, TRUE);
            estado->scrollY = GetScrollPos(ventana, SB_VERT);
            ScrollWindowEx(ventana, 0, anterior - estado->scrollY, NULL, NULL, NULL, NULL,
                           SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
            return 0;
        }
        case WM_ERASEBKGND: {
            RECT r;
            GetClientRect(ventana, &r);
            FillRect((HDC)wp, &r, brochaFondo());
            return 1;
        }

        case WM_CTLCOLORSTATIC: {
            bool suave = GetPropW((HWND)lp, L"suave") != NULL;
            SetTextColor((HDC)wp, suave ? mezclar(tema().textoSuave, tema().fondo, 0.15) : tema().texto);
            SetBkColor((HDC)wp, tema().fondo);
            return (LRESULT)brochaFondo();
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(ventana, &ps);
            if (estado && !estado->subtitulo.empty()) {
                // Cabecera: icono en un cuadrado de color y la explicacion del formulario
                const Tema& t = tema();
                const int y = 20 - estado->scrollY, lado = escalar(44);
                RECT franja;
                GetClientRect(ventana, &franja);
                franja.top = y - 20;
                franja.bottom = y - 20 + estado->altoCabecera - 10;
                HBRUSH b = CreateSolidBrush(mezclar(t.fondo, t.acento, t.oscuro ? 0.07 : 0.05));
                FillRect(dc, &franja, b);
                DeleteObject(b);
                RECT cuadro = {24, y, 24 + lado, y + lado};
                COLORREF fondo = mezclar(t.fondo, t.acento, t.oscuro ? 0.22 : 0.14);
                panelRedondeado(dc, cuadro, fondo, fondo, escalar(12));
                dibujarIcono(dc, estado->icono == ICONO_NINGUNO ? ICONO_EDITAR : estado->icono,
                             24 + lado / 2, y + lado / 2, escalar(24), t.acento);
                RECT r = {24 + lado + escalar(16), y, franja.right - 24, y + lado};
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, t.textoSuave);
                HGDIOBJ fv = SelectObject(dc, fuenteInterfaz());
                RECT medida = r;
                DrawTextW(dc, estado->subtitulo.c_str(), -1, &medida, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
                int alto = medida.bottom - medida.top;
                if (alto < lado) r.top += (lado - alto) / 2;
                DrawTextW(dc, estado->subtitulo.c_str(), -1, &r, DT_LEFT | DT_WORDBREAK);
                SelectObject(dc, fv);
                HPEN lapiz = CreatePen(PS_SOLID, 1, t.borde);
                HGDIOBJ lv = SelectObject(dc, lapiz);
                MoveToEx(dc, 0, franja.bottom, NULL);
                LineTo(dc, franja.right, franja.bottom);
                SelectObject(dc, lv);
                DeleteObject(lapiz);
            }
            EndPaint(ventana, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            SetTextColor((HDC)wp, tema().texto);
            SetBkColor((HDC)wp, tema().panel);
            return (LRESULT)brochaPanel();
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
            if (!estado || !di) break;
            int id = (int)di->CtlID;
            if (id >= ID_PRIMER_CAMPO && id < ID_PRIMER_AUXILIAR) {
                size_t i = (size_t)(id - ID_PRIMER_CAMPO);
                if (i < estado->campos->size()) {
                    const Campo& c = (*estado->campos)[i];
                    if (c.tipo == Campo::Casilla) {
                        dibujarCasilla(di, estado->marcados[i] != 0, c.etiqueta);
                        return TRUE;
                    }
                    if (c.tipo == Campo::Color) {
                        dibujarMuestraColor(di, estado->colores[i]);
                        return TRUE;
                    }
                }
            }
            dibujarBoton(di);
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == ID_ACEPTAR) {
                if (estado && leerValores(ventana, estado)) {
                    estado->aceptado = true;
                    DestroyWindow(ventana);
                }
                return 0;
            }
            if (id == ID_CANCELAR || id == IDCANCEL) {
                DestroyWindow(ventana);
                return 0;
            }
            if (estado && id >= ID_PRIMER_CAMPO && id < ID_PRIMER_AUXILIAR) {
                size_t i = (size_t)(id - ID_PRIMER_CAMPO);
                if (i < estado->campos->size()) {
                    const Campo& c = (*estado->campos)[i];
                    if (c.tipo == Campo::Casilla) {
                        estado->marcados[i] = !estado->marcados[i];
                        InvalidateRect(estado->controles[i], NULL, TRUE);
                        return 0;
                    }
                    if (c.tipo == Campo::Color) {
                        COLORREF nuevo = estado->colores[i];
                        if (elegirColor(ventana, estado->colores[i], nuevo)) {
                            estado->colores[i] = nuevo;
                            InvalidateRect(estado->controles[i], NULL, TRUE);
                        }
                        return 0;
                    }
                    if (c.tipo == Campo::Accion) {
                        if (c.accion) c.accion(ventana);
                        return 0;
                    }
                }
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(ventana);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(ventana, mensaje, wp, lp);
}

// Texto de ejemplo en gris dentro de un campo vacio. El de Windows (EM_SETCUEBANNER) sale casi negro
// en el tema claro y se confunde con un valor ya escrito.
LRESULT CALLBACK procCampoConPista(HWND h, UINT mensaje, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (mensaje == WM_NCDESTROY) RemoveWindowSubclass(h, procCampoConPista, 1);
    LRESULT r = DefSubclassProc(h, mensaje, wp, lp);
    if (mensaje == WM_PAINT && GetWindowTextLengthW(h) == 0) {
        const wchar_t* pista = (const wchar_t*)GetPropW(h, L"pista");
        if (pista && *pista) {
            HDC dc = GetDC(h);
            RECT c;
            SendMessageW(h, EM_GETRECT, 0, (LPARAM)&c);
            HGDIOBJ fv = SelectObject(dc, (HFONT)SendMessageW(h, WM_GETFONT, 0, 0));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, mezclar(tema().textoSuave, tema().panel, 0.35));
            DrawTextW(dc, pista, -1, &c, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, fv);
            ReleaseDC(h, dc);
        }
    }
    if ((mensaje == WM_SETFOCUS || mensaje == WM_KILLFOCUS || mensaje == WM_CHAR) && GetWindowTextLengthW(h) == 0)
        InvalidateRect(h, NULL, TRUE);
    return r;
}

void registrarClaseFormulario() {
    static bool hecho = false;
    if (hecho) return;
    hecho = true;

    WNDCLASSEXW clase;
    ZeroMemory(&clase, sizeof(clase));
    clase.cbSize = sizeof(clase);
    clase.lpfnWndProc = procFormulario;
    clase.hInstance = GetModuleHandleW(NULL);
    clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
    clase.hbrBackground = NULL;
    clase.lpszClassName = kClaseFormulario;
    RegisterClassExW(&clase);
}

}  // namespace

void pistaDelUltimo(std::vector<Campo>& campos, const std::wstring& pista, const std::wstring& ayuda) {
    if (campos.empty()) return;
    campos.back().pista = pista;
    campos.back().ayuda = ayuda;
}

bool formulario(HWND padre, const std::wstring& titulo, std::vector<Campo>& campos, const std::wstring& subtitulo,
                Icono icono) {
    registrarClaseFormulario();

    const int margen = 24;
    const int anchoEtiqueta = 280;
    const int anchoControl = 360;
    const int alto = escalar(30);
    const int separacion = escalar(44);
    const int anchoCliente = margen * 2 + anchoEtiqueta + anchoControl;
    const int altoAyuda = escalar(20);  // linea de ayuda bajo la etiqueta
    const int altoCabecera = subtitulo.empty() ? 74 : 20 + escalar(44) + 30;
    int altoCampos = 0;
    for (const Campo& c : campos) altoCampos += separacion + (c.ayuda.empty() ? 0 : altoAyuda);
    const int altoContenido = altoCabecera + altoCampos + alto + 42;
    RECT area = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    const int altoMaximo = area.bottom - area.top - 70;
    const int altoCliente = altoContenido < altoMaximo ? altoContenido : altoMaximo;

    RECT deseado;
    deseado.left = 0;
    deseado.top = 0;
    deseado.right = anchoCliente;
    deseado.bottom = altoCliente;
    DWORD estilo = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    if (altoContenido > altoCliente) estilo |= WS_VSCROLL;
    if (estilo & WS_VSCROLL) deseado.right += GetSystemMetrics(SM_CXVSCROLL);
    AdjustWindowRect(&deseado, estilo, FALSE);

    RECT rectPadre;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (padre && GetWindowRect(padre, &rectPadre)) {
        x = rectPadre.left +
            ((rectPadre.right - rectPadre.left) - (deseado.right - deseado.left)) / 2;
        y = rectPadre.top +
            ((rectPadre.bottom - rectPadre.top) - (deseado.bottom - deseado.top)) / 3;
    }
    if (y < area.top) y = area.top;
    if (y + deseado.bottom - deseado.top > area.bottom)
        y = area.bottom - (deseado.bottom - deseado.top);

    HWND ventana = CreateWindowExW(WS_EX_DLGMODALFRAME, kClaseFormulario, titulo.c_str(), estilo, x,
                                   y, deseado.right - deseado.left, deseado.bottom - deseado.top,
                                   padre, NULL, GetModuleHandleW(NULL), NULL);
    if (!ventana) return false;

    EstadoFormulario estado;
    estado.campos = &campos;
    estado.marcados.assign(campos.size(), 0);
    estado.colores.assign(campos.size(), tema().acento);
    estado.subtitulo = subtitulo;
    estado.icono = icono;
    estado.altoCabecera = altoCabecera;
    SetWindowLongPtrW(ventana, GWLP_USERDATA, (LONG_PTR)&estado);

    SCROLLINFO desplazamiento = {};
    desplazamiento.cbSize = sizeof(desplazamiento);
    desplazamiento.fMask = SIF_RANGE | SIF_PAGE;
    desplazamiento.nMax = altoContenido - 1;
    desplazamiento.nPage = altoCliente;
    SetScrollInfo(ventana, SB_VERT, &desplazamiento, TRUE);
    if (subtitulo.empty()) {
        HWND ayuda = crearEtiqueta(ventana, L"Revisa los campos y guarda tus cambios.");
        MoveWindow(ayuda, margen, 22, anchoCliente - 2 * margen, 34, TRUE);
    }
    int y0 = altoCabecera;
    for (size_t i = 0; i < campos.size(); ++i) {
        Campo& campo = campos[i];
        int idControl = ID_PRIMER_CAMPO + (int)i;
        HWND control = NULL;
        int xControl = margen + anchoEtiqueta;

        if (campo.tipo != Campo::Casilla && campo.tipo != Campo::Accion) {
            CreateWindowExW(0, L"STATIC", campo.etiqueta.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
                            margen, y0 + 3, anchoEtiqueta - 18, separacion - 5, ventana, NULL, NULL, NULL);
        }
        if (!campo.ayuda.empty()) {  // bajo el campo, alineada con el (en las casillas, bajo su texto)
            const bool casilla = campo.tipo == Campo::Casilla;
            HWND a = CreateWindowExW(0, L"STATIC", campo.ayuda.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                                     casilla ? margen + 26 : xControl, y0 + alto + 3,
                                     casilla ? anchoEtiqueta + anchoControl - 26 : anchoControl,
                                     altoAyuda, ventana, NULL, NULL, NULL);
            SetPropW(a, L"suave", (HANDLE)1);
        }

        switch (campo.tipo) {
            case Campo::Fecha: {
                control = CreateWindowExW(0, L"SysDateTimePick32", L"",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATEFORMAT,
                                          xControl, y0, anchoControl, alto, ventana,
                                          (HMENU)(INT_PTR)idControl, NULL, NULL);
                if (control) {
                    SendMessageW(control, DTM_SETFORMATW, 0, (LPARAM)L"dddd d 'de' MMMM 'de' yyyy");
                    SYSTEMTIME st;
                    if (textoAFecha(campo.valor, st)) {
                        SendMessageW(control, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
                    }
                    tematizarControlNativo(control, tema().oscuro ? L"DarkMode_CFD" : L"CFD");
                }
                break;
            }
            case Campo::Casilla: {
                estado.marcados[i] = (campo.valor == L"1") ? 1 : 0;
                control = CreateWindowExW(0, L"BUTTON", L"",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          margen, y0, anchoEtiqueta + anchoControl, alto, ventana,
                                          (HMENU)(INT_PTR)idControl, NULL, NULL);
                break;
            }
            case Campo::Accion: {
                control = CreateWindowExW(0, L"BUTTON", campo.etiqueta.c_str(),
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          margen, y0, anchoEtiqueta + anchoControl, alto, ventana,
                                          (HMENU)(INT_PTR)idControl, NULL, NULL);
                break;
            }
            case Campo::Color: {
                estado.colores[i] = textoAColor(campo.valor, tema().acento);
                control = CreateWindowExW(0, L"BUTTON", L"",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          xControl, y0, 90, alto, ventana,
                                          (HMENU)(INT_PTR)idControl, NULL, NULL);
                CreateWindowExW(0, L"STATIC", L"(pulsa para elegir)",
                                WS_CHILD | WS_VISIBLE | SS_LEFT, xControl + 100, y0 + 5,
                                anchoControl - 100, alto, ventana, NULL, NULL, NULL);
                break;
            }
            default: {
                control = CreateWindowExW(0, L"EDIT", campo.valor.c_str(),
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                              ES_AUTOHSCROLL,
                                          xControl, y0, anchoControl, alto, ventana,
                                          (HMENU)(INT_PTR)idControl, NULL, NULL);
                tematizarControlNativo(control, tema().oscuro ? L"DarkMode_CFD" : L"CFD");
                if (!campo.pista.empty()) {
                    SetPropW(control, L"pista", (HANDLE)campo.pista.c_str());
                    SetWindowSubclass(control, procCampoConPista, 1, 0);
                }
                break;
            }
        }

        estado.controles.push_back(control);
        y0 += separacion + (campo.ayuda.empty() ? 0 : altoAyuda);
    }

    int yBotones = y0 + 12;
    crearBoton(ventana, L"Guardar", ID_ACEPTAR, true);
    crearBoton(ventana, L"Cancelar", ID_CANCELAR, false);
    MoveWindow(GetDlgItem(ventana, ID_ACEPTAR), anchoCliente - margen - 214, yBotones, 100, 30,
               TRUE);
    MoveWindow(GetDlgItem(ventana, ID_CANCELAR), anchoCliente - margen - 104, yBotones, 100, 30,
               TRUE);

    aplicarFuente(ventana);
    barraDeTituloOscura(ventana);
    ShowWindow(ventana, SW_SHOW);
    UpdateWindow(ventana);
    if (!estado.controles.empty() && estado.controles[0]) SetFocus(estado.controles[0]);

    if (padre) EnableWindow(padre, FALSE);

    MSG msg;
    while (IsWindow(ventana)) {
        BOOL r = GetMessageW(&msg, NULL, 0, 0);
        if (r == 0) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (r == -1) break;
        if (!IsDialogMessageW(ventana, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (padre) {
        EnableWindow(padre, TRUE);
        SetActiveWindow(padre);
        SetForegroundWindow(padre);
    }
    return estado.aceptado;
}

}  // namespace gui
