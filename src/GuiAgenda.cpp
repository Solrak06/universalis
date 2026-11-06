// GuiAgenda.cpp - «Tu agenda del dia» dibujada (ver GuiAgenda.hpp).
#include "GuiAgenda.hpp"

#include <algorithm>
#include <cwctype>

namespace agenda {

namespace {

const wchar_t* kClase = L"UniversalisAgenda";

struct Estado {
    Dia dia;
    int scroll = 0;
    int alto = 0;  // alto de todo el contenido
    int encima = -1;  // fila bajo el raton (indice global de linea)
    std::vector<std::pair<RECT, int> > filas;  // rectangulo (sin desplazar) y asignatura
};

int s(int px) { return gui::escalar(px); }

// Letra pequena (detalles y titulos de seccion), derivada de la de la interfaz.
HFONT fuentePeque() {
    static HFONT f = NULL;
    static HFONT base = NULL;
    if (!f || base != gui::fuenteInterfaz()) {
        if (f) DeleteObject(f);
        f = NULL;
        base = gui::fuenteInterfaz();
        LOGFONTW lf;
        ZeroMemory(&lf, sizeof(lf));
        if (GetObjectW(base, sizeof(lf), &lf)) {
            lf.lfHeight = (LONG)(lf.lfHeight * 0.86);
            f = CreateFontIndirectW(&lf);
        }
        if (!f) f = base;
    }
    return f;
}

int anchoDe(HDC dc, HFONT f, const std::wstring& t) {
    HGDIOBJ v = SelectObject(dc, f);
    SIZE m = SIZE();
    GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &m);
    SelectObject(dc, v);
    return m.cx;
}

void escribir(HDC dc, HFONT f, COLORREF c, const std::wstring& t, RECT r, UINT formato) {
    HGDIOBJ v = SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, t.c_str(), -1, &r, formato | DT_NOPREFIX);
    SelectObject(dc, v);
}

