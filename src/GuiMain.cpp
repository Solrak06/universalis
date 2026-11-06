// GuiMain.cpp - Ventana principal del Gestor de Asignaturas (Win32).
//
// Tres columnas:  asignaturas | ficha de la asignatura | calendario
// Comparte todo el modelo con la version de consola.
#include "Gui.hpp"
#include "GuiAgenda.hpp"
#include "GuiAjustes.hpp"
#include "GuiProfesor.hpp"
#include <shellapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Almacen.hpp"
#include "ExportadorHtml.hpp"
#include "GuiCalendario.hpp"
#include "Json.hpp"
#include "Modelo.hpp"
#include "Rutas.hpp"

namespace {

const wchar_t* kClasePrincipal = L"GestorAsignaturasPrincipal";
const wchar_t* kClaseResumen = L"GestorAsignaturasResumen";

enum Controles {
    ID_LISTA_ASIG1 = 1001,
    ID_LISTA_ASIG2,
    ID_LISTA_EXAM,
    ID_CALENDARIO,
    ID_RESUMEN,
    ID_TITULO_APP,
    ID_DIA,
    ID_PIE_IZQ,
    ID_PIE_DER,

    ID_BTN_NUEVA_ASIG = 1100,
    ID_BTN_EDITAR_ASIG,
    ID_BTN_DUPLICAR_ASIG,
    ID_BTN_BORRAR_ASIG,
    ID_BTN_CARPETA,
    ID_BTN_TEMARIO,
    ID_BTN_PRACTICAS,
    ID_BTN_NUEVA_PRUEBA,
    ID_BTN_NOTA,
    ID_BTN_DUPLICAR_PRUEBA,
    ID_BTN_EDITAR_PRUEBA,
    ID_BTN_BORRAR_PRUEBA,
    ID_BTN_EXPORTAR,
    ID_BTN_TEMA,
    ID_BTN_AJUSTES,
    ID_BTN_PROFESOR,
    ID_BTN_GUIA,

