// GuiAjustes.cpp - Ventana de Ajustes.
//
// Una lista de ajustes por secciones (Apariencia, Texto y tamano, Ventanas, Calendario y
// avisos, Estudio, Datos y ayuda) con filas al estilo de Windows 11: el nombre y una ayuda a
// la izquierda y el control a la derecha. Cada cambio se aplica al momento al estilo global
// (gui::Estilo y gui::Tema), asi que la propia ventana es la vista previa; al guardar, la
// ventana principal y el Profesor usan la nueva apariencia.
// Se dibuja con las piezas de la ventana del profesor (ProfesorUi.hpp).
#include "GuiAjustes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "ProfesorUi.hpp"

namespace ajustes {

namespace {

gui::Estilo estiloDe(const Ajustes& a) {
    gui::Estilo e;
    e.paleta = a.paleta;
    e.fuente = gui::aW(a.fuente);
    e.texto = a.tamTexto;
    e.esquinas = a.esquinas;
    e.fondo = a.fondo;
    e.cristal = a.cristal;
    e.animaciones = a.animaciones;
    e.barra = a.barra;
    e.contraste = a.contraste;
    e.anchura = a.anchura;
    e.pomodoro = a.pomodoro;
    e.descanso = a.descanso;
    e.musicaAlConcentrarse = a.musicaAlConcentrarse;
    e.campana = a.campana;
    e.respiracion = a.respiracion;
    e.objetivoDiario = a.objetivoDiario;
    e.efectosAmbiente = a.efectosAmbiente;
    return e;
}

COLORREF colorDe(const std::string& hex) {
    unsigned long rgb = 0;
    if (!colorDesdeTexto(hex, rgb)) return 0;
    return RGB((int)((rgb >> 16) & 0xFF), (int)((rgb >> 8) & 0xFF), (int)(rgb & 0xFF));
}

}  // namespace

bool temaOscuroEfectivo(const Curso& curso) {
    return curso.ajustes.temaSistema ? gui::sistemaEnOscuro() : curso.temaOscuro;
}

void aplicarApariencia(const Curso& curso) {
    gui::ponerEstilo(estiloDe(curso.ajustes));
    gui::ponerTema(temaOscuroEfectivo(curso), colorDe(curso.ajustes.colorAcento), curso.ajustes.escalaInterfaz);
}

namespace {

using namespace profesor::ui;

const wchar_t* kClase = L"UniversalisAjustes";
const int ID_TITULO = 5001;

enum Seccion { S_APARIENCIA, S_TEXTO, S_VENTANAS, S_CALENDARIO, S_ESTUDIO, S_DATOS, N_SECCIONES };
const wchar_t* SECCIONES[] = {L"Apariencia", L"Texto y tamaño", L"Ventanas", L"Calendario y avisos", L"Estudio",
                              L"Datos y ayuda"};

enum AccionAj {
    C_SECCION = 3000, C_MODO, C_PALETA, C_ACENTO, C_ACENTO_OTRO, C_BARRA, C_FONDO, C_ESQUINAS, C_CRISTAL, C_CONTRASTE,
    C_ESCALA, C_TEXTO, C_FUENTE, C_ANIMACIONES, C_ANCHURA, C_LUNES, C_FINDE, C_AVISO, C_OBJETIVO, C_ORDENAR,
    C_CONFIRMAR, C_POMODORO, C_DESCANSO, C_MUSICA, C_EXTRA, C_RESTABLECER, C_GUARDAR, C_CANCELAR, C_DEMO,
    C_CAMPANA, C_RESPIRACION, C_OBJETIVO_DIA, C_EFECTOS, C_ESTILO
};

// Estilos: combinaciones de serie de modo, paleta, acento, letra, esquinas, fondo, cristal y barra.
struct EstiloDeSerie {
    const wchar_t* nombre;
    const wchar_t* lema;
    bool oscuro;
    int paleta;
    const char* acento;
    const wchar_t* fuente;
    int esquinas, fondo;
    bool cristal;
    int barra;
};
const EstiloDeSerie ESTILOS[] = {
    {L"Universalis", L"El de siempre", true, 0, "#5b9dff", L"", 100, 2, true, 0},
    {L"Aurora", L"Violeta y brillos", true, 4, "#8b5cf6", L"", 160, 3, true, 3},
    {L"Medianoche", L"Azul profundo", true, 1, "#06b6d4", L"Bahnschrift", 100, 1, true, 3},
    {L"Bosque", L"Verde tranquilo", true, 3, "#22c55e", L"Corbel", 100, 2, true, 3},
    {L"Atardecer", L"Cálido y vivo", true, 5, "#f97316", L"", 160, 3, true, 2},
    {L"OLED", L"Negro puro, plano", true, 7, "#f59e0b", L"", 50, 0, false, 0},
    {L"Papel", L"Sepia para leer", false, 8, "#b45309", L"Georgia", 50, 0, false, 1},
    {L"Minimal", L"Blanco y sobrio", false, 7, "#64748b", L"Segoe UI", 50, 0, false, 0},
    {L"Océano", L"Claro y fresco", false, 2, "#0ea5e9", L"", 100, 2, true, 2},
    {L"Pizarra", L"Gris azulado", true, 9, "#6366f1", L"Segoe UI", 100, 1, false, 1},
};

const char* ACENTOS[] = {"#5b9dff", "#6366f1", "#8b5cf6", "#d946ef", "#ec4899", "#f43f5e",
                         "#ef4444", "#f97316", "#f59e0b", "#84cc16", "#22c55e", "#14b8a6", "#06b6d4", "#64748b"};
const wchar_t* FUENTES[] = {L"", L"Segoe UI", L"Bahnschrift", L"Calibri", L"Candara", L"Corbel", L"Verdana",
                            L"Tahoma", L"Trebuchet MS", L"Georgia", L"Cambria", L"Consolas"};
const int ESCALAS[] = {100, 110, 115, 125, 135, 150};
const int TEXTOS[] = {90, 100, 110, 120, 130};
const wchar_t* NOMBRES_TEXTO[] = {L"Pequeña", L"Normal", L"Grande", L"Muy grande", L"Enorme"};
const int ESQUINAS[] = {0, 50, 100, 160};
const wchar_t* NOMBRES_ESQUINAS[] = {L"Rectas", L"Suaves", L"Redondeadas", L"Muy redondeadas"};
const wchar_t* NOMBRES_FONDO[] = {L"Liso", L"Halos suaves", L"Halos", L"Halos intensos"};
const wchar_t* NOMBRES_BARRA[] = {L"Negra", L"Grafito", L"Color de acento", L"Color de la paleta"};
const wchar_t* NOMBRES_ANCHURA[] = {L"Normal", L"Amplio", L"Todo el ancho"};
const int AVISOS[] = {3, 5, 7, 10, 14, 21, 30};
const int OBJETIVOS[] = {5, 6, 7, 8, 9};
const int POMODOROS[] = {15, 20, 25, 30, 45, 50, 60};
const int DESCANSOS[] = {3, 5, 10, 15};
const int OBJETIVOS_DIA[] = {0, 30, 60, 90, 120, 180, 240};

template <typename T, std::size_t N>
int cuantos(const T (&)[N]) {
    return (int)N;
}

struct EstadoAj {
    Estado ui;
    Curso* curso = NULL;
    Curso trabajo;           // solo ajustes, titulo y tema: lo que se esta cambiando
    Curso original;          // para «Cancelar»
    std::vector<AccionExtra> acciones;
    bool guardado = false;
    HWND titulo = NULL;
    RECT rectTitulo = RECT();
    int seccionY[N_SECCIONES] = {};  // donde empieza cada seccion (sin desplazamiento)
    std::map<std::wstring, HFONT> muestras;  // cada tipo de letra en su propia letra
    std::vector<int> fuentesHay;            // indices de FUENTES instaladas
};

int altoBarra(Estado* s) { return E(s, 52); }
int anchoLado(Estado* s) { return E(s, 240); }
int altoPie(Estado* s) { return E(s, 76); }

int CALLBACK fuenteVista(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp) {
    *(bool*)lp = true;
    return 0;
}

bool existeFuente(const wchar_t* nombre) {
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, nombre, LF_FACESIZE - 1);
    bool hay = false;
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, fuenteVista, (LPARAM)&hay, 0);
    ReleaseDC(NULL, dc);
    return hay;
}

