#include "GuiCalendario.hpp"

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

namespace cal {

namespace {

const wchar_t* kClase = L"GestorAsignaturasCalendario";

int ALTO_CABECERA() { return gui::escalar(54); }  // titulo del mes con sus botones al lado
int ALTO_SEMANA() { return gui::escalar(28); }
int MARGEN() { return gui::escalar(10); }
int LADO_FLECHA() { return gui::escalar(30); }
int ANCHO_HOY() { return gui::escalar(54); }

struct Estado {
    const Curso* curso = NULL;
    int anio = 2026;
    int mes = 1;
    int diaSeleccionado = 0;  // 0 = ninguno
    int diaEncima = 0;        // dia bajo el raton
    bool lunesPrimero = true;
    bool verFinDeSemana = true;
    int diasDeAviso = 7;
};

Estado* estadoDe(HWND h) {
    return reinterpret_cast<Estado*>(GetWindowLongPtrW(h, GWLP_USERDATA));
}

// Cuantas columnas se pintan (7 con fin de semana, 5 sin el).
int columnas(const Estado* e) { return (e && !e->verFinDeSemana) ? 5 : 7; }

// Columna en pantalla de un dia de la semana (0=lunes ... 6=domingo).
// Devuelve -1 si ese dia esta oculto.
int columnaDe(const Estado* e, int diaSemana) {
    int col = (e && !e->lunesPrimero) ? (diaSemana + 1) % 7 : diaSemana;
    if (e && !e->verFinDeSemana) {
        if (diaSemana >= 5) return -1;                 // sabado y domingo fuera
        col = (e->lunesPrimero) ? diaSemana : diaSemana;  // L-V siempre en orden
    }
    return col;
}

// En el calendario SOLO se marcan las pruebas evaluables.
struct MarcaDia {
    std::vector<COLORREF> examenes;   // colores de las asignaturas con prueba
    std::vector<std::wstring> textos; // "PROG2 Parcial 1"
};

// Pruebas de un dia concreto (del mes mostrado o de los meses vecinos).
MarcaDia marcasDe(const Estado* e, int anio, int mes, int dia) {
    MarcaDia m;
    if (!e || !e->curso) return m;

    for (std::size_t i = 0; i < e->curso->asignaturas.size(); ++i) {
        const Asignatura& a = e->curso->asignaturas[i];
        for (std::size_t j = 0; j < a.examenes.size(); ++j) {
            const Fecha& fe = a.examenes[j].fecha;
            if (fe.anio == anio && fe.mes == mes && fe.dia == dia) {
                m.examenes.push_back(gui::colorDeAsignatura(a.color, i));
                m.textos.push_back(gui::aW(a.codigo) + L"  " + gui::aW(a.examenes[j].nombre));
                break;
            }
        }
    }
    return m;
}

RECT rectFlecha(HWND h, bool izquierda) {
    RECT cliente;
    GetClientRect(h, &cliente);
    RECT r;
    r.top = gui::escalar(24) - LADO_FLECHA() / 2;  // a la altura del titulo
    r.bottom = r.top + LADO_FLECHA();
    if (izquierda) {
        r.right = cliente.right - MARGEN() - LADO_FLECHA() - 6 - ANCHO_HOY() - 8;
        r.left = r.right - LADO_FLECHA();
    } else {
        r.right = cliente.right - MARGEN() - ANCHO_HOY() - 8;
        r.left = r.right - LADO_FLECHA();
    }
    return r;
}

RECT rectHoy(HWND h) {
    RECT cliente;
    GetClientRect(h, &cliente);
    RECT r;
    r.top = gui::escalar(24) - LADO_FLECHA() / 2;
    r.bottom = r.top + LADO_FLECHA();
    r.right = cliente.right - MARGEN();
    r.left = r.right - ANCHO_HOY();
    return r;
}

RECT rectCelda(HWND h, int columna, int fila) {
    RECT cliente;
    GetClientRect(h, &cliente);
    int ancho = cliente.right - 2 * MARGEN();
    int altoRejilla = cliente.bottom - ALTO_CABECERA() - ALTO_SEMANA() - MARGEN();
    int totalColumnas = columnas(estadoDe(h));
    int anchoCelda = ancho / totalColumnas;
    int altoCelda = altoRejilla / 6;

    RECT r;
    r.left = MARGEN() + columna * anchoCelda;
    r.top = ALTO_CABECERA() + ALTO_SEMANA() + fila * altoCelda;
    r.right = r.left + anchoCelda;
    r.bottom = r.top + altoCelda;
    return r;
}

void avisarAlPadre(HWND h) {
    HWND padre = GetParent(h);
    if (!padre) return;
    SendMessageW(padre, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), CAL_DIA_ELEGIDO), (LPARAM)h);
}