// Pastilla con fondo tenue del color y texto del mismo color. Devuelve su ancho.
int pildora(HDC dc, int derecha, int cy, const std::wstring& t, COLORREF c) {
    const gui::Tema& tm = gui::tema();
    HFONT f = fuentePeque();
    int w = anchoDe(dc, f, t) + s(16), h = s(22);
    RECT r = {derecha - w, cy - h / 2, derecha, cy - h / 2 + h};
    gui::panelRedondeado(dc, r, gui::mezclar(tm.panel, c, tm.oscuro ? 0.18 : 0.12), gui::mezclar(tm.panel, c, 0.45), h / 2);
    escribir(dc, f, tm.oscuro ? gui::mezclar(c, RGB(255, 255, 255), 0.25) : gui::mezclar(c, RGB(0, 0, 0), 0.2), t, r,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return w;
}

void pintar(HWND h, HDC destino) {
    Estado* e = (Estado*)GetWindowLongPtrW(h, GWLP_USERDATA);
    RECT c;
    GetClientRect(h, &c);
    HDC dc = CreateCompatibleDC(destino);
    HBITMAP lienzo = CreateCompatibleBitmap(destino, std::max(1L, c.right), std::max(1L, c.bottom));
    HGDIOBJ viejo = SelectObject(dc, lienzo);
    const gui::Tema& tm = gui::tema();
    HBRUSH fondo = CreateSolidBrush(tm.panel);
    FillRect(dc, &c, fondo);
    DeleteObject(fondo);
    SetBkMode(dc, TRANSPARENT);
    if (!e) {
        BitBlt(destino, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, viejo);
        DeleteObject(lienzo);
        DeleteDC(dc);
        return;
    }
    e->filas.clear();
    // Si no cabe todo, se deja sitio a la derecha para la barrita de desplazamiento
    const int w = c.right - (e->alto > c.bottom ? s(10) : 0);
    int y = -e->scroll;

    // Cabecera: fecha y cuanto falta
    {
        int ew = 0;
        if (!e->dia.etiqueta.empty())
            ew = pildora(dc, w, y + s(14), e->dia.etiqueta, e->dia.colorEtiqueta == CLR_INVALID ? tm.acento : e->dia.colorEtiqueta);
        RECT r = {0, y, w - ew - s(8), y + s(28)};
        escribir(dc, gui::fuenteNegrita(), tm.texto, e->dia.fecha, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += s(38);
    }

    if (e->dia.secciones.empty()) {
        RECT r = {0, y + s(10), w, y + s(80)};
        escribir(dc, gui::fuenteInterfaz(), tm.textoSuave, e->dia.vacio, r, DT_LEFT | DT_WORDBREAK);
        y += s(80);
    }

    int indice = 0;
    for (const Seccion& sec : e->dia.secciones) {
        // Titulo de la seccion con su cuenta
        RECT rt = {0, y, w, y + s(22)};
        std::wstring titulo = sec.titulo;
        for (wchar_t& ch : titulo) ch = (wchar_t)towupper(ch);
        escribir(dc, fuentePeque(), tm.textoSuave, titulo, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int tw = anchoDe(dc, fuentePeque(), titulo);
        HPEN lapiz = CreatePen(PS_SOLID, 1, gui::mezclar(tm.panel, tm.borde, 0.7));
        HGDIOBJ lv = SelectObject(dc, lapiz);
        MoveToEx(dc, tw + s(10), y + s(11), NULL);
        LineTo(dc, w, y + s(11));
        SelectObject(dc, lv);
        DeleteObject(lapiz);
        y += s(26);

        for (const Linea& l : sec.lineas) {
            int alto = l.detalle.empty() ? s(34) : s(48);
            RECT fila = {-s(6), y, w + s(2), y + alto - s(4)};
            if (l.asignatura >= 0 && e->encima == indice)
                gui::panelRedondeado(dc, fila, tm.panelAlt, tm.panelAlt, s(10));
            COLORREF franja = l.color == CLR_INVALID ? tm.acento : l.color;
            RECT barra = {0, fila.top + s(7), s(4), fila.bottom - s(7)};
            gui::panelRedondeado(dc, barra, franja, franja, s(4));
            int derecha = w - s(4);
            if (!l.pastilla.empty())
                derecha -= pildora(dc, derecha, (fila.top + fila.bottom) / 2, l.pastilla,
                                   l.colorPastilla == CLR_INVALID ? franja : l.colorPastilla) + s(8);
            int tx = s(14);
            if (l.detalle.empty()) {
                RECT r = {tx, fila.top, derecha, fila.bottom};
                escribir(dc, gui::fuenteInterfaz(), tm.texto, l.titulo, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else {
                RECT r1 = {tx, fila.top + s(3), derecha, fila.top + s(23)};
                escribir(dc, gui::fuenteNegrita(), tm.texto, l.titulo, r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                RECT r2 = {tx, fila.top + s(23), derecha, fila.bottom - s(2)};
                escribir(dc, fuentePeque(), tm.textoSuave, l.detalle, r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            RECT sinDesplazar = fila;
            OffsetRect(&sinDesplazar, 0, e->scroll);
            e->filas.push_back(std::make_pair(sinDesplazar, l.asignatura));
            ++indice;
            y += alto;
        }
        if (!sec.pie.empty()) {
            RECT r = {s(14), y, w, y + s(22)};
            escribir(dc, fuentePeque(), tm.medio, sec.pie, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += s(24);
        }
        y += s(10);
    }
    e->alto = y + e->scroll;
    if ((e->alto > c.bottom) != (w != c.right)) InvalidateRect(h, NULL, FALSE);  // cambia el sitio de la barrita

    // Indicador de desplazamiento
    if (e->alto > c.bottom) {
        int pista = c.bottom - s(8), largo = std::max(s(24), pista * (int)c.bottom / e->alto);
        int maximo = e->alto - c.bottom;
        int pos = (pista - largo) * e->scroll / std::max(1, maximo);
        RECT r = {c.right - s(3), s(4) + pos, c.right, s(4) + pos + largo};
        COLORREF col = gui::mezclar(tm.panel, tm.textoSuave, 0.4);
        gui::panelRedondeado(dc, r, col, col, s(2));
    }

    BitBlt(destino, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, viejo);
    DeleteObject(lienzo);
    DeleteDC(dc);
}

int filaEn(Estado* e, POINT pt) {
    pt.y += e->scroll;
    for (std::size_t i = 0; i < e->filas.size(); ++i)
        if (PtInRect(&e->filas[i].first, pt)) return (int)i;
    return -1;
}

LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Estado* e = (Estado*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR) new Estado());
            return 0;
        case WM_DESTROY:
            delete e;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PRINTCLIENT: pintar(h, (HDC)wp); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            pintar(h, dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SIZE:
            if (e) {
                RECT c;
                GetClientRect(h, &c);
                e->scroll = std::max(0, std::min(e->scroll, e->alto - (int)c.bottom));
            }
            InvalidateRect(h, NULL, FALSE);
            return 0;
        case WM_MOUSEWHEEL:
            if (e) {
                RECT c;
                GetClientRect(h, &c);
                int maximo = std::max(0, e->alto - (int)c.bottom);
                int nuevo = std::max(0, std::min(maximo, e->scroll - GET_WHEEL_DELTA_WPARAM(wp) * s(60) / WHEEL_DELTA));
                if (nuevo != e->scroll) {
                    e->scroll = nuevo;
                    InvalidateRect(h, NULL, FALSE);
                }
            }
            return 0;
        case WM_MOUSEMOVE:
            if (e) {
                POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
                int f = filaEn(e, pt);
                if (f >= 0 && e->filas[(std::size_t)f].second < 0) f = -1;
                if (f != e->encima) {
                    e->encima = f;
                    InvalidateRect(h, NULL, FALSE);
                }
                TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
                TrackMouseEvent(&t);
            }
            return 0;
        case WM_MOUSELEAVE:
            if (e && e->encima >= 0) {
                e->encima = -1;
                InvalidateRect(h, NULL, FALSE);
            }
            return 0;
        case WM_SETCURSOR:
            if (e && e->encima >= 0) {
                SetCursor(LoadCursorW(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        case WM_LBUTTONUP:
            if (e) {
                POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
                int f = filaEn(e, pt);
                if (f >= 0 && e->filas[(std::size_t)f].second >= 0)
                    PostMessageW(GetParent(h), WM_AGENDA_ELEGIDA, (WPARAM)e->filas[(std::size_t)f].second, 0);
            }
            return 0;
        default: break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

HWND crear(HWND padre, int id) {
    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW clase;
        ZeroMemory(&clase, sizeof(clase));
        clase.cbSize = sizeof(clase);
        clase.lpfnWndProc = proc;
        clase.hInstance = GetModuleHandleW(NULL);
        clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
        clase.lpszClassName = kClase;
        RegisterClassExW(&clase);
        registrada = true;
    }
    return CreateWindowExW(0, kClase, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, padre, (HMENU)(INT_PTR)id,
                           GetModuleHandleW(NULL), NULL);
}

void poner(HWND agenda, const Dia& dia) {
    Estado* e = (Estado*)GetWindowLongPtrW(agenda, GWLP_USERDATA);
    if (!e) return;
    e->dia = dia;
    e->scroll = 0;
    e->encima = -1;
    InvalidateRect(agenda, NULL, FALSE);
}

}  // namespace agenda