void borrarMuestras(EstadoAj* o) {
    for (auto& m : o->muestras) DeleteObject(m.second);
    o->muestras.clear();
}

HFONT muestra(EstadoAj* o, const std::wstring& cara) {
    auto it = o->muestras.find(cara);
    if (it != o->muestras.end()) return it->second;
    HFONT f = CreateFontW(-E(&o->ui, (int)std::lround(15 * o->trabajo.ajustes.tamTexto / 100.0)), 0, 0, 0, FW_NORMAL,
                          FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, cara.empty() ? L"Segoe UI" : cara.c_str());
    o->muestras[cara] = f;
    return f;
}

// Aplica lo que hay en `trabajo` a todo el programa y rehace fuentes y fondo de esta ventana.
void aplicar(EstadoAj* o) {
    aplicarApariencia(o->trabajo);
    Estado* s = &o->ui;
    borrarFuentes(s);
    crearFuentes(s);
    borrarMuestras(o);
    if (s->fondo) DeleteObject(s->fondo);
    s->fondo = NULL;
    if (o->titulo) SendMessageW(o->titulo, WM_SETFONT, (WPARAM)s->f.texto, TRUE);
    gui::barraDeTituloOscura(s->ventana);
    InvalidateRect(s->ventana, NULL, FALSE);
}

// ------------------------------------------------------------ piezas de la lista