void mesAnterior(int& anio, int& mes) {
    if (--mes < 1) {
        mes = 12;
        --anio;
    }
}

void mesSiguiente(int& anio, int& mes) {
    if (++mes > 12) {
        mes = 1;
        ++anio;
    }
}

void dibujarPastillaMes(HDC dc, const RECT& r, const std::wstring& texto, bool resaltado) {
    const gui::Tema& t = gui::tema();
    HBRUSH b = CreateSolidBrush(resaltado ? gui::mezclar(t.panel, t.acento, 0.25)
                                          : gui::mezclar(t.panel, t.fondo, 0.5));
    HPEN p = CreatePen(PS_SOLID, 1, resaltado ? t.acento : t.borde);
    HGDIOBJ bv = SelectObject(dc, b);
    HGDIOBJ pv = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 9, 9);
    SelectObject(dc, bv);
    SelectObject(dc, pv);
    DeleteObject(b);
    DeleteObject(p);

    SetBkMode(dc, TRANSPARENT);
    COLORREF color = resaltado ? t.acento : t.textoSuave;
    if (texto == L"<" || texto == L">") {  // flechas dibujadas
        gui::dibujarIcono(dc, texto == L"<" ? gui::ICONO_ANTERIOR : gui::ICONO_SIGUIENTE, (r.left + r.right) / 2,
                          (r.top + r.bottom) / 2, gui::escalar(18), color);
        return;
    }
    SetTextColor(dc, color);
    HGDIOBJ fv = SelectObject(dc, gui::fuenteNegrita());
    RECT rr = r;
    DrawTextW(dc, texto.c_str(), -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, fv);
}