    // acciones sin boton propio (atajos de teclado)
    ID_ACC_GUARDAR = 1200,
    ID_ACC_MARCAR_HECHA,
    ID_ACC_REFRESCAR
};

// Medidas base de la maquetacion (se escalan con los ajustes).
const int MARGEN_BASE = 24;
const int HUECO_BASE = 18;
const int ANCHO_IZQ_BASE = 360;
const int ANCHO_CAL_BASE = 420;
const int ALTO_BOTON_BASE = 38;
const int ALTO_BARRA_BASE = 96;
const int ALTO_PIE_BASE = 36;
const int ALTO_TITULO_BASE = 34;
const int ALTO_RESUMEN_BASE = 224;
const int ALTO_CAL_BASE = 350;
const int RADIO = 16;

int MARGEN() { return gui::escalar(MARGEN_BASE); }
int HUECO() { return gui::escalar(HUECO_BASE); }
int g_anchoLienzo = 0, g_altoLienzo = 0;
// La columna de las asignaturas crece un poco en pantallas anchas (hasta 60 px), para que no se corten los
// nombres, y cede 30 px en las estrechas
int ANCHO_IZQ() {
    int extra = std::max(-gui::escalar(30), std::min(gui::escalar(60), (g_anchoLienzo - gui::escalar(1600)) / 5));
    return gui::escalar(ANCHO_IZQ_BASE) + extra;
}
// En ventanas estrechas el calendario cede hasta 70 px a la columna central (la que tiene más botones)
int ANCHO_CAL() {
    int menos = std::max(0, std::min(gui::escalar(70), (gui::escalar(1700) - g_anchoLienzo) / 4));
    return gui::escalar(ANCHO_CAL_BASE) - menos;
}
int ALTO_BOTON() { return gui::escalar(ALTO_BOTON_BASE); }
int ALTO_BARRA() { return gui::escalar(ALTO_BARRA_BASE); }
// Botones de ventana propios (la barra de titulo de Windows se quita): 1 min, 2 max, 3 cerrar.
int ANCHO_BOTON_VENTANA() { return gui::escalar(46); }
int ANCHO_CONTROLES() { return 3 * ANCHO_BOTON_VENTANA(); }
int g_encimaVentana = 0;

RECT botonVentana(HWND ventana, int cual) {
    RECT c;
    GetClientRect(ventana, &c);
    int w = ANCHO_BOTON_VENTANA();
    RECT r = {c.right - (4 - cual) * w, 0, c.right - (3 - cual) * w, gui::escalar(34)};
    return r;
}

int botonVentanaEn(HWND ventana, POINT pt) {
    for (int i = 1; i <= 3; ++i) {
        RECT r = botonVentana(ventana, i);
        if (PtInRect(&r, pt)) return i;
    }
    return 0;
}

void pintarBotonesVentana(HWND ventana, HDC dc) {
    for (int i = 1; i <= 3; ++i) {
        RECT r = botonVentana(ventana, i);
        if (g_encimaVentana == i) {
            HBRUSH b = CreateSolidBrush(i == 3 ? RGB(196, 43, 28) : RGB(40, 42, 48));
            FillRect(dc, &r, b);
            DeleteObject(b);
        }
        int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, k = gui::escalar(5);
        HPEN lapiz = CreatePen(PS_SOLID, 1, RGB(235, 238, 244));
        HGDIOBJ viejo = SelectObject(dc, lapiz);
        HGDIOBJ vb = SelectObject(dc, GetStockObject(NULL_BRUSH));
        if (i == 1) {
            MoveToEx(dc, cx - k, cy, NULL);
            LineTo(dc, cx + k + 1, cy);
        } else if (i == 2) {
            if (IsZoomed(ventana)) {
                Rectangle(dc, cx - k, cy - k + 2, cx + k - 1, cy + k + 1);
                MoveToEx(dc, cx - k + 2, cy - k, NULL);
                LineTo(dc, cx + k + 1, cy - k);
                LineTo(dc, cx + k + 1, cy + k - 1);
            } else {
                Rectangle(dc, cx - k, cy - k, cx + k + 1, cy + k + 1);
            }
        } else {
            MoveToEx(dc, cx - k, cy - k, NULL);
            LineTo(dc, cx + k + 1, cy + k + 1);
            MoveToEx(dc, cx + k, cy - k, NULL);
            LineTo(dc, cx - k - 1, cy + k + 1);
        }
        SelectObject(dc, vb);
        SelectObject(dc, viejo);
        DeleteObject(lapiz);
    }
}
int ALTO_PIE() { return gui::escalar(ALTO_PIE_BASE); }
int ALTO_TITULO() { return gui::escalar(ALTO_TITULO_BASE); }
int ALTO_RESUMEN() { return gui::escalar(ALTO_RESUMEN_BASE); }
int ALTO_CAL() { return gui::escalar(ALTO_CAL_BASE); }

// ------------------------------------------------------------------- estado

Curso g_curso;
Almacen* g_almacen = NULL;

HWND g_principal = NULL;
HWND g_listaAsig1 = NULL;   // primer cuatrimestre
HWND g_listaAsig2 = NULL;   // segundo cuatrimestre
HWND g_listaExam = NULL;
HWND g_calendario = NULL;
HWND g_resumen = NULL;
HWND g_tituloApp = NULL;
HWND g_subtituloApp = NULL;
HWND g_profesor = NULL;
HWND g_guia = NULL;
HWND g_infoDia = NULL;
HWND g_pieIzq = NULL;
HWND g_pieDer = NULL;
std::vector<HWND> g_botones;
HACCEL g_atajos = NULL;
int g_scrollX = 0, g_scrollY = 0;

int g_indiceSel = -1;                    // asignatura seleccionada
std::vector<HWND> g_listaDeAsignatura;   // en que lista esta cada asignatura
std::vector<int> g_filaDeAsignatura;     // y en que fila

RECT g_rTitulo1, g_rTitulo2, g_rTituloExam, g_rTituloDia, g_rTituloFicha, g_rTituloCal, g_rMaterial;
int g_finDatosBarra = 0;  // hasta donde llegan los datos del curso en la barra superior

// ------------------------------------------------------------------- ayudas

HBRUSH brochaBarra() {
    static HBRUSH brocha = NULL;
    static COLORREF color = 0;
    COLORREF actual = gui::colorBarraSuperior();
    if (!brocha || color != actual) {
        if (brocha) DeleteObject(brocha);
        brocha = CreateSolidBrush(actual);
        color = actual;
    }
    return brocha;
}

// Fuente grande de la nota media; se rehace si cambia la escala.
HFONT fuenteEnorme() {
    static HFONT f = NULL;
    static int escalaCreada = -1;
    if (!f || escalaCreada != gui::escalaInterfaz()) {
        if (f) DeleteObject(f);
        f = NULL;
        LOGFONTW lf;
        ZeroMemory(&lf, sizeof(lf));
        if (GetObjectW(gui::fuenteInterfaz(), sizeof(lf), &lf)) {
            lf.lfWeight = FW_BOLD;
            lf.lfHeight = (LONG)(lf.lfHeight * 2.4);
            f = CreateFontIndirectW(&lf);
        }
        if (!f) f = gui::fuenteNegrita();
        escalaCreada = gui::escalaInterfaz();
    }
    return f;
}

std::wstring num(double valor, int decimales = 2) {
    return gui::aW(formatearNota(valor, decimales));
}

std::wstring textoCuentaAtras(long dias) {
    if (dias < 0) return L"";
    if (dias == 0) return L"hoy";
    if (dias == 1) return L"mañana";
    return L"en " + std::to_wstring(dias) + L" días";
}

Asignatura* asignaturaActual() {
    if (g_indiceSel < 0 || (std::size_t)g_indiceSel >= g_curso.asignaturas.size()) return NULL;
    return &g_curso.asignaturas[(std::size_t)g_indiceSel];
}

Examen* examenActual() {
    Asignatura* a = asignaturaActual();
    if (!a) return NULL;
    int fila = gui::filaSeleccionada(g_listaExam);
    if (fila < 0) return NULL;
    LPARAM indice = gui::datosDeFila(g_listaExam, fila);
    if (indice < 0 || (std::size_t)indice >= a->examenes.size()) return NULL;
    return &a->examenes[(std::size_t)indice];
}

void limpiarSeleccion(HWND lista) {
    if (!lista) return;
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_STATE;
    item.state = 0;
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(lista, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&item);
}

void guardar() {
    if (!g_almacen) return;
    if (!g_almacen->guardar(g_curso)) {
        gui::avisarError(g_principal, L"No se pudieron guardar los cambios:\n" +
                                          gui::aW(g_almacen->ultimoError()));
    }
}

bool confirmar(const std::wstring& texto) {
    if (!g_curso.ajustes.confirmarBorrado) return true;
    return gui::preguntar(g_principal, texto);
}

// ------------------------------------------- panel de la ficha (dibujado)

// Resumen de estudio que publica el profesor en datos/profesor/resumen.json (una entrada por
// asignatura). Texto vacio si aun no se ha estudiado con el profesor.
js::Valor leerResumenesDeEstudio() {
    std::ifstream f(rutas::unir(rutas::directorioDelEjecutable(), "datos/profesor/resumen.json").c_str(),
                    std::ios::binary);
    js::Valor todo;
    if (!f) return todo;
    std::string contenido((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string error;
    if (!js::Valor::parsear(contenido, todo, error)) return js::Valor();
    return todo;
}

std::wstring resumenDeEstudio(const Asignatura& a) {
    js::Valor todo = leerResumenesDeEstudio();
    if (!todo.esObjeto() || !todo.contiene(a.codigo)) return L"";
    const js::Valor& r = todo[a.codigo];
    std::wstring t;
    if (!r["precision"].esNulo()) t += std::to_wstring(r["precision"].comoEntero()) + L" % de aciertos";
    int tests = r["tests"].comoEntero();
    if (tests) t += (t.empty() ? L"" : L"  ·  ") + std::to_wstring(tests) + (tests == 1 ? L" test" : L" tests");
    int racha = r["racha"].comoEntero();
    if (racha) t += (t.empty() ? L"" : L"  ·  ") + std::wstring(L"racha ") + std::to_wstring(racha) + (racha == 1 ? L" día" : L" días");
    int tarjetas = r["tarjetas_pendientes"].comoEntero();
    if (tarjetas) t += (t.empty() ? L"" : L"  ·  ") + std::to_wstring(tarjetas) + (tarjetas == 1 ? L" tarjeta para hoy" : L" tarjetas para hoy");
    return t.empty() ? L"" : L"Estudio:  " + t;
}

void pintarResumen(HWND h, HDC destino) {
    RECT cliente;
    GetClientRect(h, &cliente);

    HDC dc = CreateCompatibleDC(destino);
    HBITMAP lienzo = CreateCompatibleBitmap(destino, cliente.right, cliente.bottom);
    HGDIOBJ lienzoViejo = SelectObject(dc, lienzo);

    const gui::Tema& t = gui::tema();
    FillRect(dc, &cliente, gui::brochaFondo());
    gui::panelRedondeado(dc, cliente, t.panel, t.borde, RADIO);
    SetBkMode(dc, TRANSPARENT);

    Asignatura* a = asignaturaActual();
    if (!a) {
        // Primer contacto: que es esto y por donde se empieza.
        const auto s = gui::escalar;
        const int lado = s(84);
        RECT dibujo = {s(28), s(30), s(28) + lado, s(30) + lado};
        RECT halo = dibujo;
        InflateRect(&halo, s(6), s(6));
        gui::panelRedondeado(dc, halo, gui::mezclar(t.panel, t.acento, t.oscuro ? 0.14 : 0.08),
                             gui::mezclar(t.panel, t.acento, 0.25), s(22));
        InflateRect(&dibujo, -s(12), -s(12));
        gui::decoracion(dc, L"bienvenida", dibujo, t.acento, 255, gui::ICONO_BIRRETE);

        int x = s(28) + lado + s(26);
        RECT r = {x, s(30), cliente.right - s(24), s(64)};
        HGDIOBJ fv = SelectObject(dc, gui::fuenteTitulo());
        SetTextColor(dc, t.texto);
        DrawTextW(dc, g_curso.asignaturas.empty() ? L"Tu curso empieza aquí" : L"Elige una asignatura", -1, &r,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, gui::fuenteInterfaz());
        SetTextColor(dc, t.textoSuave);
        const wchar_t* pasos[] = {
            L"Crea tus asignaturas con «Nueva asignatura» (abajo a la izquierda).",
            L"Añade sus pruebas con «Nueva prueba»: fecha y cuánto pesan en la nota.",
            L"Cuando sepas una nota, «Poner nota»: la media se calcula sola."};
        if (!g_curso.asignaturas.empty())
            pasos[0] = L"Pulsa una asignatura de la lista de la izquierda para ver aquí su ficha.";
        int y = s(74);
        for (int i = 0; i < 3; ++i) {
            RECT numero = {x, y + s(2), x + s(22), y + s(24)};
            gui::panelRedondeado(dc, numero, gui::mezclar(t.panel, t.acento, 0.2), gui::mezclar(t.panel, t.acento, 0.2), s(22));
            SetTextColor(dc, t.acento);
            SelectObject(dc, gui::fuenteNegrita());
            wchar_t n[4] = {(wchar_t)(L'1' + i), 0};
            DrawTextW(dc, n, -1, &numero, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RECT linea = {x + s(32), y, cliente.right - s(24), y + s(26)};
            SelectObject(dc, gui::fuenteInterfaz());
            SetTextColor(dc, t.textoSuave);
            DrawTextW(dc, pasos[i], -1, &linea, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += s(34);
        }
        RECT pie = {x, cliente.bottom - s(40), cliente.right - s(24), cliente.bottom - s(14)};
        SetTextColor(dc, gui::mezclar(t.textoSuave, t.panel, 0.3));
        DrawTextW(dc, L"¿Perdido? El botón ? de arriba a la derecha (o F1) abre la guía rápida.", -1, &pie,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, fv);
        BitBlt(destino, 0, 0, cliente.right, cliente.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, lienzoViejo);
        DeleteObject(lienzo);
        DeleteDC(dc);
        return;
    }

    const std::size_t indice = (std::size_t)g_indiceSel;
    const COLORREF color = gui::colorDeAsignatura(a->color, indice);
    const int pad = gui::escalar(22);
    const int anchoMedia = gui::escalar(150);
    const auto s = gui::escalar;
    int y = pad;

    // --- cuadro con el dibujo de la asignatura, en su color (como las fichas del profesor)
    const int lado = s(56);
    {
        RECT cuadro = {pad, y + s(2), pad + lado, y + s(2) + lado};
        gui::panelRedondeado(dc, cuadro, gui::mezclar(t.panel, color, t.oscuro ? 0.20 : 0.14),
                             gui::mezclar(t.panel, color, t.oscuro ? 0.34 : 0.26), s(16));
        InflateRect(&cuadro, -s(13), -s(13));
        gui::decoracion(dc, L"asignatura", cuadro, color, 255, gui::ICONO_LIBRO);
    }

    RECT r;
    r.left = pad + lado + s(16);
    r.right = cliente.right - anchoMedia;
    r.top = y;
    r.bottom = y + s(22);
    SetTextColor(dc, color);
    HGDIOBJ fv = SelectObject(dc, gui::fuenteNegrita());
    DrawTextW(dc, gui::aW(a->codigo).c_str(), -1, &r,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // --- nombre en el color de la asignatura
    r.top = y + s(24);
    r.bottom = r.top + s(34);
    SetTextColor(dc, t.texto);
    SelectObject(dc, gui::fuenteTitulo());
    DrawTextW(dc, gui::aW(a->nombre).c_str(), -1, &r,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // --- meta
    std::wstring meta = (a->cuatri == 2) ? L"2.º cuatrimestre" : L"1er cuatrimestre";
    if (!a->cuatrimestre.empty()) meta = gui::aW(a->cuatrimestre) + L"   ·   " + meta;
    if (a->creditos > 0) meta += L"   ·   " + std::to_wstring(a->creditos) + L" ECTS";
    if (!a->diasClase.empty()) meta += L"   ·   clase: " + gui::aW(diasClaseLegibles(*a));
    if (!a->profesor.empty()) meta += L"   ·   " + gui::aW(a->profesor);

    // La linea de datos va debajo de la media grande, asi que puede ocupar todo el ancho
    r.top = y + s(66);
    r.bottom = r.top + s(22);
    r.left = pad;
    r.right = cliente.right - pad;
    SetTextColor(dc, t.textoSuave);
    SelectObject(dc, gui::fuenteInterfaz());
    DrawTextW(dc, meta.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // --- media grande a la derecha
    RECT rm;
    rm.left = cliente.right - anchoMedia;
    rm.right = cliente.right - pad;
    rm.top = y;
    rm.bottom = y + s(20);
    SetTextColor(dc, t.textoSuave);
    DrawTextW(dc, L"Media actual", -1, &rm, DT_RIGHT | DT_SINGLELINE);
    rm.top = y + s(16);
    rm.bottom = y + s(62);
    SelectObject(dc, fuenteEnorme());
    if (a->tieneNotas()) {
        SetTextColor(dc, gui::colorDeNota(a->notaMediaActual()));
        DrawTextW(dc, num(a->notaMediaActual()).c_str(), -1, &rm,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(dc, gui::mezclar(t.textoSuave, t.panel, 0.2));
        SelectObject(dc, gui::fuenteNegrita());
        DrawTextW(dc, L"Sin notas aún", -1, &rm, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(dc, gui::fuenteInterfaz());
    if (a->tieneNotas()) {
        const int nivel = nivelDeNota(a->notaMediaActual());
        std::wstring etiqueta = gui::aW(calificacionDeNivel(nivel));
        HGDIOBJ fmedida = SelectObject(dc, gui::fuenteNegrita());
        SIZE medida;
        ZeroMemory(&medida, sizeof(medida));
        GetTextExtentPoint32W(dc, etiqueta.c_str(), (int)etiqueta.size(), &medida);
        SelectObject(dc, fmedida);
        gui::pastilla(dc, cliente.right - pad - (medida.cx + 22), y + s(82), s(24), etiqueta,
                      gui::colorDeNivel(nivel));
    }

    // --- aviso de prueba cercana
    long dias = diasHastaProximaPrueba(*a);
    if (dias >= 0 && dias <= g_curso.ajustes.diasDeAviso) {
        std::wstring aviso = L"Próxima prueba " + textoCuentaAtras(dias);
        HGDIOBJ fmedida = SelectObject(dc, gui::fuenteNegrita());
        SIZE medida;
        ZeroMemory(&medida, sizeof(medida));
        GetTextExtentPoint32W(dc, aviso.c_str(), (int)aviso.size(), &medida);
        SelectObject(dc, fmedida);
        gui::pastilla(dc, pad, cliente.bottom - pad - s(72), s(24), aviso,
                      dias <= 2 ? t.mal : t.medio);
    }
    else {
        RECT objetivo = {pad, cliente.bottom - pad - s(74), cliente.right - pad,
                         cliente.bottom - pad - s(46)};
        SelectObject(dc, gui::fuenteInterfaz());
        SetTextColor(dc, t.textoSuave);
        std::wstring textoObjetivo = L"Tu objetivo  " + num(g_curso.ajustes.notaObjetivo, 1) + L" / 10";
        // Cuanto hace falta sacar, de media, en lo que queda por evaluar para llegar al objetivo
        double queda = a->ponderacionTotal() - a->ponderacionEvaluada();
        if (a->ponderacionEvaluada() > 0 && queda > 0) {
            double necesita = (g_curso.ajustes.notaObjetivo - a->notaAcumulada()) / (queda / 100.0);
            if (necesita <= 0) textoObjetivo += L"   ·   ya lo tienes asegurado";
            else if (necesita > 10) textoObjetivo += L"   ·   ya no se alcanza con lo que queda";
            else textoObjetivo += L"   ·   necesitas un " + num(necesita, 1) + L" de media en lo que queda";
        }
        std::wstring estudio = resumenDeEstudio(*a);  // va a la derecha en la misma fila: no pisarlo
        if (!estudio.empty()) {
            SIZE me = {0, 0};
            GetTextExtentPoint32W(dc, estudio.c_str(), (int)estudio.size(), &me);
            objetivo.right = std::max<LONG>(objetivo.left + s(200), objetivo.right - me.cx - s(24));
        }
        DrawTextW(dc, textoObjetivo.c_str(), -1, &objetivo,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    // --- estudio con el profesor, a la derecha de la fila del objetivo / aviso
    {
        std::wstring estudio = resumenDeEstudio(*a);
        if (!estudio.empty()) {
            RECT re = {pad + s(260), cliente.bottom - pad - s(74), cliente.right - pad, cliente.bottom - pad - s(46)};
            SelectObject(dc, gui::fuenteInterfaz());
            SetTextColor(dc, t.textoSuave);
            DrawTextW(dc, estudio.c_str(), -1, &re, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
    }

    // --- barra de progreso y pie
    int yBarra = cliente.bottom - pad - s(42);
    RECT rb;
    rb.left = pad;
    rb.right = cliente.right - pad;
    rb.top = yBarra;
    rb.bottom = yBarra + s(8);
    gui::barraProgresoDibujada(dc, rb, a->ponderacionEvaluada(), color,
                               gui::mezclar(t.panel, t.borde, 0.8));

    std::wstring pie = L"Evaluado " + num(a->ponderacionEvaluada(), 0) + L" % de " +
                       num(a->ponderacionTotal(), 0) + L" %      ·      Nota acumulada " +
                       num(a->notaAcumulada()) + L" / 10      ·      " +
                       std::to_wstring((int)a->examenes.size()) + ((int)a->examenes.size() == 1 ? L" prueba" : L" pruebas");
    RECT rp;
    rp.left = pad;
    rp.right = cliente.right - pad;
    rp.top = yBarra + s(16);
    rp.bottom = cliente.bottom - pad + s(6);
    SetTextColor(dc, t.textoSuave);
    DrawTextW(dc, pie.c_str(), -1, &rp, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(dc, fv);
    BitBlt(destino, 0, 0, cliente.right, cliente.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, lienzoViejo);
    DeleteObject(lienzo);
    DeleteDC(dc);
}

LRESULT CALLBACK procResumen(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    if (mensaje == WM_ERASEBKGND) return 1;
    if (mensaje == WM_PRINTCLIENT) { pintarResumen(h, (HDC)wp); return 0; }
    if (mensaje == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        pintarResumen(h, dc);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, mensaje, wp, lp);
}

void registrarClaseResumen() {
    static bool hecho = false;
    if (hecho) return;
    hecho = true;
    WNDCLASSEXW clase;
    ZeroMemory(&clase, sizeof(clase));
    clase.cbSize = sizeof(clase);
    clase.style = CS_HREDRAW | CS_VREDRAW;
    clase.lpfnWndProc = procResumen;
    clase.hInstance = GetModuleHandleW(NULL);
    clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
    clase.lpszClassName = kClaseResumen;
    RegisterClassExW(&clase);
}

// ------------------------------------------------------------------ refresco

void actualizarBotones() {
    bool hayAsignatura = (asignaturaActual() != NULL);
    bool hayPrueba = (examenActual() != NULL);
    EnableWindow(g_profesor, hayAsignatura);
    if (g_botones.size() < 15) return;
    for (int i = 4; i <= 9; ++i) EnableWindow(g_botones[(std::size_t)i], hayAsignatura);
    EnableWindow(g_botones[10], hayAsignatura);
    for (int i = 11; i <= 14; ++i) EnableWindow(g_botones[(std::size_t)i], hayPrueba);
}

void refrescarPie() {
    const double media = g_curso.mediaExpediente();
    std::wstring izq = L"Media del expediente:  " + num(media) + L" / 10";
    if (g_curso.creditosConNota() > 0) izq += L"   ·   " + gui::aW(calificacionDeNota(media));

    double m1 = mediaDeCuatrimestre(g_curso, 1);
    double m2 = mediaDeCuatrimestre(g_curso, 2);
    if (m1 > 0.0) izq += L"      1.º: " + num(m1);
    if (m2 > 0.0) izq += L"      2.º: " + num(m2);

    std::wstring der;
    std::vector<ExamenConAsignatura> proximos = proximosExamenes(g_curso, 1);
    if (!proximos.empty()) {
        Fecha hoy = Fecha::hoy();
        long dias = hoy.diasHasta(proximos[0].examen->fecha);
        der = L"Próxima prueba:  " + gui::aW(proximos[0].asignatura->codigo) + L" - " +
              gui::aW(proximos[0].examen->nombre) + L"   (" + textoCuentaAtras(dias) + L")";
    } else {
        der = L"Sin pruebas pendientes";
    }
    // Solo las ya hechas que esperan nota: las futuras ya se cuentan arriba como «por delante»
    int esperando = 0;
    Fecha hoyPie = Fecha::hoy();
    hoyPie.hora = -1;
    for (const Asignatura& a : g_curso.asignaturas)
        for (const Examen& e : a.examenes)
            if (!e.calificado && e.fecha.valida() && hoyPie.diasHasta(e.fecha) < 0) ++esperando;
    if (esperando > 0) der += L"      ·      " + std::to_wstring(esperando) + L" esperando su nota";

    SetWindowTextW(g_pieIzq, izq.c_str());
    SetWindowTextW(g_pieDer, der.c_str());
    InvalidateRect(g_pieIzq, NULL, TRUE);
    InvalidateRect(g_pieDer, NULL, TRUE);
}

// Reparte el ancho de la tabla de pruebas sin dejar franja vacia: si ninguna prueba tiene
// «Qué entra», esa columna desaparece y su sitio se lo quedan las demas.
void repartirColumnasExam() {
    RECT util;
    GetClientRect(g_listaExam, &util);  // ya descuenta la barra de desplazamiento si la hay
    int ancho = util.right - gui::escalar(2);
    if (ancho < gui::escalar(200)) return;
    bool conApuntes = false;
    if (const Asignatura* a = asignaturaActual())
        for (const Examen& e : a->examenes) conApuntes = conApuntes || !e.apuntes.empty();
    const int fijas[] = {92, 52, 56, 124, 64};  // fecha, peso, nota, calificacion, aporta
    int anchoFijas = 0;
    for (int f : fijas) anchoFijas += gui::escalar(f);
    if (conApuntes) {
        int resto = ancho - anchoFijas;
        int prueba = resto * 45 / 100;
        if (prueba < gui::escalar(90)) prueba = gui::escalar(90);
        int queEntra = resto - prueba;
        if (queEntra < gui::escalar(60)) queEntra = gui::escalar(60);
        for (int i = 0; i < 5; ++i) ListView_SetColumnWidth(g_listaExam, i + 1, gui::escalar(fijas[i]));
        ListView_SetColumnWidth(g_listaExam, 0, prueba);
        ListView_SetColumnWidth(g_listaExam, 6, queEntra);
    } else {
        // Sin apuntes: la prueba se lleva una parte y el resto se reparte en proporcion
        int prueba = ancho * 24 / 100;
        if (prueba < gui::escalar(90)) prueba = gui::escalar(90);
        int sobra = ancho - prueba, usado = 0;
        for (int i = 0; i < 5; ++i) {
            int c = i == 4 ? sobra - usado : gui::escalar(fijas[i]) * sobra / anchoFijas;
            ListView_SetColumnWidth(g_listaExam, i + 1, c);
            usado += c;
        }
        ListView_SetColumnWidth(g_listaExam, 0, prueba);
        ListView_SetColumnWidth(g_listaExam, 6, 0);
    }
}

// Prueba sin nota: cuanto falta, o que ya paso y falta apuntar la nota.
std::wstring estadoDePrueba(const Examen& e) {
    if (!e.fecha.valida()) return L"pendiente";
    Fecha hoy = Fecha::hoy();
    hoy.hora = -1;
    long dias = hoy.diasHasta(e.fecha);
    if (dias < 0) return L"falta nota";
    return textoCuentaAtras(dias);
}

void refrescarExamenes() {
    gui::limpiarLista(g_listaExam);
    Asignatura* a = asignaturaActual();
    if (a) {
        for (std::size_t i = 0; i < a->examenes.size(); ++i) {
            const Examen& e = a->examenes[i];
            int fila = gui::anadirFila(g_listaExam, gui::aW(e.nombre), (LPARAM)i);

            std::wstring fecha = gui::aW(e.fecha.aTextoCorto());
            gui::ponerTexto(g_listaExam, fila, 1, fecha);
            gui::ponerTexto(g_listaExam, fila, 2, num(e.ponderacion, 0) + L" %");
            gui::ponerTexto(g_listaExam, fila, 3, e.calificado ? num(e.nota) : L"-");
            gui::ponerTexto(g_listaExam, fila, 4,
                            e.calificado ? gui::aW(calificacionDeNota(e.nota)) : estadoDePrueba(e));
            gui::ponerTexto(g_listaExam, fila, 5, e.calificado ? num(e.aportacion()) : L"-");
            gui::ponerTexto(g_listaExam, fila, 6, gui::aW(e.apuntes));
        }
    }
    repartirColumnasExam();
    InvalidateRect(g_resumen, NULL, TRUE);
    actualizarBotones();
}

COLORREF colorDeIndice(int k) {
    if (k < 0) return gui::tema().acento;
    return gui::colorDeAsignatura(g_curso.asignaturas[(std::size_t)k].color, (std::size_t)k);
}

// «Tu agenda del dia»: pruebas, clases, tareas y, si es hoy, lo que conviene estudiar.
void mostrarExamenesDelDia(const Fecha& f) {
    static const wchar_t* DIAS[] = {L"Lunes", L"Martes", L"Miércoles", L"Jueves", L"Viernes", L"Sábado", L"Domingo"};
    const gui::Tema& t = gui::tema();
    agenda::Dia d;
    Fecha hoy = Fecha::hoy();
    hoy.hora = -1;
    long dias = hoy.diasHasta(f);
    if (f.valida()) {
        std::wstring mes = gui::aW(Fecha::nombreMes(f.mes));
        if (!mes.empty()) mes[0] = (wchar_t)towlower(mes[0]);
        d.fecha = std::wstring(DIAS[f.diaSemana()]) + L", " + std::to_wstring(f.dia) + L" de " + mes;
        if (f.anio != hoy.anio) d.fecha += L" de " + std::to_wstring(f.anio);
    }
    if (dias == 0) d.etiqueta = L"Hoy";
    else if (dias == 1) d.etiqueta = L"Mañana";
    else if (dias == -1) d.etiqueta = L"Ayer";
    else if (dias > 0) d.etiqueta = L"En " + std::to_wstring(dias) + L" días";
    else d.etiqueta = L"Hace " + std::to_wstring(-dias) + L" días";
    d.colorEtiqueta = dias == 0 ? t.acento : dias > 0 ? t.ok : t.textoSuave;
    d.vacio = L"Nada programado este día.\nElige otro día en el calendario para ver su agenda.";

    // Pruebas de ese dia
    agenda::Seccion pruebas;
    pruebas.titulo = L"Pruebas";
    for (std::size_t k = 0; k < g_curso.asignaturas.size(); ++k) {
        const Asignatura& a = g_curso.asignaturas[k];
        for (const Examen& e : a.examenes) {
            if (e.fecha.anio != f.anio || e.fecha.mes != f.mes || e.fecha.dia != f.dia) continue;
            agenda::Linea l;
            l.titulo = gui::aW(a.codigo) + L"  ·  " + gui::aW(e.nombre);
            l.detalle = gui::aW(a.nombre);
            if (e.fecha.tieneHora()) {
                wchar_t h[16];
                _snwprintf(h, 15, L"%02d:%02d", e.fecha.hora, e.fecha.minuto);
                h[15] = L'\0';
                l.detalle = std::wstring(h) + L"  ·  " + l.detalle;
            }
            l.color = colorDeIndice((int)k);
            if (e.calificado) {
                l.pastilla = num(e.nota);
                l.colorPastilla = gui::colorDeNota(e.nota);
            } else {
                l.pastilla = num(e.ponderacion, 0) + L" %";
                l.colorPastilla = dias >= 0 && dias <= 2 ? t.mal : t.medio;
            }
            l.asignatura = (int)k;
            pruebas.lineas.push_back(l);
        }
    }
    if (!pruebas.lineas.empty()) d.secciones.push_back(pruebas);

    if (f.valida()) {
        int diaSemana = f.diaSemana();
        // Clases: los dias de clase de la ficha de cada asignatura
        agenda::Seccion clases;
        clases.titulo = L"Clases";
        for (std::size_t k = 0; k < g_curso.asignaturas.size(); ++k) {
            if (!hayClase(g_curso.asignaturas[k], diaSemana)) continue;
            agenda::Linea l;
            l.titulo = gui::aW(g_curso.asignaturas[k].codigo);
            l.detalle = gui::aW(g_curso.asignaturas[k].nombre);
            l.color = colorDeIndice((int)k);
            l.asignatura = (int)k;
            clases.lineas.push_back(l);
        }
        if (!clases.lineas.empty()) d.secciones.push_back(clases);
    }

    // Hoy: lo que conviene estudiar (pruebas cercanas de todas las asignaturas y tarjetas pendientes)
    if (dias == 0) {
        js::Valor estudio = leerResumenesDeEstudio();
        std::vector<std::pair<long, agenda::Linea> > lineas;  // ordenadas por urgencia
        for (std::size_t k = 0; k < g_curso.asignaturas.size(); ++k) {
            const Asignatura& a = g_curso.asignaturas[k];
            long mejor = -1;
            std::wstring prueba;
            for (const Examen& e : a.examenes) {
                if (e.calificado || !e.fecha.valida()) continue;
                long dd = hoy.diasHasta(e.fecha);
                if (dd >= 0 && dd <= 14 && (mejor < 0 || dd < mejor)) {
                    mejor = dd;
                    prueba = gui::aW(e.nombre);
                }
            }
            int tarjetas = estudio.esObjeto() && estudio.contiene(a.codigo) ? estudio[a.codigo]["tarjetas_pendientes"].comoEntero() : 0;
            if (mejor < 0 && !tarjetas) continue;
            agenda::Linea l;
            l.titulo = gui::aW(a.codigo) + (mejor >= 0 ? L"  ·  " + prueba : L"  ·  repaso");
            l.detalle = tarjetas ? std::to_wstring(tarjetas) + (tarjetas == 1 ? L" tarjeta para repasar" : L" tarjetas para repasar") : gui::aW(a.nombre);
            if (mejor >= 0) {
                l.pastilla = textoCuentaAtras(mejor);
                l.colorPastilla = mejor <= 2 ? t.mal : mejor <= 7 ? t.medio : t.ok;
            }
            l.color = colorDeIndice((int)k);
            l.asignatura = (int)k;
            lineas.push_back(std::make_pair(mejor < 0 ? 1000L : mejor, l));
        }
        std::stable_sort(lineas.begin(), lineas.end(),
                         [](const std::pair<long, agenda::Linea>& a, const std::pair<long, agenda::Linea>& b) { return a.first < b.first; });
        agenda::Seccion estudiar;
        estudiar.titulo = L"Para estudiar hoy";
        for (const auto& l : lineas) estudiar.lineas.push_back(l.second);
        if (!estudiar.lineas.empty()) d.secciones.push_back(estudiar);
    }
    agenda::poner(g_infoDia, d);
}

void rellenarLista(HWND lista, int cuatri) {
    gui::limpiarLista(lista);
    std::vector<std::size_t> indices =
        asignaturasDe(g_curso, cuatri, g_curso.ajustes.ordenarPorDia);

    for (std::size_t k = 0; k < indices.size(); ++k) {
        const std::size_t i = indices[k];
        const Asignatura& a = g_curso.asignaturas[i];

        int fila = gui::anadirFila(lista, gui::aW(a.diasClase), (LPARAM)i);
        gui::ponerTexto(lista, fila, 1, gui::aW(a.nombre));
        gui::ponerTexto(lista, fila, 2, a.tieneNotas() ? num(a.notaMediaActual()) : L"--");
        g_listaDeAsignatura[i] = lista;
        g_filaDeAsignatura[i] = fila;
    }
}

void seleccionarAsignatura(int indice) {
    if (indice < 0 || (std::size_t)indice >= g_curso.asignaturas.size()) return;
    HWND lista = g_listaDeAsignatura[(std::size_t)indice];
    if (!lista) return;
    limpiarSeleccion(lista == g_listaAsig1 ? g_listaAsig2 : g_listaAsig1);
    g_indiceSel = indice;
    gui::seleccionarFila(lista, g_filaDeAsignatura[(std::size_t)indice]);
}

void refrescarAsignaturas(int indiceAMantener = -1) {
    g_listaDeAsignatura.assign(g_curso.asignaturas.size(), (HWND)NULL);
    g_filaDeAsignatura.assign(g_curso.asignaturas.size(), -1);

    rellenarLista(g_listaAsig1, 1);
    rellenarLista(g_listaAsig2, 2);

    int destino = indiceAMantener;
    if (destino < 0 || (std::size_t)destino >= g_curso.asignaturas.size()) destino = g_indiceSel;
    if (destino < 0 || (std::size_t)destino >= g_curso.asignaturas.size()) {
        destino = g_curso.asignaturas.empty() ? -1 : 0;
    }
    g_indiceSel = destino;
    if (destino >= 0) seleccionarAsignatura(destino);

    refrescarExamenes();
    refrescarPie();
    cal::refrescar(g_calendario);
    InvalidateRect(g_resumen, NULL, TRUE);
}

// ------------------------------------------------------------------ acciones

bool pedirDatosAsignatura(const std::wstring& titulo, Asignatura& a, std::size_t indiceColor) {
    std::vector<gui::Campo> campos;
    campos.push_back(gui::Campo(L"Código", gui::aW(a.codigo)));
    gui::pistaDelUltimo(campos, L"Ej.: SOA", L"Abreviatura corta: sale en el calendario");
    campos.push_back(gui::Campo(L"Nombre", gui::aW(a.nombre)));
    gui::pistaDelUltimo(campos, L"Ej.: Sistemas Operativos Avanzados");
    campos.push_back(gui::Campo(L"Profesor", gui::aW(a.profesor)));
    gui::pistaDelUltimo(campos, L"Opcional");
    campos.push_back(
        gui::Campo(L"Créditos ECTS", std::to_wstring(a.creditos), gui::Campo::Numero, 1, 60));
    gui::pistaDelUltimo(campos, L"Ej.: 6", L"Pesan en la media del expediente");
    campos.push_back(gui::Campo(L"Curso y grupo", gui::aW(a.cuatrimestre)));
    gui::pistaDelUltimo(campos, L"Ej.: 3.º curso, grupo A");
    campos.push_back(gui::Campo(L"Cuatrimestre", std::to_wstring(a.cuatri),
                                gui::Campo::Numero, 1, 2));
    gui::pistaDelUltimo(campos, L"1 o 2", L"1 = primero, 2 = segundo");
    campos.push_back(gui::Campo(L"Días de clase", gui::aW(a.diasClase)));
    gui::pistaDelUltimo(campos, L"Ej.: L X  (lunes y miércoles)", L"L M X J V, separados por espacios");
    campos.push_back(gui::Campo(L"Color",
                                gui::aW(a.color.empty() ? colorDePaleta(indiceColor) : a.color),
                                gui::Campo::Color));
    gui::pistaDelUltimo(campos, L"", L"Para reconocerla en listas y calendario");

    const bool nueva = a.codigo.empty() && a.nombre.empty();
    if (!gui::formulario(g_principal, titulo, campos,
                         nueva ? L"Solo hacen falta el código o el nombre; lo demás puedes rellenarlo luego."
                               : L"Cambia lo que necesites: sus pruebas y notas no se tocan.",
                         gui::ICONO_LIBRO))
        return false;

    a.codigo = gui::aUtf8(campos[0].valor);
    a.nombre = gui::aUtf8(campos[1].valor);
    if (a.nombre.empty()) a.nombre = a.codigo;
    if (a.codigo.empty()) a.codigo = a.nombre;
    a.profesor = gui::aUtf8(campos[2].valor);
    a.creditos = (int)wcstod(campos[3].valor.c_str(), NULL);
    a.cuatrimestre = gui::aUtf8(campos[4].valor);
    a.cuatri = (int)wcstod(campos[5].valor.c_str(), NULL);
    if (a.cuatri != 2) a.cuatri = 1;
    a.diasClase = normalizarDiasClase(gui::aUtf8(campos[6].valor));
    a.color = gui::aUtf8(campos[7].valor);
    return true;
}

void accionNuevaAsignatura() {
    Asignatura a;
    a.cuatrimestre = g_curso.asignaturas.empty()
                         ? std::string("1.er curso")
                         : g_curso.asignaturas[0].cuatrimestre;
    if (!pedirDatosAsignatura(L"Nueva asignatura", a, g_curso.asignaturas.size())) return;
    if (a.codigo.empty() && a.nombre.empty()) {
        gui::avisarError(g_principal, L"Necesitas al menos un código o un nombre.");
        return;
    }
    g_curso.asignaturas.push_back(a);
    prepararCarpetasDelCurso(g_curso);
    guardar();
    refrescarAsignaturas((int)g_curso.asignaturas.size() - 1);
}

void accionEditarAsignatura() {
    Asignatura* a = asignaturaActual();
    if (!a) {
        gui::avisarError(g_principal, L"Selecciona antes una asignatura.");
        return;
    }
    const int indice = g_indiceSel;
    Asignatura copia = *a;
    if (!pedirDatosAsignatura(L"Editar asignatura", copia, (std::size_t)indice)) return;
    g_curso.asignaturas[(std::size_t)indice] = copia;
    prepararCarpetasDelCurso(g_curso);
    guardar();
    refrescarAsignaturas(indice);
}

void accionDuplicarAsignatura() {
    Asignatura* a = asignaturaActual();
    if (!a) {
        gui::avisarError(g_principal, L"Selecciona antes una asignatura.");
        return;
    }
    Asignatura copia = duplicarAsignatura(*a);
    if (!pedirDatosAsignatura(L"Duplicar asignatura", copia, g_curso.asignaturas.size())) return;
    g_curso.asignaturas.push_back(copia);
    prepararCarpetasDelCurso(g_curso);
    guardar();
    refrescarAsignaturas((int)g_curso.asignaturas.size() - 1);
}

void accionBorrarAsignatura() {
    Asignatura* a = asignaturaActual();
    if (!a) return;
    if (!confirmar(L"¿Borrar la asignatura «" + gui::aW(a->nombre) +
                   L"» y todas sus pruebas?\n\n(su carpeta de apuntes no se toca)")) {
        return;
    }
    int indice = g_indiceSel;
    g_curso.asignaturas.erase(g_curso.asignaturas.begin() + (std::ptrdiff_t)indice);
    g_indiceSel = -1;
    guardar();
    refrescarAsignaturas(indice > 0 ? indice - 1 : 0);
}

void accionAbrirCarpeta(int cual) {  // 0 = raiz, 1 = temario, 2 = practicas
    Asignatura* a = asignaturaActual();
    if (!a) {
        gui::avisarError(g_principal, L"Selecciona antes una asignatura.");
        return;
    }
    std::string ruta;
    if (cual == 0) {
        prepararCarpetaDeAsignatura(*a, true);
        prepararCarpetaDeAsignatura(*a, false);
        ruta = carpetaRaizDeAsignatura(*a);
    } else {
        ruta = prepararCarpetaDeAsignatura(*a, cual == 1);
    }
    if (ruta.empty() || !rutas::existe(ruta)) {
        gui::avisarError(g_principal, L"No se pudo crear la carpeta del material.");
        return;
    }
    gui::abrirEnSistema(g_principal, gui::aW(ruta));
}

bool pedirDatosExamen(const std::wstring& titulo, Examen& e, bool esNuevo) {
    Fecha base = e.fecha.valida() ? e.fecha : Fecha::hoy();
    wchar_t fechaTexto[16];
    _snwprintf(fechaTexto, 15, L"%04d-%02d-%02d", base.anio, base.mes, base.dia);
    fechaTexto[15] = L'\0';

    std::vector<gui::Campo> campos;
    campos.push_back(gui::Campo(L"Nombre de la prueba", gui::aW(e.nombre)));
    gui::pistaDelUltimo(campos, L"Ej.: Parcial 1, Práctica 2, Final");
    campos.push_back(gui::Campo(L"Fecha", fechaTexto, gui::Campo::Fecha));
    campos.push_back(gui::Campo(L"Peso en la nota (%)", num(e.ponderacion),
                                gui::Campo::Numero, 0, 100));
    gui::pistaDelUltimo(campos, L"Ej.: 25", L"Entre todas las pruebas deberían sumar 100");
    campos.push_back(gui::Campo(L"Qué entra / notas", gui::aW(e.apuntes)));
    gui::pistaDelUltimo(campos, L"Ej.: temas 1 a 4, con apuntes");
    campos.push_back(
        gui::Campo(L"Ya tengo la nota", e.calificado ? L"1" : L"0", gui::Campo::Casilla));
    campos.push_back(gui::Campo(L"Nota (0-10)", num(e.nota), gui::Campo::Numero, 0, 10));
    gui::pistaDelUltimo(campos, L"", L"Solo cuenta si marcas «Ya tengo la nota»");

    Asignatura* a = asignaturaActual();
    std::wstring sub = L"Un examen, práctica o entrega. Saldrá en el calendario y en la agenda.";
    if (esNuevo && a) {
        double restante = 100.0 - a->ponderacionTotal();
        if (restante > 0.0 && restante < 100.0)
            sub += L" Te queda un " + num(restante, 0) + L" % de la nota por repartir.";
    }
    if (!gui::formulario(g_principal, titulo, campos, sub, gui::ICONO_NOTAS)) return false;

    e.nombre = gui::aUtf8(campos[0].valor);
    if (e.nombre.empty()) e.nombre = "Prueba sin nombre";
    Fecha f;
    if (Fecha::desdeTexto(gui::aUtf8(campos[1].valor), f)) e.fecha = f;
    e.fecha.hora = -1;
    e.ponderacion = wcstod(campos[2].valor.c_str(), NULL);
    e.apuntes = gui::aUtf8(campos[3].valor);
    e.calificado = (campos[4].valor == L"1");
    e.nota = wcstod(campos[5].valor.c_str(), NULL);
    return true;
}

void accionNuevaPrueba() {
    Asignatura* a = asignaturaActual();
    if (!a) {
        gui::avisarError(g_principal, L"Selecciona antes una asignatura.");
        return;
    }
    Examen e;
    e.fecha = cal::seleccion(g_calendario);
    e.ponderacion = 25.0;
    // sugerir lo que falta para llegar al 100 %
    double restante = 100.0 - a->ponderacionTotal();
    if (restante > 0.0 && restante < 100.0) e.ponderacion = restante;

    if (!pedirDatosExamen(L"Nueva prueba en " + gui::aW(a->codigo), e, true)) return;
    a->examenes.push_back(e);
    a->ordenarExamenes();
    guardar();
    refrescarAsignaturas(g_indiceSel);
    cal::seleccionar(g_calendario, e.fecha);
    mostrarExamenesDelDia(e.fecha);
}

void accionDuplicarPrueba() {
    Asignatura* a = asignaturaActual();
    Examen* e = examenActual();
    if (!a || !e) {
        gui::avisarError(g_principal, L"Selecciona antes una prueba de la tabla.");
        return;
    }
    Examen copia = *e;
    copia.nombre += " (copia)";
    copia.calificado = false;
    copia.nota = 0.0;
    if (!pedirDatosExamen(L"Duplicar prueba", copia, false)) return;
    a->examenes.push_back(copia);
    a->ordenarExamenes();
    guardar();
    refrescarAsignaturas(g_indiceSel);
}

void accionEditarPrueba() {
    Asignatura* a = asignaturaActual();
    Examen* e = examenActual();
    if (!a || !e) {
        gui::avisarError(g_principal, L"Selecciona antes una prueba de la tabla.");
        return;
    }
    Examen copia = *e;
    if (!pedirDatosExamen(L"Editar prueba", copia, false)) return;
    *e = copia;
    a->ordenarExamenes();
    guardar();
    refrescarAsignaturas(g_indiceSel);
    cal::seleccionar(g_calendario, copia.fecha);
    mostrarExamenesDelDia(copia.fecha);
}

void accionPonerNota() {
    Asignatura* a = asignaturaActual();
    Examen* e = examenActual();
    if (!a || !e) {
        gui::avisarError(g_principal, L"Selecciona antes una prueba de la tabla.");
        return;
    }
    std::vector<gui::Campo> campos;
    campos.push_back(gui::Campo(L"Nota obtenida (0-10)", num(e->calificado ? e->nota : 5.0),
                                gui::Campo::Numero, 0, 10));
    gui::pistaDelUltimo(campos, L"Ej.: 7,5", L"Se admiten decimales con coma o punto");
    if (!gui::formulario(g_principal, gui::aW(e->nombre) + L" · poner nota", campos,
                         L"Esta prueba vale un " + num(e->ponderacion, 0) +
                             L" % de " + gui::aW(a->codigo) + L". Al guardar se recalcula la media.",
                         gui::ICONO_ESTRELLA))
        return;

    e->nota = wcstod(campos[0].valor.c_str(), NULL);
    e->calificado = true;
    guardar();
    refrescarAsignaturas(g_indiceSel);
}

// Marca o desmarca la prueba como calificada, sin abrir ningun cuadro.
void accionAlternarHecha() {
    Examen* e = examenActual();
    if (!e) return;
    if (e->calificado) {
        e->calificado = false;
    } else {
        accionPonerNota();
        return;
    }
    guardar();
    refrescarAsignaturas(g_indiceSel);
}

void accionBorrarPrueba() {
    Asignatura* a = asignaturaActual();
    Examen* e = examenActual();
    if (!a || !e) {
        gui::avisarError(g_principal, L"Selecciona antes una prueba de la tabla.");
        return;
    }
    if (!confirmar(L"¿Borrar la prueba «" + gui::aW(e->nombre) + L"»?")) return;

    int fila = gui::filaSeleccionada(g_listaExam);
    LPARAM indice = gui::datosDeFila(g_listaExam, fila);
    if (indice < 0) return;
    a->examenes.erase(a->examenes.begin() + (std::ptrdiff_t)indice);
    guardar();
    refrescarAsignaturas(g_indiceSel);
}

void accionExportarHtml() {
    std::string destino =
        rutas::absoluta(rutas::unir(rutas::padre(g_almacen->ruta()), "informe.html"));
    std::string error;
    if (!html::exportar(g_curso, destino, error)) {
        gui::avisarError(g_principal, L"No se pudo generar el informe:\n" + gui::aW(error));
        return;
    }
    if (gui::preguntar(g_principal,
                       L"Informe generado en:\n" + gui::aW(destino) + L"\n\n¿Abrirlo ahora?")) {
        gui::abrirEnSistema(g_principal, gui::aW(destino));
    }
}

// ------------------------------------------------------- acciones de ajustes

void abrirCarpetaPrograma(HWND ventana) {
    gui::abrirEnSistema(ventana, gui::aW(rutas::directorioDelEjecutable()));
}

void abrirCarpetaAsignaturas(HWND ventana) {
    std::string ruta = rutas::unir(rutas::directorioDelEjecutable(), "Asignaturas");
    rutas::crearDirectorios(ruta);
    gui::abrirEnSistema(ventana, gui::aW(ruta));
}

void abrirCarpetaDatos(HWND ventana) {
    std::string ruta = rutas::padre(g_almacen->ruta());
    if (ruta.empty()) ruta = rutas::directorioDelEjecutable();
    gui::abrirEnSistema(ventana, gui::aW(rutas::absoluta(ruta)));
}

void hacerCopiaDeSeguridad(HWND ventana) {
    guardar();
    std::string carpeta = rutas::unir(rutas::padre(g_almacen->ruta()), "copias");
    if (!rutas::crearDirectorios(carpeta)) {
        gui::avisarError(ventana, L"No se pudo crear la carpeta de copias.");
        return;
    }
    Fecha hoy = Fecha::hoy();
    char nombre[64];
    std::snprintf(nombre, sizeof(nombre), "asignaturas-%04d-%02d-%02d.json", hoy.anio, hoy.mes,
                  hoy.dia);
    std::string destino = rutas::unir(carpeta, nombre);

    std::ifstream origen(g_almacen->ruta().c_str(), std::ios::binary);
    std::ofstream copia(destino.c_str(), std::ios::binary | std::ios::trunc);
    if (!origen || !copia) {
        gui::avisarError(ventana, L"No se pudo copiar el fichero de datos.");
        return;
    }
    copia << origen.rdbuf();
    copia.close();
    gui::avisar(ventana, L"Copia guardada en:\n" + gui::aW(rutas::absoluta(destino)));
}

void mostrarAtajos(HWND ventana) {
    gui::avisar(ventana,
                L"Atajos de teclado\r\n\r\n"
                L"Ctrl + N     Nueva asignatura\r\n"
                L"Ctrl + D     Duplicar asignatura\r\n"
                L"F2               Editar asignatura\r\n"
                L"Ctrl + P     Nueva prueba\r\n"
                L"F3               Editar prueba\r\n"
                L"Espacio      Poner nota / marcar pendiente\r\n"
                L"Supr            Borrar la prueba elegida\r\n"
                L"Ctrl + E     Exportar informe HTML\r\n"
                L"Ctrl + T     Tema claro / oscuro\r\n"
                L"Ctrl + S     Guardar ahora\r\n"
                L"F1               Guía rápida\r\n"
                L"F5               Recargar la vista\r\n\r\n"
                L"En el Profesor  (F1 los enseña todos)\r\n\r\n"
                L"Ctrl + K     Paleta de comandos: ir a cualquier sitio\r\n"
                L"F9               Modo enfoque (sin distracciones)\r\n"
                L"F11             Pantalla completa\r\n"
                L"Ctrl + M     Música sí / no\r\n"
                L"Ctrl + F     Buscar en los apuntes");
}

// ------------------------------------------------------------- guia rapida
// Ventana dibujada que explica la pantalla en cuatro pasos. Sale sola la primera vez
// (queda una marca en datos/guia_vista) y luego con el boton ? o F1.

const wchar_t* kClaseGuia = L"UniversalisGuia";
bool g_guiaEncima = false;

struct PasoGuia {
    const wchar_t* dibujo;
    gui::Icono reserva;
    COLORREF color;
    const wchar_t* titulo;
    const wchar_t* texto;
};

const PasoGuia* pasosGuia(int& n) {
    static const PasoGuia pasos[] = {
        {L"asignaturas", gui::ICONO_LIBRO, RGB(96, 165, 250), L"1.  Tus asignaturas  ·  columna izquierda",
         L"Crea cada asignatura con «Nueva asignatura». Pulsa una para ver su ficha; con «Temario» abres la "
         L"carpeta donde guardar sus apuntes."},
        {L"notas", gui::ICONO_NOTAS, RGB(45, 212, 191), L"2.  Pruebas y notas  ·  centro",
         L"Añade exámenes, prácticas y entregas con «Nueva prueba» indicando cuánto pesan. Cuando tengas la nota, "
         L"«Poner nota»: la media y lo que te falta se calculan solos."},
        {L"calendario", gui::ICONO_CALENDARIO, RGB(251, 146, 60), L"3.  Calendario y agenda  ·  derecha",
         L"Los días con pruebas salen marcados del color de su asignatura. Pulsa un día para ver debajo sus "
         L"pruebas y clases."},
        {L"estudiar", gui::ICONO_BIRRETE, RGB(167, 139, 250), L"4.  Estudiar con el Profesor  ·  botón de arriba",
         L"Elige una asignatura y ábrelo: prepara tests, tarjetas y resúmenes a partir de los apuntes de su "
         L"carpeta Temario, y te propone un plan para cada día."},
    };
    n = (int)(sizeof(pasos) / sizeof(pasos[0]));
    return pasos;
}

RECT botonGuia(HWND h) {
    RECT c;
    GetClientRect(h, &c);
    const auto s = gui::escalar;
    RECT r = {c.right - s(28) - s(150), c.bottom - s(24) - s(42), c.right - s(28), c.bottom - s(24)};
    return r;
}

void pintarGuia(HWND h, HDC destino) {
    RECT c;
    GetClientRect(h, &c);
    HDC dc = CreateCompatibleDC(destino);
    HBITMAP lienzo = CreateCompatibleBitmap(destino, c.right, c.bottom);
    HGDIOBJ viejo = SelectObject(dc, lienzo);
    const gui::Tema& t = gui::tema();
    const auto s = gui::escalar;
    FillRect(dc, &c, gui::brochaFondo());
    SetBkMode(dc, TRANSPARENT);

    // Cabecera
    RECT cab = {0, 0, c.right, s(112)};
    HBRUSH b = CreateSolidBrush(gui::mezclar(t.fondo, t.acento, t.oscuro ? 0.10 : 0.06));
    FillRect(dc, &cab, b);
    DeleteObject(b);
    RECT icono = {s(28), s(24), s(28) + s(64), s(24) + s(64)};
    gui::panelRedondeado(dc, icono, gui::mezclar(t.panel, t.acento, 0.18), gui::mezclar(t.panel, t.acento, 0.35), s(18));
    InflateRect(&icono, -s(10), -s(10));
    gui::decoracion(dc, L"bienvenida", icono, t.acento, 255, gui::ICONO_BIRRETE);
    HGDIOBJ fv = SelectObject(dc, gui::fuenteTitulo());
    SetTextColor(dc, t.texto);
    RECT r = {s(112), s(28), c.right - s(28), s(60)};
    DrawTextW(dc, L"Bienvenido a Universalis", -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, gui::fuenteInterfaz());
    SetTextColor(dc, t.textoSuave);
    r.top = s(60);
    r.bottom = s(86);
    DrawTextW(dc, L"Tu curso en una sola ventana. Así se usa, en cuatro pasos:", -1, &r,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Pasos
    int n = 0;
    const PasoGuia* pasos = pasosGuia(n);
    int y = s(134);
    const int lado = s(48);
    for (int i = 0; i < n; ++i) {
        RECT cuadro = {s(28), y, s(28) + lado, y + lado};
        COLORREF fondo = gui::mezclar(t.fondo, pasos[i].color, t.oscuro ? 0.20 : 0.14);
        gui::panelRedondeado(dc, cuadro, fondo, fondo, s(14));
        InflateRect(&cuadro, -s(11), -s(11));
        gui::decoracion(dc, pasos[i].dibujo, cuadro, pasos[i].color, 255, pasos[i].reserva);
        RECT rt = {s(28) + lado + s(18), y - s(2), c.right - s(28), y + s(22)};
        SelectObject(dc, gui::fuenteNegrita());
        SetTextColor(dc, t.texto);
        DrawTextW(dc, pasos[i].titulo, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        rt.top = rt.bottom + s(2);
        rt.bottom = rt.top + s(48);
        SelectObject(dc, gui::fuenteInterfaz());
        SetTextColor(dc, t.textoSuave);
        DrawTextW(dc, pasos[i].texto, -1, &rt, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        y += s(84);
    }

    // Pie: atajos y boton
    HPEN lapiz = CreatePen(PS_SOLID, 1, t.borde);
    HGDIOBJ lv = SelectObject(dc, lapiz);
    MoveToEx(dc, s(28), c.bottom - s(90), NULL);
    LineTo(dc, c.right - s(28), c.bottom - s(90));
    SelectObject(dc, lv);
    DeleteObject(lapiz);
    RECT boton = botonGuia(h);
    RECT atajos = {s(28), boton.top, boton.left - s(16), boton.bottom};
    SetTextColor(dc, gui::mezclar(t.textoSuave, t.fondo, 0.2));
    DrawTextW(dc, L"Atajos:  Ctrl+N asignatura  ·  Ctrl+P prueba  ·  Espacio nota  ·  F1 esta guía",
              -1, &atajos, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    COLORREF fb = g_guiaEncima ? gui::mezclar(t.acento, RGB(255, 255, 255), 0.12) : t.acento;
    gui::panelRedondeado(dc, boton, fb, fb, s(12));
    SelectObject(dc, gui::fuenteNegrita());
    bool claro = (GetRValue(fb) * 299 + GetGValue(fb) * 587 + GetBValue(fb) * 114) > 155000;
    SetTextColor(dc, claro ? RGB(15, 25, 40) : RGB(255, 255, 255));
    DrawTextW(dc, L"Entendido", -1, &boton, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    HPEN marco = CreatePen(PS_SOLID, 1, gui::mezclar(t.borde, t.acento, 0.3));
    lv = SelectObject(dc, marco);
    HGDIOBJ bv = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, 0, 0, c.right, c.bottom);
    SelectObject(dc, bv);
    SelectObject(dc, lv);
    DeleteObject(marco);

    SelectObject(dc, fv);
    BitBlt(destino, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, viejo);
    DeleteObject(lienzo);
    DeleteDC(dc);
}

LRESULT CALLBACK procGuia(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    switch (mensaje) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            pintarGuia(h, dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_NCHITTEST: {  // se arrastra desde la cabecera
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            ScreenToClient(h, &pt);
            return pt.y < gui::escalar(112) ? HTCAPTION : HTCLIENT;
        }
        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            RECT b = botonGuia(h);
            bool encima = PtInRect(&b, pt) != 0;
            if (encima != g_guiaEncima) {
                g_guiaEncima = encima;
                InvalidateRect(h, &b, FALSE);
                TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0};
                TrackMouseEvent(&te);
            }
            SetCursor(LoadCursorW(NULL, encima ? IDC_HAND : IDC_ARROW));
            return 0;
        }
        case WM_MOUSELEAVE:
            g_guiaEncima = false;
            InvalidateRect(h, NULL, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            RECT b = botonGuia(h);
            if (PtInRect(&b, pt)) DestroyWindow(h);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE || wp == VK_RETURN || wp == VK_SPACE) DestroyWindow(h);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(h, mensaje, wp, lp);
}

void abrirGuia(HWND padre) {
    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW clase;
        ZeroMemory(&clase, sizeof(clase));
        clase.cbSize = sizeof(clase);
        clase.style = CS_DROPSHADOW;
        clase.lpfnWndProc = procGuia;
        clase.hInstance = GetModuleHandleW(NULL);
        clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
        clase.lpszClassName = kClaseGuia;
        registrada = RegisterClassExW(&clase) != 0;
    }
    const int ancho = gui::escalar(760), alto = gui::escalar(668);
    RECT rp;
    GetWindowRect(padre, &rp);
    int x = rp.left + ((rp.right - rp.left) - ancho) / 2;
    int y = rp.top + ((rp.bottom - rp.top) - alto) / 2;
    if (y < rp.top) y = rp.top;
    g_guiaEncima = false;
    HWND h = CreateWindowExW(0, kClaseGuia, L"Guía rápida", WS_POPUP, x, y, ancho, alto, padre,
                             NULL, GetModuleHandleW(NULL), NULL);
    if (!h) return;
    EnableWindow(padre, FALSE);
    ShowWindow(h, SW_SHOW);
    SetFocus(h);
    MSG msg;
    while (IsWindow(h) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(padre, TRUE);
    SetForegroundWindow(padre);
}

// La primera vez que se abre el programa se enseña la guía.
void guiaSiEsLaPrimeraVez(HWND ventana) {
    std::string marca = rutas::unir(rutas::directorioDelEjecutable(), "datos/guia_vista");
    if (rutas::existe(marca)) return;
    std::ofstream f(marca.c_str());
    f << "La guía rápida ya se ha enseñado. Borra este fichero para verla otra vez al arrancar.\n";
    abrirGuia(ventana);
}

void aplicarTema() {
    ajustes::aplicarApariencia(g_curso);  // estilo global (paleta, letra, esquinas...) y tema
    gui::modoOscuroDelSistema(gui::tema().oscuro);
    gui::barraDeTituloOscura(g_principal);
    gui::tematizarLista(g_listaAsig1);
    gui::tematizarLista(g_listaAsig2);
    gui::tematizarLista(g_listaExam);
    gui::alturaFilasLista(g_listaAsig1, gui::escalar(38));
    gui::alturaFilasLista(g_listaAsig2, gui::escalar(38));
    gui::alturaFilasLista(g_listaExam, gui::escalar(38));
    gui::aplicarFuente(g_principal);
    SendMessageW(g_tituloApp, WM_SETFONT, (WPARAM)gui::fuenteTitulo(), TRUE);
    InvalidateRect(g_infoDia, NULL, FALSE);
    cal::ponerOpciones(g_calendario, g_curso.ajustes.primerDiaLunes,
                       g_curso.ajustes.mostrarFinDeSemana, g_curso.ajustes.diasDeAviso);
}

void colocarControles(HWND ventana);

void accionCambiarTema() {
    g_curso.temaOscuro = !gui::tema().oscuro;
    g_curso.ajustes.temaSistema = false;  // cambiarlo a mano deja de seguir a Windows
    aplicarTema();
    guardar();
    colocarControles(g_principal);
    RedrawWindow(g_principal, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void accionAjustes() {
    std::vector<ajustes::AccionExtra> extra = {
        {L"Abrir la carpeta del programa", abrirCarpetaPrograma},
        {L"Abrir la carpeta Asignaturas", abrirCarpetaAsignaturas},
        {L"Abrir la carpeta de datos", abrirCarpetaDatos},
        {L"Guardar copia de seguridad", hacerCopiaDeSeguridad},
        {L"Ver los atajos de teclado", mostrarAtajos}};
    bool guardado = ajustes::abrir(g_principal, g_curso, extra);
    if (guardado) guardar();
    aplicarTema();
    refrescarAsignaturas(g_indiceSel);
    colocarControles(g_principal);
    RedrawWindow(g_principal, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void accionDiaElegido() {
    Fecha f = cal::seleccion(g_calendario);
    mostrarExamenesDelDia(f);

    for (std::size_t i = 0; i < g_curso.asignaturas.size(); ++i) {
        const Asignatura& a = g_curso.asignaturas[i];
        for (std::size_t j = 0; j < a.examenes.size(); ++j) {
            const Fecha& fe = a.examenes[j].fecha;
            if (fe.anio == f.anio && fe.mes == f.mes && fe.dia == f.dia) {
                seleccionarAsignatura((int)i);
                refrescarExamenes();
                gui::seleccionarFila(g_listaExam, (int)j);
                return;
            }
        }
    }
}

// ------------------------------------------------------------------- ventana

void prepararListaAsignaturas(HWND lista) {
    gui::columna(lista, 0, L"Día", gui::escalar(46));  // la letra del día de clase: deja más sitio al nombre
    gui::columna(lista, 1, L"Asignatura", gui::escalar(194));
    gui::columna(lista, 2, L"Media", gui::escalar(68));
    gui::alturaFilasLista(lista, gui::escalar(38));
}

void crearControles(HWND ventana) {
    registrarClaseResumen();

    g_tituloApp = gui::crearEtiqueta(ventana, L"UNIVERSALIS", ID_TITULO_APP);
    g_subtituloApp = gui::crearEtiqueta(ventana, L"Tu espacio académico");

    g_botones.push_back(gui::crearBotonIcono(ventana, gui::ICONO_EXPORTAR, ID_BTN_EXPORTAR,
                                             L"Informe: crea una página con todas tus notas para verla o imprimirla  (Ctrl+E)"));
    g_botones.push_back(gui::crearBotonIcono(ventana, gui::ICONO_TEMA, ID_BTN_TEMA,
                                             L"Cambiar entre tema claro y oscuro  (Ctrl+T)"));
    g_botones.push_back(gui::crearBotonIcono(ventana, gui::ICONO_AJUSTES, ID_BTN_AJUSTES,
                                             L"Ajustes: colores, letra, nota objetivo, avisos y copias de seguridad"));
    g_guia = gui::crearBotonIcono(ventana, gui::ICONO_AYUDA, ID_BTN_GUIA,
                                  L"Guía rápida: qué es cada parte de la ventana  (F1)");

    g_listaAsig1 = gui::crearLista(ventana, ID_LISTA_ASIG1);
    prepararListaAsignaturas(g_listaAsig1);
    g_listaAsig2 = gui::crearLista(ventana, ID_LISTA_ASIG2);
    prepararListaAsignaturas(g_listaAsig2);

    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_MAS, L"Nueva asignatura", ID_BTN_NUEVA_ASIG, true,
                                                L"Añade una asignatura: código, nombre, créditos y días de clase  (Ctrl+N)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_EDITAR, L"Editar", ID_BTN_EDITAR_ASIG, false,
                                                L"Cambia los datos de la asignatura elegida  (F2 o doble clic)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_DUPLICAR, L"Duplicar", ID_BTN_DUPLICAR_ASIG, false,
                                                L"Crea otra asignatura con las mismas pruebas y pesos  (Ctrl+D)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_BORRAR, L"Borrar", ID_BTN_BORRAR_ASIG, false,
                                                L"Borra la asignatura y sus pruebas (su carpeta de apuntes no se toca)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_CARPETA, L"Carpeta", ID_BTN_CARPETA, false,
                                                L"Abre en el Explorador la carpeta de la asignatura"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_LIBRO, L"Temario", ID_BTN_TEMARIO, false,
                                                L"Abre la carpeta del temario: deja aquí tus apuntes (PDF, Word…) "
                                                L"y el Profesor estudiará con ellos"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_MATRAZ, L"Prácticas", ID_BTN_PRACTICAS, false,
                                                L"Abre la carpeta de prácticas y entregas de la asignatura"));

    g_resumen = CreateWindowExW(0, kClaseResumen, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10,
                                ventana, (HMENU)(INT_PTR)ID_RESUMEN, NULL, NULL);

    g_listaExam = gui::crearLista(ventana, ID_LISTA_EXAM);
    gui::columna(g_listaExam, 0, L"Prueba", gui::escalar(180));
    gui::columna(g_listaExam, 1, L"Fecha", gui::escalar(190));
    gui::columna(g_listaExam, 2, L"Peso", gui::escalar(56));
    gui::columna(g_listaExam, 3, L"Nota", gui::escalar(58));
    gui::columna(g_listaExam, 4, L"Calificación", gui::escalar(104));
    gui::columna(g_listaExam, 5, L"Aporta", gui::escalar(58));
    gui::columna(g_listaExam, 6, L"Qué entra", gui::escalar(160));
    gui::alturaFilasLista(g_listaExam, gui::escalar(38));

    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_MAS, L"Nueva prueba", ID_BTN_NUEVA_PRUEBA, true,
                                                L"Añade un examen, práctica o entrega con su fecha y lo que pesa en la nota  (Ctrl+P)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_ESTRELLA, L"Poner nota", ID_BTN_NOTA, false,
                                                L"Apunta la nota de la prueba elegida: la media se recalcula sola  (Espacio)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_DUPLICAR, L"Duplicar", ID_BTN_DUPLICAR_PRUEBA, false,
                                                L"Copia la prueba elegida (útil para PL1, PL2, PL3…)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_EDITAR, L"Editar", ID_BTN_EDITAR_PRUEBA, false,
                                                L"Cambia la fecha, el peso o lo que entra  (F3 o doble clic)"));
    g_botones.push_back(gui::crearBotonConIcono(ventana, gui::ICONO_BORRAR, L"Borrar", ID_BTN_BORRAR_PRUEBA, false,
                                                L"Borra la prueba elegida  (Supr)"));

    g_calendario = cal::crear(ventana, ID_CALENDARIO);
    cal::ponerCurso(g_calendario, &g_curso);

    g_infoDia = agenda::crear(ventana, ID_DIA);

    g_pieIzq = gui::crearEtiqueta(ventana, L"", ID_PIE_IZQ);
    g_pieDer = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 10, 10,
                               ventana, (HMENU)(INT_PTR)ID_PIE_DER, NULL, NULL);

    g_profesor = gui::crearBotonConIcono(ventana, gui::ICONO_BIRRETE, L"Estudiar con el Profesor", ID_BTN_PROFESOR, true,
                                         L"Tests, tarjetas, resúmenes y dudas sobre la asignatura elegida, "
                                         L"preparados a partir de tus apuntes");
    gui::botonEnBarra(g_profesor);
    gui::aplicarFuente(ventana);
    SendMessageW(g_tituloApp, WM_SETFONT, (WPARAM)gui::fuenteTitulo(), TRUE);
}

void colocar(HWND control, int x, int y, int cx, int cy) {
    if (control) MoveWindow(control, x - g_scrollX, y - g_scrollY, cx, cy, TRUE);
}

void colocarControles(HWND ventana) {
    if (g_botones.size() < 15) return;
    static bool colocando = false;
    if (colocando) return;
    colocando = true;

    RECT cliente;
    GetClientRect(ventana, &cliente);
    // Un lienzo desplazable mantiene los controles accesibles al ampliar a 150%.
    // Dos pasadas tienen en cuenta el espacio que ocupa la otra barra.
    for (int i = 0; i < 2; ++i) {
        g_anchoLienzo = (int)cliente.right > gui::escalar(1240) ? (int)cliente.right : gui::escalar(1240);
        g_altoLienzo = (int)cliente.bottom > gui::escalar(720) ? (int)cliente.bottom : gui::escalar(720);
        SCROLLINFO si = {};
        si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMax = g_anchoLienzo - 1; si.nPage = cliente.right; si.nPos = g_scrollX;
        SetScrollInfo(ventana, SB_HORZ, &si, TRUE);
        g_scrollX = GetScrollPos(ventana, SB_HORZ);
        si.nMax = g_altoLienzo - 1; si.nPage = cliente.bottom; si.nPos = g_scrollY;
        SetScrollInfo(ventana, SB_VERT, &si, TRUE);
        g_scrollY = GetScrollPos(ventana, SB_VERT);
        GetClientRect(ventana, &cliente);
    }
    const int ancho = g_anchoLienzo;
    const int alto = g_altoLienzo;

    const int margen = MARGEN();
    const int hueco = HUECO();
    const int anchoIzq = ANCHO_IZQ();
    const int anchoCal = ANCHO_CAL();
    const int altoBoton = ALTO_BOTON();
    const int altoBarra = ALTO_BARRA();
    const int altoTitulo = ALTO_TITULO();

    // ---- barra superior negra
    colocar(g_tituloApp, margen + gui::escalar(52), gui::escalar(23), gui::escalar(250),
            gui::escalar(28));
    colocar(g_subtituloApp, margen + gui::escalar(52), gui::escalar(54), gui::escalar(255), gui::escalar(22));
    // Derecha: herramientas pequenas (solo icono) y, antes, las dos grandes partes del programa.
    const int altoAccion = gui::escalar(40);
    const int yIcono = (altoBarra - altoAccion) / 2;
    const int derecha = ancho - ANCHO_CONTROLES();  // deja sitio a minimizar/maximizar/cerrar
    HWND iconos[] = {g_botones[0], g_botones[1], g_botones[2], g_guia};
    int x = derecha - margen;
    for (int i = 3; i >= 0; --i) {
        x -= altoAccion;
        colocar(iconos[i], x, yIcono, altoAccion, altoAccion);
        x -= gui::escalar(6);
    }
    x -= gui::escalar(14);
    const int anchoProfesor = gui::escalar(246);
    x -= anchoProfesor;
    colocar(g_profesor, x, yIcono, anchoProfesor, altoAccion);
    g_finDatosBarra = x - gui::escalar(24);

    const int arriba = altoBarra + hueco;
    const int fondo = alto - ALTO_PIE() - hueco;

    // =================================================== columna izquierda
    const int xIzq = margen;
    const int altoMaterial = gui::escalar(30);
    int nBotones = 3 * altoBoton + 8 + altoMaterial;
    int disponible = (fondo - arriba) - (2 * altoTitulo) - hueco - nBotones - hueco;
    if (disponible < 220) disponible = 220;

    int n1 = 0, n2 = 0;
    for (std::size_t i = 0; i < g_curso.asignaturas.size(); ++i) {
        if (g_curso.asignaturas[i].cuatri == 2) ++n2; else ++n1;
    }
    if (n1 + n2 == 0) { n1 = 1; n2 = 1; }
    int alto1 = disponible * (n1 + 1) / (n1 + n2 + 2);
    if (alto1 < 110) alto1 = 110;
    int alto2 = disponible - alto1;
    if (alto2 < 110) {
        alto2 = 110;
        alto1 = disponible - alto2;
        if (alto1 < 110) alto1 = 110;
    }

    int y = arriba;
    g_rTitulo1.left = xIzq;
    g_rTitulo1.right = xIzq + anchoIzq;
    g_rTitulo1.top = y;
    g_rTitulo1.bottom = y + altoTitulo;
    y += altoTitulo;
    colocar(g_listaAsig1, xIzq, y, anchoIzq, alto1);
    y += alto1 + hueco;

    g_rTitulo2.left = xIzq;
    g_rTitulo2.right = xIzq + anchoIzq;
    g_rTitulo2.top = y;
    g_rTitulo2.bottom = y + altoTitulo;
    y += altoTitulo;
    colocar(g_listaAsig2, xIzq, y, anchoIzq, alto2);
    y += alto2 + hueco;

    // Nueva asignatura a todo el ancho; debajo, lo que se hace con la elegida y su material.
    colocar(g_botones[3], xIzq, y, anchoIzq, altoBoton);
    y += altoBoton + 8;
    int tercio = (anchoIzq - 16) / 3;
    for (int i = 0; i < 3; ++i)
        colocar(g_botones[(std::size_t)(4 + i)], xIzq + i * (tercio + 8), y, tercio, altoBoton);
    y += altoBoton;
    g_rMaterial.left = xIzq;
    g_rMaterial.right = xIzq + anchoIzq;
    g_rMaterial.top = y;
    g_rMaterial.bottom = y + altoMaterial;
    y += altoMaterial;
    for (int i = 0; i < 3; ++i)
        colocar(g_botones[(std::size_t)(7 + i)], xIzq + i * (tercio + 8), y, tercio, altoBoton);

    // ===================================================== columna derecha
    const int xDer = ancho - margen - anchoCal;
    g_rTituloCal.left = xDer;
    g_rTituloCal.right = xDer + anchoCal;
    g_rTituloCal.top = arriba;
    g_rTituloCal.bottom = arriba + altoTitulo;
    colocar(g_calendario, xDer, arriba + altoTitulo, anchoCal, ALTO_CAL());

    int yDia = arriba + altoTitulo + ALTO_CAL() + hueco;
    g_rTituloDia.left = xDer;
    g_rTituloDia.right = xDer + anchoCal;
    g_rTituloDia.top = yDia;
    g_rTituloDia.bottom = yDia + altoTitulo;

    int altoDia = fondo - (yDia + altoTitulo);
    if (altoDia < 60) altoDia = 60;
    colocar(g_infoDia, xDer + 14, yDia + altoTitulo + 10, anchoCal - 28, altoDia - 20);

    // ====================================================== columna central
    const int xCen = xIzq + anchoIzq + margen;
    int anchoCen = xDer - margen - xCen;
    if (anchoCen < 360) anchoCen = 360;

    g_rTituloFicha.left = xCen;
    g_rTituloFicha.right = xCen + anchoCen;
    g_rTituloFicha.top = arriba;
    g_rTituloFicha.bottom = arriba + altoTitulo;
    colocar(g_resumen, xCen, arriba + altoTitulo, anchoCen, ALTO_RESUMEN());

    int yTabla = arriba + altoTitulo + ALTO_RESUMEN() + hueco;
    g_rTituloExam.left = xCen;
    g_rTituloExam.right = xCen + anchoCen;
    g_rTituloExam.top = yTabla;
    g_rTituloExam.bottom = yTabla + altoTitulo;

    int yBotDer = fondo - altoBoton;
    int altoTabla = yBotDer - (yTabla + altoTitulo) - hueco;
    if (altoTabla < 120) altoTabla = 120;
    colocar(g_listaExam, xCen, yTabla + altoTitulo, anchoCen, altoTabla);

    // «Nueva prueba» y «Poner nota» son lo mas usado: un poco mas anchos que el resto.
    int unidad = (anchoCen - 32) * 10 / 56;
    int xb = xCen;
    for (int i = 0; i < 5; ++i) {
        int w = i < 2 ? unidad * 13 / 10 : (i == 4 ? xCen + anchoCen - xb : unidad);
        colocar(g_botones[(std::size_t)(10 + i)], xb, yBotDer, w, altoBoton);
        xb += w + 8;
    }

    // Las columnas se reparten el ancho disponible: nunca hace falta desplazarse en horizontal.
    for (HWND lista : {g_listaAsig1, g_listaAsig2}) {
        ListView_SetColumnWidth(lista, 0, gui::escalar(46));
        // La columna del nombre ocupa todo el ancho util (sin dejar franja vacia a la derecha).
        RECT util;
        GetClientRect(lista, &util);
        int nombre = util.right - gui::escalar(46) - gui::escalar(60);
        ListView_SetColumnWidth(lista, 1, nombre > gui::escalar(80) ? nombre : anchoIzq - gui::escalar(134));
        ListView_SetColumnWidth(lista, 2, gui::escalar(60));
    }
    repartirColumnasExam();

    // ---- pie
    colocar(g_pieIzq, margen, alto - ALTO_PIE() + 8, ancho / 2 - margen - 8, gui::escalar(22));
    colocar(g_pieDer, ancho / 2, alto - ALTO_PIE() + 8, ancho / 2 - margen, gui::escalar(22));

    InvalidateRect(ventana, NULL, TRUE);
    colocando = false;
}

void pintarVentana(HWND ventana, HDC dc) {
    const gui::Tema& t = gui::tema();
    RECT cliente;
    GetClientRect(ventana, &cliente);
    cliente.right = g_anchoLienzo; cliente.bottom = g_altoLienzo;
    int estadoDC = SaveDC(dc);
    SetViewportOrgEx(dc, -g_scrollX, -g_scrollY, NULL);

    RECT barra = cliente;
    barra.bottom = ALTO_BARRA();
    HBRUSH negro = CreateSolidBrush(gui::colorBarraSuperior());
    FillRect(dc, &barra, negro);
    DeleteObject(negro);

    RECT franja = barra;
    franja.top = barra.bottom - 1;
    HBRUSH acento = CreateSolidBrush(gui::mezclar(gui::colorBarraSuperior(), t.acento, 0.75));
    FillRect(dc, &franja, acento);
    DeleteObject(acento);

    RECT marca = {MARGEN(), gui::escalar(27), MARGEN() + gui::escalar(38), gui::escalar(65)};
    gui::panelRedondeado(dc, marca, t.acento, t.acento, gui::escalar(12));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(16, 28, 46));
    HGDIOBJ fuente = SelectObject(dc, gui::fuenteTitulo());
    DrawTextW(dc, L"U", -1, &marca, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, gui::fuenteInterfaz());
    RECT estadisticas = {gui::escalar(370), gui::escalar(27), g_finDatosBarra, gui::escalar(50)};  // hasta el Profesor
    SetTextColor(dc, RGB(235, 241, 250));
    // Pruebas que aun no han llegado y pruebas ya hechas que esperan su nota (estas piden hacer algo).
    int porDelante = 0, esperandoNota = 0;
    {
        Fecha hoy = Fecha::hoy();
        hoy.hora = -1;
        for (const Asignatura& a : g_curso.asignaturas)
            for (const Examen& e : a.examenes)
                if (!e.calificado) (e.fecha.valida() && hoy.diasHasta(e.fecha) < 0 ? esperandoNota : porDelante)++;
    }
    std::wstring datos = std::to_wstring(g_curso.asignaturas.size()) +
                         (g_curso.asignaturas.size() == 1 ? L" asignatura" : L" asignaturas") + L"   ·   " +
                         std::to_wstring(porDelante) + (porDelante == 1 ? L" prueba por delante" : L" pruebas por delante");
    DrawTextW(dc, datos.c_str(), -1, &estadisticas, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (esperandoNota) {
        SIZE medida = {0, 0};
        GetTextExtentPoint32W(dc, datos.c_str(), (int)datos.size(), &medida);
        RECT resto = estadisticas;
        resto.left += medida.cx;
        std::wstring falta = L"   ·   " + std::to_wstring(esperandoNota) + L" esperando su nota";
        SetTextColor(dc, gui::mezclar(t.medio, RGB(255, 255, 255), 0.15));
        if (resto.left < resto.right)
            DrawTextW(dc, falta.c_str(), -1, &resto, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    estadisticas.top = gui::escalar(52); estadisticas.bottom = gui::escalar(75);
    SetTextColor(dc, RGB(164, 184, 210));
    DrawTextW(dc, gui::aW(g_curso.titulo).c_str(), -1, &estadisticas, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(dc, fuente);

    HPEN lapiz = CreatePen(PS_SOLID, 1, t.borde);
    HGDIOBJ lv = SelectObject(dc, lapiz);
    MoveToEx(dc, MARGEN(), cliente.bottom - ALTO_PIE(), NULL);
    LineTo(dc, cliente.right - MARGEN(), cliente.bottom - ALTO_PIE());
    SelectObject(dc, lv);
    DeleteObject(lapiz);

    // Cuantas hay y cuantas tienen una prueba cerca (son las filas resaltadas en ambar)
    int n[3] = {0, 0, 0}, cerca[3] = {0, 0, 0};
    for (const Asignatura& a : g_curso.asignaturas) {
        int c = a.cuatri == 2 ? 2 : 1;
        ++n[c];
        long dias = diasHastaProximaPrueba(a);
        if (dias >= 0 && dias <= g_curso.ajustes.diasDeAviso) ++cerca[c];
    }
    auto pista = [&](int c) {
        if (cerca[c]) return std::to_wstring(cerca[c]) + (cerca[c] == 1 ? L" con prueba cerca" : L" con pruebas cerca");
        return n[c] == 1 ? std::wstring(L"1 asignatura") : std::to_wstring(n[c]) + L" asignaturas";
    };
    gui::tituloConIcono(dc, g_rTitulo1, gui::ICONO_LIBRO, L"Primer cuatrimestre", pista(1), t.acento,
                        cerca[1] ? t.medio : CLR_INVALID);
    gui::tituloConIcono(dc, g_rTitulo2, gui::ICONO_LIBRO, L"Segundo cuatrimestre", pista(2), t.acento,
                        cerca[2] ? t.medio : CLR_INVALID);
    gui::tituloConIcono(dc, g_rTituloFicha, gui::ICONO_FICHA, L"Mi asignatura",
                        asignaturaActual() ? L"Doble clic en la lista para editarla" : L"", t.acento);
    gui::tituloConIcono(dc, g_rTituloExam, gui::ICONO_NOTAS, L"Evaluación y notas",
                        asignaturaActual() && !asignaturaActual()->examenes.empty()
                            ? L"Elige una prueba y pulsa «Poner nota» (o Espacio)" : L"", t.acento);
    gui::tituloConIcono(dc, g_rTituloCal, gui::ICONO_CALENDARIO, L"Calendario académico",
                        L"Pulsa un día  ↓", t.acento);
    gui::tituloConIcono(dc, g_rTituloDia, gui::ICONO_AGENDA, L"Tu agenda del día", L"", t.acento);

    // Rotulo del material, entre los dos grupos de botones de la izquierda
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, gui::mezclar(t.textoSuave, t.fondo, 0.2));
    HGDIOBJ fm = SelectObject(dc, gui::fuenteInterfaz());
    RECT material = g_rMaterial;
    material.left += gui::escalar(26);
    material.top += gui::escalar(4);
    gui::dibujarIcono(dc, gui::ICONO_CARPETA, g_rMaterial.left + gui::escalar(9), (material.top + material.bottom) / 2,
                      gui::escalar(14), gui::mezclar(t.textoSuave, t.fondo, 0.2));
    DrawTextW(dc, L"Material de la asignatura", -1, &material, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, fm);

    RECT panelDia = g_rTituloDia;
    panelDia.top = g_rTituloDia.bottom;
    panelDia.bottom = cliente.bottom - ALTO_PIE() - HUECO();
    if (panelDia.bottom > panelDia.top + 20) {
        gui::panelRedondeado(dc, panelDia, t.panel, t.borde, RADIO);
    }

    RestoreDC(dc, estadoDC);
    if (g_scrollY == 0) pintarBotonesVentana(ventana, dc);
    gui::marcoAlrededorDe(dc, g_listaAsig1, t.borde, RADIO);
    gui::marcoAlrededorDe(dc, g_listaAsig2, t.borde, RADIO);
    gui::marcoAlrededorDe(dc, g_listaExam, t.borde, RADIO);
}

// Fondo alterno (filas pares/impares) y aviso de prueba cercana.
COLORREF fondoDeFila(int fila, bool avisar) {
    const gui::Tema& t = gui::tema();
    if (avisar) return gui::mezclar(t.panel, t.medio, t.oscuro ? 0.07 : 0.08);
    if (fila % 2 == 1) return gui::mezclar(t.panel, t.fondo, 0.45);
    return t.panel;
}

// Dibujo completo de la celda: mismo contraste con y sin foco, sin rejilla nativa.
LRESULT pintarCelda(NMLVCUSTOMDRAW* cd, COLORREF texto, bool aviso, bool asignatura,
                    COLORREF pastilla = CLR_INVALID) {
    HWND lista = cd->nmcd.hdr.hwndFrom;
    int fila = (int)cd->nmcd.dwItemSpec;
    int col = cd->iSubItem;
    bool elegida = (ListView_GetItemState(lista, fila, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    const gui::Tema& t = gui::tema();
    RECT r;
    ListView_GetSubItemRect(lista, fila, col, LVIR_BOUNDS, &r);
    if (col == 0) r.right = r.left + ListView_GetColumnWidth(lista, 0);
    HDC dc = cd->nmcd.hdc;
    int guardado = SaveDC(dc);
    IntersectClipRect(dc, r.left, r.top, r.right, r.bottom);
    COLORREF fondo = elegida ? gui::mezclar(t.panel, t.acento, t.oscuro ? 0.25 : 0.14)
                            : fondoDeFila(fila, aviso);
    HBRUSH b = CreateSolidBrush(fondo);
    FillRect(dc, &r, b); DeleteObject(b);
    if ((elegida || aviso) && col == 0) {
        // Barrita a la izquierda: acento si esta elegida, ambar si la prueba se acerca.
        RECT marca = r; marca.right = marca.left + gui::escalar(3);
        b = CreateSolidBrush(elegida ? t.acento : t.medio); FillRect(dc, &marca, b); DeleteObject(b);
    }
    wchar_t contenido[1024] = {};
    ListView_GetItemText(lista, fila, col, contenido, 1024);
    InflateRect(&r, -gui::escalar(8), 0);
    if (asignatura && col == 1) {
        COLORREF color = gui::colorDeAsignatura(g_curso.asignaturas[cd->nmcd.lItemlParam].color,
                                                (std::size_t)cd->nmcd.lItemlParam);
        RECT punto = {r.left, (r.top + r.bottom) / 2 - 3, r.left + 6, (r.top + r.bottom) / 2 + 3};
        gui::panelRedondeado(dc, punto, color, color, 6);
        r.left += gui::escalar(14);
    }
    SetBkMode(dc, TRANSPARENT);
    if (pastilla != CLR_INVALID && contenido[0]) {
        gui::pastilla(dc, r.left, (r.top + r.bottom) / 2 - gui::escalar(11), gui::escalar(22), contenido, pastilla);
        RestoreDC(dc, guardado);
        return CDRF_SKIPDEFAULT;
    }
    SetTextColor(dc, elegida ? t.texto : texto);
    SelectObject(dc, elegida ? gui::fuenteNegrita() : gui::fuenteInterfaz());
    DrawTextW(dc, contenido, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(dc, guardado);
    return CDRF_SKIPDEFAULT;
}

// Lista sin filas: un dibujo tenue y una frase que dice que hacer.
void pintarListaVacia(HWND lista, HDC dc, RECT hueco) {
    const gui::Tema& t = gui::tema();
    const auto s = gui::escalar;
    std::wstring titulo, detalle, dibujo;
    gui::Icono reserva = gui::ICONO_NOTAS;
    if (lista == g_listaExam) {
        dibujo = L"notas";
        if (!asignaturaActual()) {
            titulo = L"Sin asignatura elegida";
            detalle = L"Elige una en la lista de la izquierda.";
        } else {
            titulo = L"Todavía no hay pruebas";
            detalle = L"Pulsa «Nueva prueba» para añadir exámenes, prácticas o entregas con su peso en la nota.";
        }
    } else {
        dibujo = L"asignaturas";
        reserva = gui::ICONO_LIBRO;
        titulo = L"Sin asignaturas";
        detalle = L"Añádelas con «Nueva asignatura».";
    }
    int alto = hueco.bottom - hueco.top;
    int lado = alto > s(170) ? s(56) : (alto > s(110) ? s(36) : 0);
    int bloque = lado + s(lado ? 12 : 0) + s(50);
    int y = hueco.top + (alto - bloque) / 2;
    if (lado) {
        RECT r = {(hueco.left + hueco.right - lado) / 2, y, (hueco.left + hueco.right + lado) / 2, y + lado};
        gui::decoracion(dc, dibujo, r, gui::mezclar(t.panel, t.acento, 0.55), 200, reserva);
        y += lado + s(12);
    }
    int guardado = SaveDC(dc);
    SetBkMode(dc, TRANSPARENT);
    RECT r = {hueco.left + s(16), y, hueco.right - s(16), y + s(24)};
    SelectObject(dc, gui::fuenteNegrita());
    SetTextColor(dc, t.texto);
    DrawTextW(dc, titulo.c_str(), -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    r.top = r.bottom;
    r.bottom = hueco.bottom;
    SelectObject(dc, gui::fuenteInterfaz());
    SetTextColor(dc, t.textoSuave);
    DrawTextW(dc, detalle.c_str(), -1, &r, DT_CENTER | DT_WORDBREAK);
    RestoreDC(dc, guardado);
}

// Tapa el hueco bajo la ultima fila (el tema oscuro dibuja ahi las lineas de columna).
LRESULT limpiarHuecoDeLista(NMLVCUSTOMDRAW* cd) {
    HWND lista = cd->nmcd.hdr.hwndFrom;
    RECT cliente;
    GetClientRect(lista, &cliente);
    int filas = ListView_GetItemCount(lista);
    int arriba = 0;
    if (filas > 0) {
        RECT ultima;
        ListView_GetItemRect(lista, filas - 1, &ultima, LVIR_BOUNDS);
        arriba = ultima.bottom;
    } else {
        HWND cabecera = ListView_GetHeader(lista);
        RECT rc = {0, 0, 0, 0};
        if (cabecera) GetWindowRect(cabecera, &rc);
        arriba = rc.bottom - rc.top;
    }
    HBRUSH b = CreateSolidBrush(gui::tema().panel);
    if (arriba < cliente.bottom) {
        RECT hueco = {cliente.left, arriba, cliente.right, cliente.bottom};
        FillRect(cd->nmcd.hdc, &hueco, b);
        if (filas == 0) pintarListaVacia(lista, cd->nmcd.hdc, hueco);
    }
    // ...y la franja a la derecha de la ultima columna.
    int anchoColumnas = 0;
    HWND cabecera = ListView_GetHeader(lista);
    int columnas = cabecera ? Header_GetItemCount(cabecera) : 0;
    for (int i = 0; i < columnas; ++i) anchoColumnas += ListView_GetColumnWidth(lista, i);
    if (columnas > 0 && anchoColumnas < cliente.right) {
        RECT franja = {anchoColumnas, cliente.top, cliente.right, std::min(arriba, (int)cliente.bottom)};
        if (cabecera) {
            RECT rc;
            GetWindowRect(cabecera, &rc);
            franja.top = rc.bottom - rc.top;
        }
        FillRect(cd->nmcd.hdc, &franja, b);
    }
    DeleteObject(b);
    return CDRF_DODEFAULT;
}

LRESULT dibujoPersonalizadoAsignaturas(LPARAM lp) {
    NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lp;
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    if (cd->nmcd.dwDrawStage == CDDS_POSTPAINT) return limpiarHuecoDeLista(cd);

    const std::size_t indice = (std::size_t)cd->nmcd.lItemlParam;
    if (indice >= g_curso.asignaturas.size()) return CDRF_DODEFAULT;
    const Asignatura& a = g_curso.asignaturas[indice];

    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        long dias = diasHastaProximaPrueba(a);
        bool avisar = (dias >= 0 && dias <= g_curso.ajustes.diasDeAviso);
        cd->clrTextBk = fondoDeFila((int)cd->nmcd.dwItemSpec, avisar);
        return CDRF_NOTIFYSUBITEMDRAW;
    }
    if (cd->nmcd.dwDrawStage != (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) return CDRF_DODEFAULT;

    long dias = diasHastaProximaPrueba(a);
    COLORREF color = cd->iSubItem == 0 ? gui::tema().textoSuave : gui::tema().texto;
    if (cd->iSubItem == 2 && a.tieneNotas()) color = gui::colorDeNota(a.notaMediaActual());
    return pintarCelda(cd, color, dias >= 0 && dias <= g_curso.ajustes.diasDeAviso, true);
}

LRESULT dibujoPersonalizadoExamenes(LPARAM lp) {
    NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lp;
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    if (cd->nmcd.dwDrawStage == CDDS_POSTPAINT) return limpiarHuecoDeLista(cd);

    Asignatura* a = asignaturaActual();
    const std::size_t indice = (std::size_t)cd->nmcd.lItemlParam;
    if (!a || indice >= a->examenes.size()) return CDRF_DODEFAULT;
    const Examen& e = a->examenes[indice];
    const gui::Tema& t = gui::tema();

    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        Fecha hoy = Fecha::hoy();
        hoy.hora = -1;
        long dias = hoy.diasHasta(e.fecha);
        bool avisar = (!e.calificado && dias >= 0 && dias <= g_curso.ajustes.diasDeAviso);
        cd->clrTextBk = fondoDeFila((int)cd->nmcd.dwItemSpec, avisar);
        return CDRF_NOTIFYSUBITEMDRAW;
    }
    if (cd->nmcd.dwDrawStage != (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) return CDRF_DODEFAULT;

    if (!e.calificado) {
        cd->clrText = t.textoSuave;
    } else if (cd->iSubItem == 3 || cd->iSubItem == 4) {
        cd->clrText = gui::colorDeNota(e.nota);
    } else {
        cd->clrText = t.texto;
    }
    Fecha hoy = Fecha::hoy(); hoy.hora = -1;
    long dias = hoy.diasHasta(e.fecha);
    COLORREF pastilla = CLR_INVALID;  // estado de las pruebas sin nota, como etiqueta de color
    if (cd->iSubItem == 4 && !e.calificado && e.fecha.valida())
        pastilla = dias < 0 ? t.medio : dias <= 2 ? t.mal : dias <= g_curso.ajustes.diasDeAviso ? t.acento : t.textoSuave;
    return pintarCelda(cd, cd->clrText,
                       !e.calificado && dias >= 0 && dias <= g_curso.ajustes.diasDeAviso, false, pastilla);
}

void seleccionCambiadaEnLista(HWND lista, NMLISTVIEW* nlv) {
    if (!(nlv->uNewState & LVIS_SELECTED) || (nlv->uOldState & LVIS_SELECTED)) return;
    LPARAM indice = gui::datosDeFila(lista, nlv->iItem);
    if (indice < 0 || (std::size_t)indice >= g_curso.asignaturas.size()) return;

    g_indiceSel = (int)indice;
    limpiarSeleccion(lista == g_listaAsig1 ? g_listaAsig2 : g_listaAsig1);
    refrescarExamenes();
}

LRESULT CALLBACK procPrincipal(HWND ventana, UINT mensaje, WPARAM wp, LPARAM lp) {
    switch (mensaje) {
        case WM_CREATE:
            g_principal = ventana;
            crearControles(ventana);
            return 0;

        case WM_SIZE:
            colocarControles(ventana);
            return 0;

        case WM_NCCALCSIZE:
            // Sin la barra de titulo de Windows: la barra negra de la aplicacion hace de barra.
            if (wp) {
                NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lp;
                LONG arriba = p->rgrc[0].top;
                DefWindowProcW(ventana, mensaje, wp, lp);
                p->rgrc[0].top = arriba;
                if (IsZoomed(ventana)) p->rgrc[0].top += GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                return 0;
            }
            break;

        case WM_NCHITTEST: {
            LRESULT r = DefWindowProcW(ventana, mensaje, wp, lp);
            if (r != HTCLIENT) return r;
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            ScreenToClient(ventana, &pt);
            if (!IsZoomed(ventana) && pt.y < gui::escalar(5)) return HTTOP;
            if (g_scrollY == 0 && botonVentanaEn(ventana, pt)) return HTCLIENT;
            if (pt.y + g_scrollY < ALTO_BARRA()) return HTCAPTION;  // arrastrar; doble clic maximiza
            return HTCLIENT;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            int encima = g_scrollY == 0 ? botonVentanaEn(ventana, pt) : 0;
            if (encima != g_encimaVentana) {
                g_encimaVentana = encima;
                RECT r = {0, 0, 0, 0};
                GetClientRect(ventana, &r);
                r.bottom = gui::escalar(34);
                InvalidateRect(ventana, &r, FALSE);
                TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, ventana, 0};
                TrackMouseEvent(&t);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
        case WM_NCMOUSEMOVE:
            if (g_encimaVentana) {
                g_encimaVentana = 0;
                InvalidateRect(ventana, NULL, FALSE);
            }
            break;

        case WM_LBUTTONUP: {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            switch (g_scrollY == 0 ? botonVentanaEn(ventana, pt) : 0) {
                case 1: ShowWindow(ventana, SW_MINIMIZE); break;
                case 2: ShowWindow(ventana, IsZoomed(ventana) ? SW_RESTORE : SW_MAXIMIZE); break;
                case 3: PostMessageW(ventana, WM_CLOSE, 0, 0); break;
                default: break;
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* info = (MINMAXINFO*)lp;
            info->ptMinTrackSize.x = 1000;
            info->ptMinTrackSize.y = 640;
            return 0;
        }

        case WM_HSCROLL:
        case WM_VSCROLL: {
            int barra = mensaje == WM_HSCROLL ? SB_HORZ : SB_VERT;
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
            GetScrollInfo(ventana, barra, &si);
            int posicion = si.nPos;
            switch (LOWORD(wp)) {
                case SB_LINEUP: posicion -= gui::escalar(32); break;
                case SB_LINEDOWN: posicion += gui::escalar(32); break;
                case SB_PAGEUP: posicion -= si.nPage; break;
                case SB_PAGEDOWN: posicion += si.nPage; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: posicion = si.nTrackPos; break;
                case SB_TOP: posicion = 0; break;
                case SB_BOTTOM: posicion = si.nMax; break;
                default: return 0;
            }
            si.fMask = SIF_POS; si.nPos = posicion;
            SetScrollInfo(ventana, barra, &si, TRUE);
            if (barra == SB_HORZ) g_scrollX = GetScrollPos(ventana, barra);
            else g_scrollY = GetScrollPos(ventana, barra);
            colocarControles(ventana);
            return 0;
        }
        case WM_MOUSEWHEEL:
            SendMessageW(ventana, WM_VSCROLL,
                         GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            return 0;

        case WM_ERASEBKGND: {
            RECT r;
            GetClientRect(ventana, &r);
            FillRect((HDC)wp, &r, gui::brochaFondo());
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(ventana, &ps);
            pintarVentana(ventana, dc);
            EndPaint(ventana, &ps);
            return 0;
        }
        case WM_PRINTCLIENT:
            pintarVentana(ventana, (HDC)wp);
            return 0;

        case WM_CTLCOLORSTATIC: {
            HWND control = (HWND)lp;
            if (control == g_tituloApp || control == g_subtituloApp) {
                SetBkColor((HDC)wp, gui::colorBarraSuperior());
                SetTextColor((HDC)wp, control == g_tituloApp ? RGB(255, 255, 255) : RGB(164, 184, 210));
                return (LRESULT)brochaBarra();
            }
            if (control == g_infoDia) {
                SetBkColor((HDC)wp, gui::tema().panel);
                SetTextColor((HDC)wp, gui::tema().texto);
                return (LRESULT)gui::brochaPanel();
            }
            SetBkColor((HDC)wp, gui::tema().fondo);
            SetTextColor((HDC)wp, gui::tema().textoSuave);
            return (LRESULT)gui::brochaFondo();
        }

        case WM_DRAWITEM:
            gui::dibujarBoton((DRAWITEMSTRUCT*)lp);
            return TRUE;

        case agenda::WM_AGENDA_ELEGIDA:  // clic en una fila de la agenda: abre esa asignatura
            if ((int)wp != g_indiceSel) {
                seleccionarAsignatura((int)wp);
                g_indiceSel = (int)wp;
                refrescarExamenes();
            }
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (HIWORD(wp) == cal::CAL_DIA_ELEGIDO && id == ID_CALENDARIO) {
                accionDiaElegido();
                return 0;
            }
            switch (id) {
                case ID_BTN_NUEVA_ASIG: accionNuevaAsignatura(); return 0;
                case ID_BTN_EDITAR_ASIG: accionEditarAsignatura(); return 0;
                case ID_BTN_DUPLICAR_ASIG: accionDuplicarAsignatura(); return 0;
                case ID_BTN_BORRAR_ASIG: accionBorrarAsignatura(); return 0;
                case ID_BTN_CARPETA: accionAbrirCarpeta(0); return 0;
                case ID_BTN_TEMARIO: accionAbrirCarpeta(1); return 0;
                case ID_BTN_PRACTICAS: accionAbrirCarpeta(2); return 0;
                case ID_BTN_NUEVA_PRUEBA: accionNuevaPrueba(); return 0;
                case ID_BTN_NOTA: accionPonerNota(); return 0;
                case ID_BTN_DUPLICAR_PRUEBA: accionDuplicarPrueba(); return 0;
                case ID_BTN_EDITAR_PRUEBA: accionEditarPrueba(); return 0;
                case ID_BTN_BORRAR_PRUEBA: accionBorrarPrueba(); return 0;
                case ID_BTN_EXPORTAR: accionExportarHtml(); return 0;
                case ID_BTN_TEMA: accionCambiarTema(); return 0;
                case ID_BTN_AJUSTES: accionAjustes(); return 0;
                case ID_BTN_GUIA: abrirGuia(ventana); return 0;
                case ID_BTN_PROFESOR:
                    if (asignaturaActual()) {
                        profesor::abrir(ventana, *asignaturaActual(), g_curso.asignaturas);
                        InvalidateRect(g_resumen, NULL, TRUE);  // el profesor actualiza el resumen de estudio
                    }
                    return 0;
                case ID_ACC_GUARDAR:
                    guardar();
                    return 0;
                case ID_ACC_MARCAR_HECHA: accionAlternarHecha(); return 0;
                case ID_ACC_REFRESCAR:
                    refrescarAsignaturas(g_indiceSel);
                    return 0;
                default: break;
            }
            break;
        }

        case WM_NOTIFY: {
            NMHDR* cab = (NMHDR*)lp;
            if (!cab) break;

            if (cab->code == (UINT)HDN_BEGINTRACKW || cab->code == (UINT)HDN_BEGINTRACKA ||
                cab->code == (UINT)HDN_DIVIDERDBLCLICKW ||
                cab->code == (UINT)HDN_DIVIDERDBLCLICKA) {
                return TRUE;
            }

            if (cab->code == NM_CUSTOMDRAW) {
                if (cab->idFrom == ID_LISTA_ASIG1 || cab->idFrom == ID_LISTA_ASIG2) {
                    return dibujoPersonalizadoAsignaturas(lp);
                }
                if (cab->idFrom == ID_LISTA_EXAM) return dibujoPersonalizadoExamenes(lp);
            }

            if (cab->code == LVN_ITEMCHANGED) {
                if (cab->idFrom == ID_LISTA_ASIG1) {
                    seleccionCambiadaEnLista(g_listaAsig1, (NMLISTVIEW*)lp);
                    return 0;
                }
                if (cab->idFrom == ID_LISTA_ASIG2) {
                    seleccionCambiadaEnLista(g_listaAsig2, (NMLISTVIEW*)lp);
                    return 0;
                }
                if (cab->idFrom == ID_LISTA_EXAM) {
                    NMLISTVIEW* nlv = (NMLISTVIEW*)lp;
                    if ((nlv->uNewState & LVIS_SELECTED) && !(nlv->uOldState & LVIS_SELECTED)) {
                        Examen* e = examenActual();
                        if (e) {
                            cal::seleccionar(g_calendario, e->fecha);
                            mostrarExamenesDelDia(e->fecha);
                        }
                        actualizarBotones();
                    }
                    else if ((nlv->uOldState & LVIS_SELECTED) && !(nlv->uNewState & LVIS_SELECTED)) {
                        actualizarBotones();
                    }
                    return 0;
                }
            }

            if (cab->code == NM_DBLCLK) {
                if (cab->idFrom == ID_LISTA_EXAM) accionEditarPrueba();
                if (cab->idFrom == ID_LISTA_ASIG1 || cab->idFrom == ID_LISTA_ASIG2) {
                    accionEditarAsignatura();
                }
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            guardar();
            DestroyWindow(ventana);
            return 0;

        case WM_SETTINGCHANGE:  // con «Como Windows», seguir al sistema cuando cambia de modo
            if (g_curso.ajustes.temaSistema && lp && lstrcmpiW((LPCWSTR)lp, L"ImmersiveColorSet") == 0 &&
                gui::sistemaEnOscuro() != gui::tema().oscuro) {
                aplicarTema();
                colocarControles(ventana);
                RedrawWindow(ventana, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(ventana, mensaje, wp, lp);
}

// Tabla de atajos de teclado (sin fichero de recursos).
HACCEL crearAtajos() {
    ACCEL a[16];
    ZeroMemory(a, sizeof(a));
    int n = 0;
    a[n].fVirt = FCONTROL | FVIRTKEY; a[n].key = 'N'; a[n].cmd = ID_BTN_NUEVA_ASIG; ++n;
    a[n].fVirt = FCONTROL | FVIRTKEY; a[n].key = 'D'; a[n].cmd = ID_BTN_DUPLICAR_ASIG; ++n;
    a[n].fVirt = FCONTROL | FVIRTKEY; a[n].key = 'P'; a[n].cmd = ID_BTN_NUEVA_PRUEBA; ++n;
    a[n].fVirt = FCONTROL | FVIRTKEY; a[n].key = 'E'; a[n].cmd = ID_BTN_EXPORTAR; ++n;
    a[n].fVirt = FCONTROL | FVIRTKEY; a[n].key = 'T'; a[n].cmd = ID_BTN_TEMA; ++n;
    a[n].fVirt = FCONTROL | FVIRTKEY; a[n].key = 'S'; a[n].cmd = ID_ACC_GUARDAR; ++n;
    a[n].fVirt = FVIRTKEY; a[n].key = VK_F2; a[n].cmd = ID_BTN_EDITAR_ASIG; ++n;
    a[n].fVirt = FVIRTKEY; a[n].key = VK_F3; a[n].cmd = ID_BTN_EDITAR_PRUEBA; ++n;
    a[n].fVirt = FVIRTKEY; a[n].key = VK_F5; a[n].cmd = ID_ACC_REFRESCAR; ++n;
    a[n].fVirt = FVIRTKEY; a[n].key = VK_F1; a[n].cmd = ID_BTN_GUIA; ++n;
    a[n].fVirt = FVIRTKEY; a[n].key = VK_DELETE; a[n].cmd = ID_BTN_BORRAR_PRUEBA; ++n;
    a[n].fVirt = FVIRTKEY; a[n].key = VK_SPACE; a[n].cmd = ID_ACC_MARCAR_HECHA; ++n;
    return CreateAcceleratorTableW(a, n);
}

}  // namespace

// ---------------------------------------------------------------------- main

int WINAPI WinMain(HINSTANCE instancia, HINSTANCE, LPSTR, int mostrar) {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    Gdiplus::GdiplusStartupInput entradaGdi;
    ULONG_PTR fichaGdi = 0;
    Gdiplus::GdiplusStartup(&fichaGdi, &entradaGdi, NULL);

    std::string carpeta = rutas::directorioDelEjecutable();
    std::string fichero = rutas::unir(carpeta, "datos/asignaturas.json");
    Almacen almacen(fichero);
    g_almacen = &almacen;

    if (!almacen.cargar(g_curso)) {
        MessageBoxW(NULL,
                    (L"No se pudo leer el fichero de datos:\n" + gui::aW(almacen.ultimoError()) +
                     L"\n\nSe abrirá con los datos iniciales.")
                        .c_str(),
                    L"Gestor de Asignaturas", MB_OK | MB_ICONWARNING);
        g_curso = Almacen::cursoDeEjemplo();
    }

    ajustes::aplicarApariencia(g_curso);
    gui::modoOscuroDelSistema(gui::tema().oscuro);
    prepararCarpetasDelCurso(g_curso);

    HICON icono = (HICON)LoadImageW(instancia, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0,
                                    LR_DEFAULTSIZE | LR_SHARED);
    if (!icono) icono = LoadIconW(NULL, IDI_APPLICATION);

    WNDCLASSEXW clase;
    ZeroMemory(&clase, sizeof(clase));
    clase.cbSize = sizeof(clase);
    clase.style = CS_HREDRAW | CS_VREDRAW;
    clase.lpfnWndProc = procPrincipal;
    clase.hInstance = instancia;
    clase.hIcon = icono;
    clase.hIconSm = icono;
    clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
    clase.hbrBackground = NULL;
    clase.lpszClassName = kClasePrincipal;
    if (!RegisterClassExW(&clase)) {
        MessageBoxW(NULL, L"No se pudo registrar la ventana.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT areaTrabajo = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &areaTrabajo, 0);
    int anchoInicial = (int)areaTrabajo.right - areaTrabajo.left;
    int altoInicial = (int)areaTrabajo.bottom - areaTrabajo.top;
    if (anchoInicial > 1500) anchoInicial = 1500;
    if (altoInicial > 940) altoInicial = 940;
    HWND ventana = CreateWindowExW(0, kClasePrincipal, L"UNIVERSALIS", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                   CW_USEDEFAULT, CW_USEDEFAULT, anchoInicial, altoInicial, NULL, NULL, instancia,
                                   NULL);
    if (!ventana) {
        MessageBoxW(NULL, L"No se pudo crear la ventana.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    gui::barraDeTituloOscura(ventana);
    SetWindowPos(ventana, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    aplicarTema();
    InvalidateRect(g_infoDia, NULL, FALSE);
    cal::ponerOpciones(g_calendario, g_curso.ajustes.primerDiaLunes,
                       g_curso.ajustes.mostrarFinDeSemana, g_curso.ajustes.diasDeAviso);

    refrescarAsignaturas(0);
    Fecha hoy = Fecha::hoy();
    hoy.hora = -1;
    cal::seleccionar(g_calendario, hoy);
    mostrarExamenesDelDia(hoy);

    g_atajos = crearAtajos();
    ShowWindow(ventana, mostrar);
    UpdateWindow(ventana);
    guiaSiEsLaPrimeraVez(ventana);

    // gestor_gui.exe --profesor CODIGO  abre directamente el profesor de esa asignatura.
    int numArgs = 0;
    LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
    if (args) {
        // gestor_gui.exe --ajustes  abre directamente los ajustes.
        for (int i = 1; i < numArgs; ++i)
            if (std::wstring(args[i]) == L"--ajustes") accionAjustes();
        for (int i = 1; i + 1 < numArgs; ++i) {
            if (std::wstring(args[i]) != L"--profesor") continue;
            std::string codigo = gui::aUtf8(args[i + 1]);
            for (std::size_t k = 0; k < g_curso.asignaturas.size(); ++k) {
                if (g_curso.asignaturas[k].codigo == codigo) {
                    profesor::abrir(ventana, g_curso.asignaturas[k], g_curso.asignaturas);
                    break;
                }
            }
        }
        LocalFree(args);
    }

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (g_atajos && TranslateAcceleratorW(ventana, g_atajos, &msg)) continue;
        if (!IsDialogMessageW(ventana, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (g_atajos) DestroyAcceleratorTable(g_atajos);
    return (int)msg.wParam;
}