// Fila de ajuste: tarjeta con el nombre y la ayuda a la izquierda. Devuelve el sitio del control.
RECT fila(EstadoAj* o, HDC dc, int x, int& y, int w, const std::wstring& nombre, const std::wstring& ayuda, int altoControl) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    bool estrecha = w < E(s, 760);
    int pad = E(s, 20);
    int anchoEt = estrecha ? w - 2 * pad : std::min(E(s, 300), w * 36 / 100);
    int altoEt = E(s, 24) + (ayuda.empty() ? 0 : altoTexto(dc, s->f.peque, ayuda, anchoEt) + E(s, 2));
    int alto = estrecha ? altoEt + altoControl + E(s, 12) + 2 * E(s, 16)
                        : std::max(altoEt, altoControl) + 2 * E(s, 16);
    RECT r = rect(x, y, w, alto);
    cristal(dc, r, E(s, 12));
    int ty = y + (estrecha ? E(s, 16) : (alto - altoEt) / 2);
    texto(dc, s->f.negrita, tm.texto, nombre, rect(x + pad, ty, anchoEt, E(s, 24)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (!ayuda.empty())
        texto(dc, s->f.peque, tm.textoSuave, ayuda, rect(x + pad, ty + E(s, 24), anchoEt, alto));
    y += alto + E(s, 8);
    if (estrecha) return rect(x + pad, ty + altoEt + E(s, 12), w - 2 * pad, altoControl);
    int cx = x + pad + anchoEt + E(s, 24);
    return rect(cx, r.top + (alto - altoControl) / 2, x + w - pad - cx, altoControl);
}

int anchoChip(EstadoAj* o, HDC dc, const std::wstring& t) { return anchoTexto(dc, o->ui.f.negrita, t) + E(&o->ui, 28); }

// Alto que ocupan unas pastillas repartidas en lineas dentro de `ancho`.
int altoChips(EstadoAj* o, HDC dc, const std::vector<std::wstring>& ops, int ancho) {
    Estado* s = &o->ui;
    int x = 0, lineas = 1;
    for (const std::wstring& t : ops) {
        int cw = anchoChip(o, dc, t);
        if (x > 0 && x + cw > ancho) {
            ++lineas;
            x = 0;
        }
        x += cw + E(s, 8);
    }
    return lineas * E(s, 34) + (lineas - 1) * E(s, 8);
}

void chips(EstadoAj* o, HDC dc, RECT r, const std::vector<std::wstring>& ops, int elegido, int accion) {
    Estado* s = &o->ui;
    int x = r.left, y = r.top;
    for (std::size_t i = 0; i < ops.size(); ++i) {
        int cw = anchoChip(o, dc, ops[i]);
        if (x > r.left && x + cw > r.right) {
            x = r.left;
            y += E(s, 42);
        }
        x += chip(s, dc, x, y, ops[i], (int)i == elegido, accion, (int)i) + E(s, 8);
    }
}

// Fila con pastillas: calcula su alto, pinta la fila y las pastillas.
void filaChips(EstadoAj* o, HDC dc, int x, int& y, int w, const std::wstring& nombre, const std::wstring& ayuda,
               const std::vector<std::wstring>& ops, int elegido, int accion) {
    Estado* s = &o->ui;
    bool estrecha = w < E(s, 760);
    int pad = E(s, 20);
    int anchoEt = estrecha ? w - 2 * pad : std::min(E(s, 300), w * 36 / 100);
    int anchoControl = estrecha ? w - 2 * pad : w - 2 * pad - anchoEt - E(s, 24);
    RECT c = fila(o, dc, x, y, w, nombre, ayuda, altoChips(o, dc, ops, anchoControl));
    chips(o, dc, c, ops, elegido, accion);
}

void filaInterruptor(EstadoAj* o, HDC dc, int x, int& y, int w, const std::wstring& nombre, const std::wstring& ayuda,
                     bool activo, int accion) {
    Estado* s = &o->ui;
    RECT c = fila(o, dc, x, y, w, nombre, ayuda, E(s, 34));
    interruptor(s, dc, c.left, c.top, activo, activo ? L"Sí" : L"No", accion);
}

int indiceDe(const int* lista, int n, int valor) {
    int mejor = 0;
    for (int i = 0; i < n; ++i)
        if (std::abs(lista[i] - valor) < std::abs(lista[mejor] - valor)) mejor = i;
    return mejor;
}

std::vector<std::wstring> numeros(const int* lista, int n, const wchar_t* sufijo) {
    std::vector<std::wstring> v;
    for (int i = 0; i < n; ++i) v.push_back(std::to_wstring(lista[i]) + sufijo);
    return v;
}

void cabeceraSeccion(EstadoAj* o, HDC dc, int x, int& y, int w, int sec, const std::wstring& sub) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    o->seccionY[sec] = y + s->scroll - (s->cuerpo.top + E(s, 28));
    texto(dc, s->f.titulo, tm.texto, SECCIONES[sec], rect(x, y, w, E(s, 38)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += E(s, 40);
    if (!sub.empty()) {
        int h = altoTexto(dc, s->f.texto, sub, w);
        texto(dc, s->f.texto, tm.textoSuave, sub, rect(x, y, w, h));
        y += h + E(s, 6);
    }
    y += E(s, 10);
}

// ----------------------------------------------------------------- secciones

// Muestra de como se ve la interfaz con lo elegido: una ventana en miniatura.
int vistaPrevia(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    int alto = E(s, 286);
    RECT caja = rect(x, y, w, alto);
    cristal(dc, caja, E(s, 16));
    // Barra superior
    RECT franja = rect(x + E(s, 1), y + E(s, 1), w - E(s, 2), E(s, 40));
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath p;
        RECT alta = franja;
        alta.bottom += E(s, 20);
        caminoRedondeado(p, alta, E(s, 15));
        g.SetClip(Gdiplus::Rect(franja.left, franja.top, franja.right - franja.left, franja.bottom - franja.top));
        Gdiplus::SolidBrush b(gc(gui::colorBarraSuperior()));
        g.FillPath(&b, &p);
    }
    redondeado(dc, rect(franja.left + E(s, 14), franja.top + E(s, 8), E(s, 24), E(s, 24)), E(s, 7), tm.acento);
    texto(dc, s->f.negrita, RGB(255, 255, 255), L"U", rect(franja.left + E(s, 14), franja.top + E(s, 8), E(s, 24), E(s, 24)),
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    texto(dc, s->f.negrita, gui::textoBarraSuperior(), L"Vista previa", rect(franja.left + E(s, 48), franja.top, E(s, 300), E(s, 40)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    // Lateral
    int lx = x + E(s, 16), ly = franja.bottom + E(s, 16), lw = std::min(E(s, 190), w / 4);
    const wchar_t* nav[] = {L"Inicio", L"Nuevo test", L"Resúmenes"};
    for (int i = 0; i < 3; ++i) {
        RECT r = rect(lx, ly + i * E(s, 40), lw, E(s, 34));
        if (i == 1) redondeado(dc, r, E(s, 9), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.22 : 0.12));
        if (i == 1) redondeado(dc, rect(r.left, r.top + E(s, 8), E(s, 3), E(s, 18)), E(s, 2), tm.acento);
        texto(dc, i == 1 ? s->f.negrita : s->f.texto, i == 1 ? tm.texto : tm.textoSuave, nav[i],
              rect(r.left + E(s, 14), r.top, lw - E(s, 20), r.bottom - r.top), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    // Contenido
    int cx = lx + lw + E(s, 20), cw = x + w - E(s, 18) - cx;
    subtituloTema(s, dc, rect(cx, ly, cw, E(s, 32)), L"Tema 2 · Aprendizaje supervisado", L"2 de 3", tm.acento);
    int py = ly + E(s, 42);
    RECT tarjeta = rect(cx, py, cw, E(s, 110));
    cristal(dc, tarjeta, E(s, 14));
    texto(dc, s->f.subtitulo, tm.texto, L"¿Qué mide la precisión de un modelo?", rect(cx + E(s, 16), py + E(s, 10), cw - E(s, 32), E(s, 26)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int qx = cx + E(s, 16);
    qx += chip(s, dc, qx, py + E(s, 46), L"Los aciertos", true, C_DEMO, 0) + E(s, 8);
    qx += chip(s, dc, qx, py + E(s, 46), L"El error", false, C_DEMO, 1) + E(s, 8);
    texto(dc, s->f.peque, tm.textoSuave, L"Tu dominio del tema", rect(cx + E(s, 16), py + E(s, 86), E(s, 160), E(s, 18)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    barra(dc, rect(cx + E(s, 170), py + E(s, 92), std::max(E(s, 40), cw - E(s, 260)), E(s, 7)), 0.68, tm.ok);
    texto(dc, s->f.peque, tm.ok, L"68 %", rect(cx + cw - E(s, 80), py + E(s, 84), E(s, 64), E(s, 22)), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    int bw = std::min(E(s, 190), cw / 2);
    boton(s, dc, rect(cx + cw - bw, tarjeta.bottom + E(s, 14), bw, E(s, 38)), L"Empezar test  →", C_DEMO, 2, PRIMARIO);
    boton(s, dc, rect(cx + cw - 2 * bw - E(s, 10), tarjeta.bottom + E(s, 14), bw, E(s, 38)), L"Repasar", C_DEMO, 3, SECUNDARIO);
    return alto;
}

// El estilo de serie que coincide con lo elegido (-1 si se ha retocado).
int estiloActual(EstadoAj* o) {
    const Ajustes& a = o->trabajo.ajustes;
    for (int i = 0; i < cuantos(ESTILOS); ++i) {
        const EstiloDeSerie& e = ESTILOS[i];
        if (!a.temaSistema && o->trabajo.temaOscuro == e.oscuro && a.paleta == e.paleta && colorDe(a.colorAcento) == colorDe(e.acento) &&
            a.esquinas == e.esquinas && a.fondo == e.fondo && a.cristal == e.cristal && a.barra == e.barra &&
            (gui::aW(a.fuente) == e.fuente || (!*e.fuente && a.fuente.empty())))
            return i;
    }
    return -1;
}

// Tarjetas de los estilos de serie: cada una es una ventanita en miniatura con sus colores.
void filaEstilos(EstadoAj* o, HDC dc, int x, int& y, int w) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    int n = cuantos(ESTILOS), mw = E(s, 150), mh = E(s, 86), hueco = E(s, 12), pie = E(s, 44);
    int porFila = std::max(1, (w - E(s, 40) + hueco) / (mw + hueco));
    int filas = (n + porFila - 1) / porFila;
    int alto = E(s, 64) + filas * (mh + pie) + (filas - 1) * hueco + E(s, 16);
    RECT caja = rect(x, y, w, alto);
    cristal(dc, caja, E(s, 12));
    glifo(s, dc, I_CHISPA, x + E(s, 31), y + E(s, 26), E(s, 20), tm.acento, L"estilos");
    texto(dc, s->f.negrita, tm.texto, L"Estilo", rect(x + E(s, 50), y + E(s, 14), w - E(s, 70), E(s, 24)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (gui::hayDecoracion(L"estilos"))  // marca de agua en la esquina
        gui::decoracion(dc, L"estilos", rect(x + w - E(s, 76), y + E(s, 6), E(s, 54), E(s, 54)), tm.acento, tm.oscuro ? 14 : 18,
                        gui::ICONO_NINGUNO);
    texto(dc, s->f.peque, tm.textoSuave, L"Un clic lo cambia todo a la vez. Después puedes retocar cada cosa más abajo.",
          rect(x + E(s, 20), y + E(s, 38), w - E(s, 40), E(s, 20)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int actual = estiloActual(o);
    int x0 = x + E(s, 20) + (w - E(s, 40) - (porFila * (mw + hueco) - hueco)) / 2;
    for (int i = 0; i < n; ++i) {
        const EstiloDeSerie& e = ESTILOS[i];
        RECT r = rect(x0 + (i % porFila) * (mw + hueco), y + E(s, 64) + (i / porFila) * (mh + pie + hueco), mw, mh);
        bool elegido = actual == i, h = encima(s, C_ESTILO, i);
        int radio = std::max(E(s, 3), E(s, 12) * e.esquinas / 100);
        COLORREF fondo = gui::muestraPaleta(e.paleta, e.oscuro), acento = colorDe(e.acento);
        COLORREF panel = e.oscuro ? mezcla(fondo, RGB(255, 255, 255), 0.07) : mezcla(fondo, RGB(255, 255, 255), 0.75);
        COLORREF tinta = e.oscuro ? RGB(230, 233, 240) : RGB(30, 34, 44);
        if (elegido) {
            RECT aro = r;
            InflateRect(&aro, E(s, 4), E(s, 4));
            redondeado(dc, aro, radio + E(s, 4), tm.panel, tm.acento, 2);
        }
        redondeado(dc, r, radio, fondo, h ? mezcla(tm.borde, tm.acento, 0.6) : tm.borde);
        // Halo del fondo
        if (e.fondo > 0) circulo(dc, r.right - E(s, 32), r.top + E(s, 40), E(s, 22), mezcla(fondo, acento, 0.08 * e.fondo));
        // Barra superior
        COLORREF barra = e.barra == 2 ? mezcla(RGB(8, 9, 12), acento, 0.36)
                         : e.barra == 3 ? mezcla(RGB(6, 7, 9), gui::muestraPaleta(e.paleta, true), 0.6)
                         : e.barra == 1 ? RGB(30, 32, 39) : (e.oscuro ? RGB(0, 0, 0) : RGB(20, 23, 31));
        {
            Gdiplus::Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath p;
            RECT alta = r;
            alta.bottom = r.top + E(s, 30);
            caminoRedondeado(p, alta, radio);
            g.SetClip(Gdiplus::Rect(r.left, r.top, r.right - r.left, E(s, 14)));
            Gdiplus::SolidBrush b(gc(barra));
            g.FillPath(&b, &p);
        }
        circulo(dc, r.left + E(s, 12), r.top + E(s, 7), E(s, 3), acento);
        // Panel con un titulo, una linea y un boton de acento
        RECT pr = rect(r.left + E(s, 10), r.top + E(s, 22), mw - E(s, 20), mh - E(s, 32));
        redondeado(dc, pr, std::max(E(s, 2), radio * 2 / 3), panel, e.cristal ? mezcla(panel, RGB(255, 255, 255), 0.18) : panel);
        if (e.cristal) redondeado(dc, rect(pr.left + E(s, 1), pr.top + E(s, 1), pr.right - pr.left - E(s, 2), E(s, 2)), E(s, 1),
                                  mezcla(panel, RGB(255, 255, 255), 0.22));
        HFONT f = muestra(o, e.fuente);
        texto(dc, f, tinta, L"Aa", rect(pr.left + E(s, 8), pr.top + E(s, 4), E(s, 40), E(s, 22)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        redondeado(dc, rect(pr.left + E(s, 8), pr.top + E(s, 30), (pr.right - pr.left) * 55 / 100, E(s, 5)), E(s, 2), mezcla(panel, tinta, 0.25));
        redondeado(dc, rect(pr.right - E(s, 46), pr.bottom - E(s, 20), E(s, 38), E(s, 13)), std::max(E(s, 2), E(s, 7) * e.esquinas / 100), acento);
        // Nombre y lema
        texto(dc, elegido ? s->f.negrita : s->f.texto, tm.texto, e.nombre, rect(r.left, r.bottom + E(s, 6), mw, E(s, 20)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.mini, tm.textoSuave, e.lema, rect(r.left, r.bottom + E(s, 25), mw, E(s, 16)), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        zona(s, rect(r.left, r.top, mw, mh + pie), C_ESTILO, i);
    }
    y += alto + E(s, 8);
}

int pintarApariencia(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    Ajustes& a = o->trabajo.ajustes;
    int y0 = y;
    cabeceraSeccion(o, dc, x, y, w, S_APARIENCIA, L"Los cambios se ven al momento en esta ventana. Al guardar se aplican a todo el "
                                                  L"programa: la ventana principal y el Profesor.");
    filaEstilos(o, dc, x, y, w);
    y += vistaPrevia(o, dc, x, y, w) + E(s, 14);

    filaChips(o, dc, x, y, w, L"Modo", L"Con «Como Windows» cambia solo cuando cambias el modo del sistema.",
              {L"Oscuro", L"Claro", L"Como Windows"}, a.temaSistema ? 2 : o->trabajo.temaOscuro ? 0 : 1, C_MODO);

    // Paletas: muestras con el fondo y un panel de cada una
    {
        bool oscuro = gui::tema().oscuro;
        int n = gui::numPaletas(), mw = E(s, 96), mh = E(s, 58), hueco = E(s, 10);
        bool estrecha = w < E(s, 760);
        int anchoControl = estrecha ? w - E(s, 40) : w - E(s, 40) - std::min(E(s, 300), w * 36 / 100) - E(s, 24);
        int porFila = std::max(1, (anchoControl + hueco) / (mw + hueco));
        int filas = (n + porFila - 1) / porFila;
        RECT c = fila(o, dc, x, y, w, L"Paleta", L"El tono del fondo, los paneles y los bordes.",
                      filas * (mh + E(s, 24)) + (filas - 1) * hueco);
        for (int i = 0; i < n; ++i) {
            RECT r = rect(c.left + (i % porFila) * (mw + hueco), c.top + (i / porFila) * (mh + E(s, 24) + hueco), mw, mh);
            bool elegida = a.paleta == i, h = encima(s, C_PALETA, i);
            COLORREF fondo = gui::muestraPaleta(i, oscuro);
            redondeado(dc, r, E(s, 10), fondo, elegida ? tm.acento : h ? mezcla(tm.borde, tm.acento, 0.5) : tm.borde, elegida ? 3 : 1);
            COLORREF panel = oscuro ? mezcla(fondo, RGB(255, 255, 255), 0.07) : mezcla(fondo, RGB(255, 255, 255), 0.7);
            redondeado(dc, rect(r.left + E(s, 10), r.top + E(s, 12), mw - E(s, 20), E(s, 12)), E(s, 4), panel);
            redondeado(dc, rect(r.left + E(s, 10), r.top + E(s, 30), (mw - E(s, 20)) * 6 / 10, E(s, 8)), E(s, 4), tm.acento);
            texto(dc, elegida ? s->f.negrita : s->f.peque, elegida ? tm.texto : tm.textoSuave, gui::nombrePaleta(i),
                  rect(r.left - E(s, 4), r.bottom + E(s, 2), mw + E(s, 8), E(s, 22)), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            zona(s, rect(r.left, r.top, mw, mh + E(s, 24)), C_PALETA, i);
        }
    }

    // Color de acento: circulos y «Otro…»
    {
        int n = cuantos(ACENTOS), d = E(s, 30), hueco = E(s, 10);
        bool estrecha = w < E(s, 760);
        int anchoControl = estrecha ? w - E(s, 40) : w - E(s, 40) - std::min(E(s, 300), w * 36 / 100) - E(s, 24);
        int porFila = std::max(1, (anchoControl + hueco) / (d + hueco));
        int total = n + 1;  // y el boton «Otro…» (cuenta como dos huecos)
        int filas = (total + 2 + porFila - 1) / porFila;
        RECT c = fila(o, dc, x, y, w, L"Color de acento", L"Botones, selecciones, barras de progreso y halos del fondo.",
                      filas * d + (filas - 1) * hueco);
        COLORREF actual = colorDe(a.colorAcento);
        bool alguno = false;
        int i = 0;
        for (; i < n; ++i) {
            COLORREF col = colorDe(ACENTOS[i]);
            int cx = c.left + (i % porFila) * (d + hueco) + d / 2, cy = c.top + (i / porFila) * (d + hueco) + d / 2;
            bool elegido = col == actual;
            alguno = alguno || elegido;
            if (elegido) circulo(dc, cx, cy, d / 2, tm.panel, tm.texto, 2.0f);
            circulo(dc, cx, cy, elegido ? d / 2 - E(s, 5) : encima(s, C_ACENTO, i) ? d / 2 : d / 2 - E(s, 2), col);
            zona(s, rect(cx - d / 2, cy - d / 2, d, d), C_ACENTO, i);
        }
        int ox = c.left + (i % porFila) * (d + hueco), oy = c.top + (i / porFila) * (d + hueco);
        if (ox + E(s, 96) > c.right) {
            ox = c.left;
            oy += d + hueco;
        }
        RECT otro = rect(ox, oy, E(s, 96), d);
        bool h = encima(s, C_ACENTO_OTRO);
        redondeado(dc, otro, d / 2, h ? tm.panelAlt : tm.panel, alguno ? tm.borde : tm.texto, alguno ? 1 : 2);
        if (!alguno) circulo(dc, otro.left + d / 2, oy + d / 2, d / 2 - E(s, 6), actual);
        texto(dc, s->f.peque, tm.texto, L"Otro…", rect(otro.left + (alguno ? 0 : d - E(s, 4)), oy, E(s, 96) - (alguno ? 0 : d - E(s, 4)), d),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        zona(s, otro, C_ACENTO_OTRO);
    }

    filaChips(o, dc, x, y, w, L"Barra superior", L"El color de la franja de arriba de cada ventana.",
              std::vector<std::wstring>(NOMBRES_BARRA, NOMBRES_BARRA + 4), a.barra, C_BARRA);
    filaChips(o, dc, x, y, w, L"Fondo", L"Las manchas de luz de color detrás de las ventanas grandes.",
              std::vector<std::wstring>(NOMBRES_FONDO, NOMBRES_FONDO + 4), a.fondo, C_FONDO);
    filaChips(o, dc, x, y, w, L"Esquinas", L"Cuánto se redondean tarjetas, botones y casillas.",
              std::vector<std::wstring>(NOMBRES_ESQUINAS, NOMBRES_ESQUINAS + 4), indiceDe(ESQUINAS, 4, a.esquinas), C_ESQUINAS);
    filaInterruptor(o, dc, x, y, w, L"Efecto cristal", L"Tarjetas con un brillo suave, algo de transparencia y sombra. Sin él, planas.",
                    a.cristal, C_CRISTAL);
    filaInterruptor(o, dc, x, y, w, L"Más contraste", L"Textos secundarios y bordes más marcados: se leen mejor con mucha luz.",
                    a.contraste, C_CONTRASTE);
    return y - y0 + E(s, 26);
}

int pintarTexto(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    Ajustes& a = o->trabajo.ajustes;
    int y0 = y;
    cabeceraSeccion(o, dc, x, y, w, S_TEXTO, L"");
    filaChips(o, dc, x, y, w, L"Tamaño de la interfaz", L"Todo más grande: textos, botones y márgenes.",
              numeros(ESCALAS, cuantos(ESCALAS), L" %"), indiceDe(ESCALAS, cuantos(ESCALAS), a.escalaInterfaz), C_ESCALA);
    filaChips(o, dc, x, y, w, L"Tamaño de la letra", L"Solo la letra, sin cambiar el resto.",
              std::vector<std::wstring>(NOMBRES_TEXTO, NOMBRES_TEXTO + 5), indiceDe(TEXTOS, 5, a.tamTexto), C_TEXTO);
    // Tipos de letra: cada uno escrito con su propia letra
    {
        std::vector<std::wstring> nombres;
        for (int i : o->fuentesHay) nombres.push_back(i == 0 ? L"La de Windows" : FUENTES[i]);
        bool estrecha = w < E(s, 760);
        int anchoControl = estrecha ? w - E(s, 40) : w - E(s, 40) - std::min(E(s, 300), w * 36 / 100) - E(s, 24);
        RECT c = fila(o, dc, x, y, w, L"Tipo de letra", L"Se usa en todas las ventanas.", altoChips(o, dc, nombres, anchoControl));
        int cx = c.left, cy = c.top;
        std::wstring actual = gui::aW(a.fuente);
        for (std::size_t k = 0; k < o->fuentesHay.size(); ++k) {
            int i = o->fuentesHay[k];
            HFONT f = muestra(o, FUENTES[i]);
            int cw = anchoTexto(dc, f, nombres[k]) + E(s, 30);
            if (cx > c.left && cx + cw > c.right) {
                cx = c.left;
                cy += E(s, 42);
            }
            RECT r = rect(cx, cy, cw, E(s, 34));
            bool elegido = actual == FUENTES[i], h = encima(s, C_FUENTE, i);
            redondeado(dc, r, E(s, 17), elegido ? mezcla(tm.panel, tm.acento, tm.oscuro ? 0.32 : 0.16) : h ? tm.panelAlt : tm.panel,
                       elegido ? tm.acento : h ? mezcla(tm.borde, tm.acento, 0.4) : tm.borde, elegido ? 2 : 1);
            texto(dc, f, elegido ? tm.texto : tm.textoSuave, nombres[k], r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            zona(s, r, C_FUENTE, i);
            cx += cw + E(s, 8);
        }
    }
    return y - y0 + E(s, 26);
}

int pintarVentanas(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    Ajustes& a = o->trabajo.ajustes;
    int y0 = y;
    cabeceraSeccion(o, dc, x, y, w, S_VENTANAS, L"");
    filaInterruptor(o, dc, x, y, w, L"Animaciones", L"Fundidos entre pantallas y animaciones de los tests en el Profesor.", a.animaciones, C_ANIMACIONES);
    filaChips(o, dc, x, y, w, L"Ancho del contenido", L"En pantallas grandes, cuánto se estira el contenido del Profesor.",
              std::vector<std::wstring>(NOMBRES_ANCHURA, NOMBRES_ANCHURA + 3), a.anchura, C_ANCHURA);
    return y - y0 + E(s, 26);
}

int pintarCalendario(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    Ajustes& a = o->trabajo.ajustes;
    int y0 = y;
    cabeceraSeccion(o, dc, x, y, w, S_CALENDARIO, L"");
    RECT c = fila(o, dc, x, y, w, L"Nombre del curso", L"Sale en la barra de arriba y en los informes.", E(s, 40));
    o->rectTitulo = c;
    redondeado(dc, c, E(s, 10), gui::tema().panel, GetFocus() == o->titulo ? gui::tema().acento : gui::tema().borde);
    filaInterruptor(o, dc, x, y, w, L"La semana empieza en lunes", L"Primera columna del calendario.", a.primerDiaLunes, C_LUNES);
    filaInterruptor(o, dc, x, y, w, L"Ver sábado y domingo", L"En el calendario de la ventana principal.", a.mostrarFinDeSemana, C_FINDE);
    filaChips(o, dc, x, y, w, L"Avisar de las pruebas", L"Se resaltan las que faltan menos de estos días.",
              numeros(AVISOS, cuantos(AVISOS), L" días"), indiceDe(AVISOS, cuantos(AVISOS), a.diasDeAviso), C_AVISO);
    filaChips(o, dc, x, y, w, L"Nota objetivo", L"La meta de la barra de cada asignatura.",
              numeros(OBJETIVOS, cuantos(OBJETIVOS), L""), indiceDe(OBJETIVOS, cuantos(OBJETIVOS), (int)std::lround(a.notaObjetivo)),
              C_OBJETIVO);
    filaInterruptor(o, dc, x, y, w, L"Ordenar asignaturas por día de clase", L"Si no, en el orden en que las añadiste.", a.ordenarPorDia,
                    C_ORDENAR);
    filaInterruptor(o, dc, x, y, w, L"Preguntar antes de borrar", L"Asignaturas y pruebas.", a.confirmarBorrado, C_CONFIRMAR);
    return y - y0 + E(s, 26);
}

int pintarEstudio(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    Ajustes& a = o->trabajo.ajustes;
    int y0 = y;
    cabeceraSeccion(o, dc, x, y, w, S_ESTUDIO, L"");
    filaChips(o, dc, x, y, w, L"Tiempo de concentración", L"Duración de cada bloque del temporizador del Profesor.",
              numeros(POMODOROS, cuantos(POMODOROS), L" min"), indiceDe(POMODOROS, cuantos(POMODOROS), a.pomodoro), C_POMODORO);
    filaChips(o, dc, x, y, w, L"Descanso", L"Pausa entre bloques.", numeros(DESCANSOS, cuantos(DESCANSOS), L" min"),
              indiceDe(DESCANSOS, cuantos(DESCANSOS), a.descanso), C_DESCANSO);
    filaInterruptor(o, dc, x, y, w, L"Música al concentrarme",
                    L"Al empezar el temporizador suena la última música que elegiste (lluvia, piano, tu carpeta…). "
                    L"En el Profesor: botón «Música» o Ctrl+M.",
                    a.musicaAlConcentrarse, C_MUSICA);
    filaInterruptor(o, dc, x, y, w, L"Efectos visuales de la música",
                    L"Mientras suena un ambiente, el fondo del Profesor lo acompaña: lluvia que cae, olas, brasas, hojas…",
                    a.efectosAmbiente, C_EFECTOS);
    filaInterruptor(o, dc, x, y, w, L"Campanilla", L"Una campana suave al acabar cada bloque y cada descanso, por encima de la música.",
                    a.campana, C_CAMPANA);
    filaInterruptor(o, dc, x, y, w, L"Pantalla de descanso",
                    L"Al acabar un bloque se abre un descanso con respiración guiada y consejos para despejarte.", a.respiracion,
                    C_RESPIRACION);
    std::vector<std::wstring> objetivos;
    for (int i = 0; i < cuantos(OBJETIVOS_DIA); ++i)
        objetivos.push_back(OBJETIVOS_DIA[i] ? (OBJETIVOS_DIA[i] % 60 ? std::to_wstring(OBJETIVOS_DIA[i]) + L" min"
                                                                     : std::to_wstring(OBJETIVOS_DIA[i] / 60) + L" h")
                                             : L"Sin objetivo");
    filaChips(o, dc, x, y, w, L"Objetivo diario", L"Minutos de concentración al día. Su anillo sale en el modo enfoque y en el descanso.",
              objetivos, indiceDe(OBJETIVOS_DIA, cuantos(OBJETIVOS_DIA), a.objetivoDiario), C_OBJETIVO_DIA);
    return y - y0 + E(s, 26);
}

int pintarDatos(EstadoAj* o, HDC dc, int x, int y, int w) {
    Estado* s = &o->ui;
    int y0 = y;
    cabeceraSeccion(o, dc, x, y, w, S_DATOS, L"");
    int bw = std::max(E(s, 200), (w - E(s, 20)) / 3), bx = x, by = y;
    for (std::size_t i = 0; i < o->acciones.size(); ++i) {
        if (bx > x && bx + bw > x + w) {
            bx = x;
            by += E(s, 52);
        }
        boton(s, dc, rect(bx, by, std::min(bw, x + w - bx), E(s, 44)), o->acciones[i].nombre, C_EXTRA, (int)i, SECUNDARIO);
        bx += bw + E(s, 10);
    }
    y = by + E(s, 60);
    boton(s, dc, rect(x, y, E(s, 260), E(s, 44)), L"Restablecer la apariencia", C_RESTABLECER, 0, PELIGRO);
    y += E(s, 60);
    return y - y0;
}

// --------------------------------------------------------------- ventana

void pintarBarra(EstadoAj* o, HDC dc, RECT b) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    const COLORREF fondo = gui::colorBarraSuperior(), tinta = gui::textoBarraSuperior();
    HBRUSH br = CreateSolidBrush(fondo);
    FillRect(dc, &b, br);
    DeleteObject(br);
    int h = b.bottom - b.top, x = b.left + E(s, 16);
    redondeado(dc, rect(x, (h - E(s, 28)) / 2, E(s, 28), E(s, 28)), E(s, 8), tm.acento);
    texto(dc, s->f.negrita, RGB(255, 255, 255), L"U", rect(x, (h - E(s, 28)) / 2, E(s, 28), E(s, 28)),
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    texto(dc, s->f.negrita, tinta, L"Ajustes y personalización", rect(x + E(s, 40), 0, E(s, 400), h), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT r = rect(b.right - E(s, 46), 0, E(s, 46), h);
    if (encima(s, A_CERRAR)) {
        HBRUSH hb = CreateSolidBrush(RGB(196, 43, 28));
        FillRect(dc, &r, hb);
        DeleteObject(hb);
    }
    iconoVentana(s, dc, r, A_CERRAR, tinta);
    zona(s, r, A_CERRAR);
}

int seccionVisible(EstadoAj* o) {
    int actual = 0;
    for (int i = 0; i < N_SECCIONES; ++i)
        if (o->seccionY[i] <= o->ui.scroll + E(&o->ui, 60)) actual = i;
    if (o->ui.scroll >= o->ui.altoContenido - o->ui.altoVisible - E(&o->ui, 4) && o->ui.altoContenido > o->ui.altoVisible)
        actual = N_SECCIONES - 1;
    return actual;
}

void pintarLado(EstadoAj* o, HDC dc, RECT lado) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    trozoDeFondo(s, dc, lado);
    velo(dc, lado, tm.oscuro ? RGB(0, 0, 0) : RGB(255, 255, 255), tm.oscuro ? 60 : 110);
    linea(dc, (float)lado.right - 0.5f, (float)lado.top, (float)lado.right - 0.5f, (float)lado.bottom, tm.borde, 1.0f);
    int y = lado.top + E(s, 24);
    int actual = seccionVisible(o);
    for (int i = 0; i < N_SECCIONES; ++i) {
        RECT r = rect(lado.left + E(s, 14), y, lado.right - lado.left - E(s, 28), E(s, 42));
        bool el = i == actual, h = encima(s, C_SECCION, i);
        if (el) {
            redondeado(dc, r, E(s, 10), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.22 : 0.12));
            redondeado(dc, rect(r.left, r.top + E(s, 11), E(s, 3), E(s, 20)), E(s, 2), tm.acento);
        } else if (h) {
            redondeado(dc, r, E(s, 10), tm.panelAlt);
        }
        static const gui::Icono ICONOS[] = {gui::ICONO_TEMA, gui::ICONO_TEXTO, gui::ICONO_VENTANA,
                                            gui::ICONO_CALENDARIO, gui::ICONO_BIRRETE, gui::ICONO_CARPETA};
        gui::dibujarIcono(dc, ICONOS[i], r.left + E(s, 28), r.top + E(s, 21), E(s, 18), el ? tm.acento : tm.textoSuave);
        texto(dc, el ? s->f.negrita : s->f.texto, el ? tm.texto : tm.textoSuave, SECCIONES[i],
              rect(r.left + E(s, 48), r.top, r.right - r.left - E(s, 52), E(s, 42)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        zona(s, r, C_SECCION, i);
        y += E(s, 46);
    }
}

void pintarPie(EstadoAj* o, HDC dc, RECT pie) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    trozoDeFondo(s, dc, pie);
    velo(dc, pie, tm.oscuro ? RGB(0, 0, 0) : RGB(255, 255, 255), tm.oscuro ? 70 : 120);
    linea(dc, (float)pie.left, (float)pie.top + 0.5f, (float)pie.right, (float)pie.top + 0.5f, tm.borde, 1.0f);
    int alto = E(s, 44), y = pie.top + (pie.bottom - pie.top - alto) / 2;
    boton(s, dc, rect(pie.right - E(s, 200) - E(s, 28), y, E(s, 200), alto), L"Guardar", C_GUARDAR, 0, PRIMARIO);
    boton(s, dc, rect(pie.right - E(s, 410) - E(s, 28), y, E(s, 200), alto), L"Cancelar", C_CANCELAR, 0, SECUNDARIO);
    texto(dc, s->f.peque, tm.textoSuave, L"Esc cancela  ·  Ctrl+S guarda", rect(pie.left + E(s, 28), y, E(s, 360), alto),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void colocarTitulo(EstadoAj* o) {
    RECT r = o->rectTitulo;
    InflateRect(&r, -E(&o->ui, 12), -E(&o->ui, 9));
    RECT visible;
    bool ver = IntersectRect(&visible, &r, &o->ui.cuerpo) && EqualRect(&visible, &r);
    if (ver) {
        RECT actual;
        GetWindowRect(o->titulo, &actual);
        MapWindowPoints(NULL, o->ui.ventana, (POINT*)&actual, 2);
        if (!EqualRect(&actual, &r)) MoveWindow(o->titulo, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
        if (!IsWindowVisible(o->titulo)) ShowWindow(o->titulo, SW_SHOW);
    } else if (IsWindowVisible(o->titulo)) {
        ShowWindow(o->titulo, SW_HIDE);
    }
}

void pintar(EstadoAj* o, HDC dc) {
    Estado* s = &o->ui;
    const gui::Tema& tm = gui::tema();
    RECT cliente;
    GetClientRect(s->ventana, &cliente);
    s->zonas.clear();
    prepararFondo(s, dc, cliente.right, cliente.bottom);
    trozoDeFondo(s, dc, cliente);
    RECT barra = {0, 0, cliente.right, altoBarra(s)};
    RECT lado = {0, barra.bottom, anchoLado(s), cliente.bottom - altoPie(s)};
    RECT pie = {0, cliente.bottom - altoPie(s), cliente.right, cliente.bottom};
    RECT cuerpo = {lado.right, barra.bottom, cliente.right, pie.top};
    s->cuerpo = cuerpo;
    int ancho = std::min((int)(cuerpo.right - cuerpo.left) - 2 * E(s, 36), E(s, 980));
    int x = cuerpo.left + ((cuerpo.right - cuerpo.left) - ancho) / 2;
    int y = cuerpo.top + E(s, 28) - s->scroll;
    s->altoVisible = cuerpo.bottom - cuerpo.top;
    o->rectTitulo = RECT();
    y += pintarApariencia(o, dc, x, y, ancho);
    y += pintarTexto(o, dc, x, y, ancho);
    y += pintarVentanas(o, dc, x, y, ancho);
    y += pintarCalendario(o, dc, x, y, ancho);
    y += pintarEstudio(o, dc, x, y, ancho);
    y += pintarDatos(o, dc, x, y, ancho);
    // Que la ultima seccion pueda subir hasta arriba
    s->altoContenido = std::max((int)(y + s->scroll - cuerpo.top) + E(s, 20), o->seccionY[S_DATOS] + s->altoVisible - E(s, 40));
    for (Zona& z : s->zonas) {
        RECT visible;
        if (!IntersectRect(&visible, &z.r, &cuerpo)) z.accion = A_NADA;
        else z.r = visible;
    }
    int maxScroll = std::max(0, s->altoContenido - s->altoVisible);
    if (s->scroll > maxScroll) {
        s->scroll = maxScroll;
        InvalidateRect(s->ventana, NULL, FALSE);
    }
    if (maxScroll > 0) {
        int pista = s->altoVisible - E(s, 16);
        int largo = std::max(E(s, 40), pista * s->altoVisible / s->altoContenido);
        int pos = (pista - largo) * s->scroll / std::max(1, maxScroll);
        redondeado(dc, rect(cuerpo.right - E(s, 10), cuerpo.top + E(s, 8) + pos, E(s, 5), largo), E(s, 3),
                   mezcla(tm.fondo, tm.textoSuave, 0.45));
    }
    pintarLado(o, dc, lado);
    pintarPie(o, dc, pie);
    pintarBarra(o, dc, barra);
    colocarTitulo(o);
}

std::wstring textoTitulo(EstadoAj* o) {
    int largo = GetWindowTextLengthW(o->titulo);
    std::vector<wchar_t> b((std::size_t)largo + 1);
    GetWindowTextW(o->titulo, b.data(), largo + 1);
    return b.data();
}

void cerrar(EstadoAj* o, bool guardar) {
    if (guardar) {
        std::wstring t = textoTitulo(o);
        if (!t.empty()) o->trabajo.titulo = gui::aUtf8(t);
        o->curso->titulo = o->trabajo.titulo;
        o->curso->temaOscuro = o->trabajo.temaOscuro;
        o->curso->ajustes = o->trabajo.ajustes;
        o->guardado = true;
    } else {
        aplicarApariencia(o->original);
    }
    DestroyWindow(o->ui.ventana);
}

void ejecutar(EstadoAj* o, int accion, int dato) {
    Estado* s = &o->ui;
    Ajustes& a = o->trabajo.ajustes;
    bool apariencia = true;  // casi todo cambia el aspecto
    switch (accion) {
        case A_CERRAR:
        case C_CANCELAR: cerrar(o, false); return;
        case C_GUARDAR: cerrar(o, true); return;
        case C_SECCION:
            s->scroll = std::max(0, std::min(o->seccionY[dato], s->altoContenido - s->altoVisible));
            apariencia = false;
            break;
        case C_MODO:
            a.temaSistema = dato == 2;
            if (dato < 2) o->trabajo.temaOscuro = dato == 0;
            break;
        case C_PALETA: a.paleta = dato; break;
        case C_ESTILO: {
            const EstiloDeSerie& e = ESTILOS[std::max(0, std::min(cuantos(ESTILOS) - 1, dato))];
            a.temaSistema = false;
            o->trabajo.temaOscuro = e.oscuro;
            a.paleta = e.paleta;
            a.colorAcento = e.acento;
            a.fuente = *e.fuente && existeFuente(e.fuente) ? gui::aUtf8(e.fuente) : "";
            a.esquinas = e.esquinas;
            a.fondo = e.fondo;
            a.cristal = e.cristal;
            a.barra = e.barra;
            break;
        }
        case C_ACENTO: a.colorAcento = ACENTOS[dato]; break;
        case C_ACENTO_OTRO: {
            COLORREF elegido = 0;
            if (!gui::elegirColor(s->ventana, colorDe(a.colorAcento), elegido)) return;
            a.colorAcento = colorATexto(((unsigned long)GetRValue(elegido) << 16) | ((unsigned long)GetGValue(elegido) << 8) |
                                        GetBValue(elegido));
            break;
        }
        case C_BARRA: a.barra = dato; break;
        case C_FONDO: a.fondo = dato; break;
        case C_ESQUINAS: a.esquinas = ESQUINAS[dato]; break;
        case C_CRISTAL: a.cristal = !a.cristal; break;
        case C_CONTRASTE: a.contraste = !a.contraste; break;
        case C_ESCALA: a.escalaInterfaz = ESCALAS[dato]; break;
        case C_TEXTO: a.tamTexto = TEXTOS[dato]; break;
        case C_FUENTE: a.fuente = gui::aUtf8(FUENTES[dato]); break;
        case C_ANIMACIONES: a.animaciones = !a.animaciones; break;
        case C_ANCHURA: a.anchura = dato; break;
        case C_LUNES: a.primerDiaLunes = !a.primerDiaLunes; apariencia = false; break;
        case C_FINDE: a.mostrarFinDeSemana = !a.mostrarFinDeSemana; apariencia = false; break;
        case C_AVISO: a.diasDeAviso = AVISOS[dato]; apariencia = false; break;
        case C_OBJETIVO: a.notaObjetivo = OBJETIVOS[dato]; apariencia = false; break;
        case C_ORDENAR: a.ordenarPorDia = !a.ordenarPorDia; apariencia = false; break;
        case C_CONFIRMAR: a.confirmarBorrado = !a.confirmarBorrado; apariencia = false; break;
        case C_POMODORO: a.pomodoro = POMODOROS[dato]; break;
        case C_DESCANSO: a.descanso = DESCANSOS[dato]; break;
        case C_MUSICA: a.musicaAlConcentrarse = !a.musicaAlConcentrarse; apariencia = false; break;
        case C_CAMPANA: a.campana = !a.campana; apariencia = false; break;
        case C_RESPIRACION: a.respiracion = !a.respiracion; apariencia = false; break;
        case C_OBJETIVO_DIA: a.objetivoDiario = OBJETIVOS_DIA[dato]; apariencia = false; break;
        case C_EFECTOS: a.efectosAmbiente = !a.efectosAmbiente; apariencia = false; break;
        case C_EXTRA:
            if (dato >= 0 && dato < (int)o->acciones.size() && o->acciones[(std::size_t)dato].hacer)
                o->acciones[(std::size_t)dato].hacer(s->ventana);
            apariencia = false;
            break;
        case C_RESTABLECER: {
            if (!gui::preguntar(s->ventana, L"¿Volver a la apariencia de serie? (Los ajustes del calendario no cambian.)")) return;
            Ajustes de;
            a.colorAcento = de.colorAcento;
            a.escalaInterfaz = de.escalaInterfaz;
            a.temaSistema = de.temaSistema;
            a.paleta = de.paleta;
            a.fuente = de.fuente;
            a.tamTexto = de.tamTexto;
            a.esquinas = de.esquinas;
            a.fondo = de.fondo;
            a.cristal = de.cristal;
            a.animaciones = de.animaciones;
            a.barra = de.barra;
            a.contraste = de.contraste;
            a.anchura = de.anchura;
            o->trabajo.temaOscuro = true;
            break;
        }
        default: return;
    }
    if (apariencia) aplicar(o);
    InvalidateRect(s->ventana, NULL, FALSE);
}

LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    EstadoAj* o = (EstadoAj*)GetWindowLongPtrW(w, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        o = (EstadoAj*)((CREATESTRUCTW*)lp)->lpCreateParams;
        o->ui.ventana = w;
        SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)o);
        crearFuentes(&o->ui);
    }
    if (!o) return DefWindowProcW(w, msg, wp, lp);
    Estado* s = &o->ui;
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) {
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
            if (r != HTCLIENT) return r;
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            ScreenToClient(w, &pt);
            if (!IsZoomed(w) && pt.y < E(s, 5)) return HTTOP;
            if (pt.y < altoBarra(s)) {
                for (const Zona& z : s->zonas)
                    if (z.accion != A_NADA && PtInRect(&z.r, pt)) return HTCLIENT;
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_CREATE:
            o->titulo = CreateWindowExW(0, L"EDIT", gui::aW(o->trabajo.titulo).c_str(), WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0,
                                        10, 10, w, (HMENU)(INT_PTR)ID_TITULO, GetModuleHandleW(NULL), NULL);
            SendMessageW(o->titulo, WM_SETFONT, (WPARAM)s->f.texto, TRUE);
            SendMessageW(o->titulo, EM_SETCUEBANNER, TRUE, (LPARAM)L"Mi curso");
            SetWindowPos(w, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)lp;
            mm->ptMinTrackSize.x = E(s, 760);
            mm->ptMinTrackSize.y = E(s, 560);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(w, NULL, FALSE);
            return 0;
        case WM_SETTINGCHANGE:  // el modo de Windows ha cambiado
            if (o->trabajo.ajustes.temaSistema && lp && lstrcmpiW((LPCWSTR)lp, L"ImmersiveColorSet") == 0) aplicar(o);
            break;
        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            int accion = A_NADA, dato = -1;
            for (auto it = s->zonas.rbegin(); it != s->zonas.rend(); ++it)
                if (it->accion != A_NADA && PtInRect(&it->r, pt)) {
                    accion = it->accion;
                    dato = it->dato;
                    break;
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
        case WM_MOUSELEAVE:
        case WM_NCMOUSEMOVE:
            if (s->hoverAccion != A_NADA) {
                s->hoverAccion = A_NADA;
                s->hoverDato = -1;
                InvalidateRect(w, NULL, FALSE);
            }
            if (msg == WM_NCMOUSEMOVE) break;
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT && s->hoverAccion != A_NADA && s->hoverAccion != A_CERRAR && s->hoverAccion != C_DEMO) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
            SetFocus(w);
            return 0;
        case WM_LBUTTONUP: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            for (auto it = s->zonas.rbegin(); it != s->zonas.rend(); ++it)
                if (it->accion != A_NADA && PtInRect(&it->r, pt)) {
                    ejecutar(o, it->accion, it->dato);
                    break;
                }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int maximo = std::max(0, s->altoContenido - s->altoVisible);
            s->scroll = std::max(0, std::min(maximo, s->scroll - GET_WHEEL_DELTA_WPARAM(wp) * E(s, 90) / WHEEL_DELTA));
            InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            int maximo = std::max(0, s->altoContenido - s->altoVisible);
            if (wp == VK_ESCAPE) cerrar(o, false);
            else if (wp == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) cerrar(o, true);
            else if (wp == VK_NEXT) s->scroll = std::min(maximo, s->scroll + s->altoVisible * 8 / 10);
            else if (wp == VK_PRIOR) s->scroll = std::max(0, s->scroll - s->altoVisible * 8 / 10);
            else if (wp == VK_DOWN) s->scroll = std::min(maximo, s->scroll + E(s, 60));
            else if (wp == VK_UP) s->scroll = std::max(0, s->scroll - E(s, 60));
            else if (wp == VK_HOME) s->scroll = 0;
            else if (wp == VK_END) s->scroll = maximo;
            if (IsWindow(w)) InvalidateRect(w, NULL, FALSE);
            return 0;
        }
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
                s->marco = CreateCompatibleBitmap(dc, std::max(1L, r.right), std::max(1L, r.bottom));
                s->marcoAncho = r.right;
                s->marcoAlto = r.bottom;
            }
            HDC mem = CreateCompatibleDC(dc);
            HGDIOBJ viejo = SelectObject(mem, s->marco);
            pintar(o, mem);
            BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, viejo);
            DeleteDC(mem);
            EndPaint(w, &ps);
            return 0;
        }
        case WM_CLOSE:
            cerrar(o, false);
            return 0;
        case WM_DESTROY:
            if (s->fondo) DeleteObject(s->fondo);
            if (s->marco) DeleteObject(s->marco);
            s->fondo = s->marco = NULL;
            borrarFuentes(s);
            borrarMuestras(o);
            return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

}  // namespace

bool abrir(HWND padre, Curso& curso, const std::vector<AccionExtra>& acciones) {
    Gdiplus::GdiplusStartupInput entrada;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &entrada, NULL);
    bool guardado = false;
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

        EstadoAj estado;
        estado.curso = &curso;
        estado.trabajo.titulo = curso.titulo;
        estado.trabajo.temaOscuro = curso.temaOscuro;
        estado.trabajo.ajustes = curso.ajustes;
        estado.original = estado.trabajo;
        estado.acciones = acciones;
        for (int i = 0; i < cuantos(FUENTES); ++i)
            if (i == 0 || existeFuente(FUENTES[i])) estado.fuentesHay.push_back(i);

        RECT trabajo;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &trabajo, 0);
        HDC pantalla = GetDC(NULL);
        double ppp = GetDeviceCaps(pantalla, LOGPIXELSY) / 96.0;
        ReleaseDC(NULL, pantalla);
        int ancho = std::min((int)(trabajo.right - trabajo.left) - 40, (int)(1240 * ppp));
        int alto = std::min((int)(trabajo.bottom - trabajo.top) - 40, (int)(900 * ppp));
        HWND w = CreateWindowExW(0, kClase, L"Ajustes", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                 trabajo.left + (trabajo.right - trabajo.left - ancho) / 2,
                                 trabajo.top + (trabajo.bottom - trabajo.top - alto) / 2, ancho, alto, padre, NULL, inst, &estado);
        if (w) {
            if (padre) EnableWindow(padre, FALSE);
            ShowWindow(w, SW_SHOW);
            MSG m;
            while (IsWindow(w) && GetMessageW(&m, NULL, 0, 0) > 0) {
                if (m.message == WM_KEYDOWN && m.hwnd == estado.titulo &&
                    (m.wParam == VK_ESCAPE || (m.wParam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)))) {
                    SendMessageW(w, WM_KEYDOWN, m.wParam, m.lParam);  // Esc y Ctrl+S tambien desde el campo de texto
                    continue;
                }
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
            if (padre) {
                EnableWindow(padre, TRUE);
                SetForegroundWindow(padre);
            }
        }
        guardado = estado.guardado;
    }
    Gdiplus::GdiplusShutdown(token);
    return guardado;
}

}  // namespace ajustes