// Pinta una celda con su numero, sus marcas de examen y sus rayas de clase.
void dibujarCelda(HDC dc, const RECT& celda, int dia, const MarcaDia& marca, bool delMes,
                  bool esHoy, bool elegido, bool encima, bool finDeSemana) {
    const gui::Tema& t = gui::tema();

    RECT caja = celda;
    InflateRect(&caja, -3, -3);

    const bool hayExamen = !marca.examenes.empty();
    COLORREF colorPrincipal = hayExamen ? marca.examenes[0] : t.acento;

    // --- fondo
    if (elegido) {
        HBRUSH b = CreateSolidBrush(gui::mezclar(t.panel, colorPrincipal, 0.42));
        HPEN p = CreatePen(PS_SOLID, 2, colorPrincipal);
        HGDIOBJ bv = SelectObject(dc, b);
        HGDIOBJ pv = SelectObject(dc, p);
        RoundRect(dc, caja.left, caja.top, caja.right, caja.bottom, 10, 10);
        SelectObject(dc, bv);
        SelectObject(dc, pv);
        DeleteObject(b);
        DeleteObject(p);
    } else if (hayExamen && delMes) {
        HBRUSH b = CreateSolidBrush(gui::mezclar(t.panel, colorPrincipal, t.oscuro ? 0.20 : 0.15));
        HGDIOBJ bv = SelectObject(dc, b);
        HGDIOBJ pv = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, caja.left, caja.top, caja.right + 1, caja.bottom + 1, 10, 10);
        SelectObject(dc, bv);
        SelectObject(dc, pv);
        DeleteObject(b);
    } else if (encima && delMes) {
        HBRUSH b = CreateSolidBrush(gui::mezclar(t.panel, t.fondo, 0.6));
        HGDIOBJ bv = SelectObject(dc, b);
        HGDIOBJ pv = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, caja.left, caja.top, caja.right + 1, caja.bottom + 1, 10, 10);
        SelectObject(dc, bv);
        SelectObject(dc, pv);
        DeleteObject(b);
    }

    if (esHoy && !elegido) {
        HPEN p = CreatePen(PS_SOLID, 2, t.acento);
        HGDIOBJ bv = SelectObject(dc, GetStockObject(NULL_BRUSH));
        HGDIOBJ pv = SelectObject(dc, p);
        RoundRect(dc, caja.left, caja.top, caja.right, caja.bottom, 10, 10);
        SelectObject(dc, bv);
        SelectObject(dc, pv);
        DeleteObject(p);
    }

    // --- numero del dia, arriba a la izquierda
    wchar_t texto[8];
    _snwprintf(texto, 7, L"%d", dia);
    texto[7] = L'\0';

    // En celdas bajas el numero va centrado y los puntos de las pruebas justo debajo,
    // sin pisarlo; en celdas grandes, arriba a la izquierda con el nombre de la prueba.
    const bool celdaPequena = (caja.bottom - caja.top) < 54 || (caja.right - caja.left) < 74;
    RECT rNum = caja;
    UINT alineacion = DT_LEFT;
    if (celdaPequena) {
        int altoNumero = 20;
        int altoPuntos = hayExamen && delMes ? 8 : 0;
        rNum.top = caja.top + ((caja.bottom - caja.top) - altoNumero - altoPuntos) / 2;
        rNum.bottom = rNum.top + altoNumero;
        alineacion = DT_CENTER;
    } else {
        rNum.left += 10;
        rNum.top += 6;
        rNum.bottom = rNum.top + 22;
        rNum.right = rNum.left + 34;
    }

    SetBkMode(dc, TRANSPARENT);
    if (!delMes) {
        SetTextColor(dc, gui::mezclar(t.panel, t.textoSuave, 0.55));
        SelectObject(dc, gui::fuenteInterfaz());
    } else if (elegido) {
        SetTextColor(dc, t.oscuro ? RGB(255, 255, 255) : RGB(20, 22, 28));
        SelectObject(dc, gui::fuenteNegrita());
    } else if (hayExamen) {
        SetTextColor(dc, t.oscuro ? gui::mezclar(colorPrincipal, RGB(255, 255, 255), 0.2)
                                  : gui::mezclar(colorPrincipal, RGB(0, 0, 0), 0.25));
        SelectObject(dc, gui::fuenteNegrita());
    } else if (esHoy) {
        SetTextColor(dc, t.acento);
        SelectObject(dc, gui::fuenteNegrita());
    } else {
        SetTextColor(dc, finDeSemana ? gui::mezclar(t.texto, t.textoSuave, 0.75) : t.texto);
        SelectObject(dc, gui::fuenteInterfaz());
    }
    DrawTextW(dc, texto, -1, &rNum, alineacion | DT_TOP | DT_SINGLELINE);

    if (!delMes) return;

    // --- nombre de la prueba dentro de la celda si hay sitio
    const int altoCelda = caja.bottom - caja.top;
    const int anchoCelda = caja.right - caja.left;
    if (!marca.textos.empty() && altoCelda >= 54 && anchoCelda >= 74) {
        SelectObject(dc, gui::fuenteInterfaz());
        int y = caja.top + 30;
        for (std::size_t i = 0; i < marca.textos.size() && i < 2; ++i) {
            RECT rt;
            rt.left = caja.left + 8;
            rt.right = caja.right - 6;
            rt.top = y;
            rt.bottom = y + 15;
            if (rt.bottom > caja.bottom - 4) break;
            SetTextColor(dc, marca.examenes[i]);
            DrawTextW(dc, marca.textos[i].c_str(), -1, &rt,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += 16;
        }
        if (marca.textos.size() > 2) {
            RECT rt;
            rt.left = caja.left + 8;
            rt.right = caja.right - 6;
            rt.top = y;
            rt.bottom = y + 14;
            if (rt.bottom <= caja.bottom - 2) {
                SetTextColor(dc, t.textoSuave);
                wchar_t mas[16];
                _snwprintf(mas, 15, L"+%d más", (int)marca.textos.size() - 2);
                mas[15] = L'\0';
                DrawTextW(dc, mas, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    } else if (!marca.examenes.empty()) {
        // celda pequena: puntos de color bajo el numero
        int diametro = 6;
        int sep = 3;
        int total = (int)marca.examenes.size();
        if (total > 4) total = 4;
        int anchoTotal = total * diametro + (total - 1) * sep;
        int x = (caja.left + caja.right) / 2 - anchoTotal / 2;
        int y = rNum.bottom + 1;
        if (y + diametro > caja.bottom - 2) y = caja.bottom - diametro - 2;
        for (int i = 0; i < total; ++i) {
            HBRUSH b = CreateSolidBrush(marca.examenes[(std::size_t)i]);
            HGDIOBJ bv = SelectObject(dc, b);
            HGDIOBJ pv = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, x, y, x + diametro + 1, y + diametro + 1);
            SelectObject(dc, bv);
            SelectObject(dc, pv);
            DeleteObject(b);
            x += diametro + sep;
        }
    }
}

void pintar(HWND h, HDC destino) {
    Estado* e = estadoDe(h);
    RECT cliente;
    GetClientRect(h, &cliente);

    HDC dc = CreateCompatibleDC(destino);
    HBITMAP lienzo = CreateCompatibleBitmap(destino, cliente.right, cliente.bottom);
    HGDIOBJ lienzoViejo = SelectObject(dc, lienzo);

    const gui::Tema& t = gui::tema();
    FillRect(dc, &cliente, gui::brochaFondo());
    gui::panelRedondeado(dc, cliente, t.panel, t.borde, 12);
    SetBkMode(dc, TRANSPARENT);

    if (!e) {
        BitBlt(destino, 0, 0, cliente.right, cliente.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, lienzoViejo);
        DeleteObject(lienzo);
        DeleteDC(dc);
        return;
    }

    // ---------------------------------------------------------- cabecera
    std::wstring nombreMes = gui::aW(Fecha::nombreMes(e->mes));
    if (!nombreMes.empty()) nombreMes[0] = (wchar_t)towupper(nombreMes[0]);
    wchar_t titulo[64];
    _snwprintf(titulo, 63, L"%s %d", nombreMes.c_str(), e->anio);
    titulo[63] = L'\0';

    RECT rTitulo;
    rTitulo.left = MARGEN() + 8;
    rTitulo.right = cliente.right - MARGEN();
    rTitulo.top = gui::escalar(8);
    rTitulo.bottom = gui::escalar(40);
    HGDIOBJ fuenteVieja = SelectObject(dc, gui::fuenteTitulo());
    SetTextColor(dc, t.texto);
    DrawTextW(dc, titulo, -1, &rTitulo, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT izq = rectFlecha(h, true);
    RECT der = rectFlecha(h, false);
    RECT hoyBoton = rectHoy(h);
    dibujarPastillaMes(dc, izq, L"<", false);
    dibujarPastillaMes(dc, der, L">", false);
    Fecha hoy = Fecha::hoy();
    dibujarPastillaMes(dc, hoyBoton, L"Hoy", hoy.anio == e->anio && hoy.mes == e->mes);

    // ------------------------------------------------- dias de la semana
    static const wchar_t* kDias[] = {L"LUN", L"MAR", L"MIÉ", L"JUE", L"VIE", L"SÁB", L"DOM"};
    SelectObject(dc, gui::fuenteNegrita());
    for (int d = 0; d < 7; ++d) {
        int c = columnaDe(e, d);
        if (c < 0) continue;
        RECT celda = rectCelda(h, c, 0);
        RECT r;
        r.left = celda.left;
        r.right = celda.right;
        r.top = ALTO_CABECERA();
        r.bottom = ALTO_CABECERA() + ALTO_SEMANA();
        SetTextColor(dc, d >= 5 ? gui::mezclar(t.textoSuave, t.panel, 0.35) : t.textoSuave);
        DrawTextW(dc, kDias[d], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // linea bajo los nombres de los dias
    HPEN lapiz = CreatePen(PS_SOLID, 1, t.borde);
    HGDIOBJ lv = SelectObject(dc, lapiz);
    MoveToEx(dc, MARGEN(), ALTO_CABECERA() + ALTO_SEMANA() - 2, NULL);
    LineTo(dc, cliente.right - MARGEN(), ALTO_CABECERA() + ALTO_SEMANA() - 2);
    SelectObject(dc, lv);
    DeleteObject(lapiz);

    // ------------------------------------------------------------ rejilla
    Fecha primero;
    primero.anio = e->anio;
    primero.mes = e->mes;
    primero.dia = 1;
    const int desplazamiento = primero.diaSemana();
    const int total = Fecha::diasEnMes(e->anio, e->mes);

    int anioPrev = e->anio, mesPrev = e->mes;
    mesAnterior(anioPrev, mesPrev);
    const int totalPrev = Fecha::diasEnMes(anioPrev, mesPrev);
    int anioSig = e->anio, mesSig = e->mes;
    mesSiguiente(anioSig, mesSig);

    for (int celdaIndice = 0; celdaIndice < 42; ++celdaIndice) {
        int fila = celdaIndice / 7;
        int diaSemana = celdaIndice % 7;   // 0 = lunes (el mes empieza en 'desplazamiento')
        int col = columnaDe(e, diaSemana);
        if (col < 0) continue;             // fin de semana oculto
        RECT celda = rectCelda(h, col, fila);

        int dia;
        int anioCelda = e->anio;
        int mesCelda = e->mes;
        bool delMes = true;

        if (celdaIndice < desplazamiento) {
            dia = totalPrev - (desplazamiento - 1 - celdaIndice);
            anioCelda = anioPrev;
            mesCelda = mesPrev;
            delMes = false;
        } else if (celdaIndice >= desplazamiento + total) {
            dia = celdaIndice - (desplazamiento + total) + 1;
            anioCelda = anioSig;
            mesCelda = mesSig;
            delMes = false;
        } else {
            dia = celdaIndice - desplazamiento + 1;
        }

        MarcaDia marca = marcasDe(e, anioCelda, mesCelda, dia);
        bool esHoy = (hoy.anio == anioCelda && hoy.mes == mesCelda && hoy.dia == dia);
        bool elegido = delMes && (e->diaSeleccionado == dia);
        bool encima = delMes && (e->diaEncima == dia);
        dibujarCelda(dc, celda, dia, marca, delMes, esHoy, elegido, encima, diaSemana >= 5);
    }

    SelectObject(dc, fuenteVieja);
    BitBlt(destino, 0, 0, cliente.right, cliente.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, lienzoViejo);
    DeleteObject(lienzo);
    DeleteDC(dc);
}

void cambiarMes(HWND h, int incremento) {
    Estado* e = estadoDe(h);
    if (!e) return;
    if (incremento < 0) {
        mesAnterior(e->anio, e->mes);
    } else {
        mesSiguiente(e->anio, e->mes);
    }
    InvalidateRect(h, NULL, FALSE);
}

int diaEnPunto(HWND h, POINT p) {
    Estado* e = estadoDe(h);
    if (!e) return 0;
    Fecha primero;
    primero.anio = e->anio;
    primero.mes = e->mes;
    primero.dia = 1;
    int desplazamiento = primero.diaSemana();
    int total = Fecha::diasEnMes(e->anio, e->mes);

    for (int dia = 1; dia <= total; ++dia) {
        int indice = desplazamiento + dia - 1;
        int col = columnaDe(e, indice % 7);
        if (col < 0) continue;
        RECT celda = rectCelda(h, col, indice / 7);
        if (PtInRect(&celda, p)) return dia;
    }
    return 0;
}

void pulsar(HWND h, int x, int y) {
    Estado* e = estadoDe(h);
    if (!e) return;

    POINT p;
    p.x = x;
    p.y = y;

    RECT izq = rectFlecha(h, true);
    RECT der = rectFlecha(h, false);
    RECT hoyBoton = rectHoy(h);
    if (PtInRect(&izq, p)) {
        cambiarMes(h, -1);
        return;
    }
    if (PtInRect(&der, p)) {
        cambiarMes(h, +1);
        return;
    }
    if (PtInRect(&hoyBoton, p)) {
        Fecha hoy = Fecha::hoy();
        e->anio = hoy.anio;
        e->mes = hoy.mes;
        e->diaSeleccionado = hoy.dia;
        InvalidateRect(h, NULL, FALSE);
        avisarAlPadre(h);
        return;
    }

    int dia = diaEnPunto(h, p);
    if (dia > 0) {
        e->diaSeleccionado = dia;
        InvalidateRect(h, NULL, FALSE);
        avisarAlPadre(h);
    }
}

LRESULT CALLBACK proc(HWND h, UINT mensaje, WPARAM wp, LPARAM lp) {
    switch (mensaje) {
        case WM_NCCREATE: {
            Estado* e = new Estado();
            Fecha hoy = Fecha::hoy();
            e->anio = hoy.anio;
            e->mes = hoy.mes;
            e->diaSeleccionado = hoy.dia;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)e);
            break;
        }
        case WM_NCDESTROY: {
            delete estadoDe(h);
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            break;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PRINTCLIENT:
            pintar(h, (HDC)wp);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            pintar(h, dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(h, NULL, FALSE);
            return 0;
        case WM_MOUSEMOVE: {
            Estado* e = estadoDe(h);
            if (e) {
                POINT p;
                p.x = (int)(short)LOWORD(lp);
                p.y = (int)(short)HIWORD(lp);
                int dia = diaEnPunto(h, p);
                if (dia != e->diaEncima) {
                    e->diaEncima = dia;
                    InvalidateRect(h, NULL, FALSE);
                    TRACKMOUSEEVENT seg;
                    ZeroMemory(&seg, sizeof(seg));
                    seg.cbSize = sizeof(seg);
                    seg.dwFlags = TME_LEAVE;
                    seg.hwndTrack = h;
                    TrackMouseEvent(&seg);
                }
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            Estado* e = estadoDe(h);
            if (e && e->diaEncima != 0) {
                e->diaEncima = 0;
                InvalidateRect(h, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            SetFocus(h);
            pulsar(h, (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
            return 0;
        case WM_MOUSEWHEEL:
            cambiarMes(h, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : +1);
            return 0;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;
        case WM_KEYDOWN:
            if (wp == VK_LEFT) cambiarMes(h, -1);
            if (wp == VK_RIGHT) cambiarMes(h, +1);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(h, mensaje, wp, lp);
}

}  // namespace

void registrarClase() {
    static bool hecho = false;
    if (hecho) return;
    hecho = true;

    WNDCLASSEXW clase;
    ZeroMemory(&clase, sizeof(clase));
    clase.cbSize = sizeof(clase);
    clase.style = CS_HREDRAW | CS_VREDRAW;
    clase.lpfnWndProc = proc;
    clase.hInstance = GetModuleHandleW(NULL);
    clase.hCursor = LoadCursorW(NULL, IDC_ARROW);
    clase.hbrBackground = NULL;
    clase.lpszClassName = kClase;
    RegisterClassExW(&clase);
}

HWND crear(HWND padre, int id) {
    registrarClase();
    return CreateWindowExW(0, kClase, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10,
                           padre, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

void ponerCurso(HWND calendario, const Curso* curso) {
    Estado* e = estadoDe(calendario);
    if (!e) return;
    e->curso = curso;
    InvalidateRect(calendario, NULL, FALSE);
}

void ponerOpciones(HWND calendario, bool lunesPrimero, bool verFinDeSemana, int diasDeAviso) {
    Estado* e = estadoDe(calendario);
    if (!e) return;
    e->lunesPrimero = lunesPrimero;
    e->verFinDeSemana = verFinDeSemana;
    e->diasDeAviso = diasDeAviso;
    InvalidateRect(calendario, NULL, FALSE);
}

void irAlMes(HWND calendario, int anio, int mes) {
    Estado* e = estadoDe(calendario);
    if (!e) return;
    e->anio = anio;
    e->mes = mes;
    InvalidateRect(calendario, NULL, FALSE);
}

void seleccionar(HWND calendario, const Fecha& fecha) {
    Estado* e = estadoDe(calendario);
    if (!e || !fecha.valida()) return;
    e->anio = fecha.anio;
    e->mes = fecha.mes;
    e->diaSeleccionado = fecha.dia;
    InvalidateRect(calendario, NULL, FALSE);
}

Fecha seleccion(HWND calendario) {
    Fecha f;
    Estado* e = estadoDe(calendario);
    if (!e) return f;
    f.anio = e->anio;
    f.mes = e->mes;
    f.dia = e->diaSeleccionado > 0 ? e->diaSeleccionado : 1;
    f.hora = -1;
    return f;
}

void refrescar(HWND calendario) {
    InvalidateRect(calendario, NULL, FALSE);
}

}  // namespace cal
