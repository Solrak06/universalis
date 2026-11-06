// GuiProfesorEnfoque.cpp - Modo enfoque y musica de fondo de la ventana del profesor.
//
// Modo enfoque (F9 o el boton «Enfoque»): pantalla completa sin barra de titulo ni lateral,
// el contenido centrado y mas estrecho, el fondo apagado y sin avisos. Arriba queda una
// capsula con la hora, el temporizador de concentracion, la musica y «Salir»; si no mueves
// el raton se reduce a una pastilla discreta.
//
// Musica (Musica.hpp): un boton en la barra (y en la capsula) abre un panel con los
// ambientes, el volumen y «Mi musica».
#include "ProfesorUi.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

#include "Musica.hpp"

namespace profesor {
namespace ui {

namespace {

const COLORREF NARANJA = RGB(251, 146, 60);
const COLORREF VERDE = RGB(52, 211, 153);
const int VOLUMENES[] = {20, 40, 60, 80, 100};

// Corchea dibujada (no hay icono de musica en la fuente de iconos).
void nota(Estado* s, HDC dc, int cx, int cy, int tam, COLORREF c) {
    float k = tam / 16.0f;
    circulo(dc, (int)(cx - 4 * k), (int)(cy + 4 * k), (int)std::max(2.0f, 3.2f * k), c);
    circulo(dc, (int)(cx + 4 * k), (int)(cy + 2.5f * k), (int)std::max(2.0f, 3.2f * k), c);
    float g = std::max(1.4f, 1.6f * k);
    linea(dc, cx - 1.2f * k, cy + 4 * k, cx - 1.2f * k, cy - 5.5f * k, c, g);
    linea(dc, cx + 6.8f * k, cy + 2.5f * k, cx + 6.8f * k, cy - 7 * k, c, g);
    linea(dc, cx - 1.2f * k, cy - 5.5f * k, cx + 6.8f * k, cy - 7 * k, c, g * 1.4f);
    (void)s;
}

// Barras que suben y bajan: que algo esta sonando.
void ecualizador(Estado* s, HDC dc, int cx, int cy, int tam, COLORREF c) {
    int ancho = std::max(2, tam / 6), hueco = std::max(1, tam / 10);
    for (int i = 0; i < 3; ++i) {
        double f = 0.35 + 0.65 * std::fabs(std::sin((s->animacion + i * 7) * 0.35 + i));
        int h = std::max(2, (int)(tam * f));
        int x = cx - (3 * ancho + 2 * hueco) / 2 + i * (ancho + hueco);
        redondeado(dc, rect(x, cy + tam / 2 - h, ancho, h), ancho / 2, c);
    }
}

std::wstring horaActual() {
    std::time_t t = std::time(nullptr);
    std::tm* l = std::localtime(&t);
    wchar_t b[16];
    swprintf(b, 16, L"%02d:%02d", l ? l->tm_hour : 0, l ? l->tm_min : 0);
    return b;
}

int restante(Estado* s) {
    if (!s->pomodoro) return 0;
    ULONGLONG ahora = s->pomodoroPausa ? s->pomodoroPausadoEn : GetTickCount64();
    return std::max(0, s->pomodoroDuracion - (int)((ahora - s->pomodoroInicio) / 1000));
}

}  // namespace

// ------------------------------------------------------------ barra de titulo

int pintarChipMusica(Estado* s, HDC dc, int derecha, int alto) {
    const COLORREF fondo = gui::colorBarraSuperior();
    const COLORREF chipFondo = mezcla(fondo, RGB(255, 255, 255), 0.06), encimaFondo = mezcla(fondo, RGB(255, 255, 255), 0.14);
    int suena = musica::sonando();
    COLORREF c = suena ? musica::color(suena) : RGB(150, 156, 168);
    std::wstring t = suena ? musica::nombre(suena) : L"Música";
    int tw = anchoTexto(dc, s->f.peque, t) + E(s, 46);
    RECT r = rect(derecha - tw, (alto - E(s, 30)) / 2, tw, E(s, 30));
    bool h = encima(s, A_MUSICA) || s->panelMusica;
    redondeado(dc, r, E(s, 15), h ? encimaFondo : chipFondo, mezcla(chipFondo, c, 0.5));
    if (suena) ecualizador(s, dc, r.left + E(s, 17), (r.top + r.bottom) / 2, E(s, 14), c);
    else nota(s, dc, r.left + E(s, 16), (r.top + r.bottom) / 2, E(s, 14), c);
    texto(dc, s->f.peque, gui::textoBarraSuperior(), t, rect(r.left + E(s, 32), r.top, tw - E(s, 36), r.bottom - r.top),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    zona(s, r, A_MUSICA);
    return tw;
}

int pintarChipEnfoque(Estado* s, HDC dc, int derecha, int alto) {
    const COLORREF fondo = gui::colorBarraSuperior();
    const COLORREF chipFondo = mezcla(fondo, RGB(255, 255, 255), 0.06), encimaFondo = mezcla(fondo, RGB(255, 255, 255), 0.14);
    std::wstring t = L"Enfoque";
    int tw = anchoTexto(dc, s->f.peque, t) + E(s, 46);
    RECT r = rect(derecha - tw, (alto - E(s, 30)) / 2, tw, E(s, 30));
    bool h = encima(s, A_ENFOQUE);
    COLORREF c = gui::tema().acento;
    redondeado(dc, r, E(s, 15), h ? encimaFondo : chipFondo, mezcla(chipFondo, c, 0.5));
    icono(s, dc, I_DIANA, r.left + E(s, 17), (r.top + r.bottom) / 2, E(s, 15), c);
    texto(dc, s->f.peque, gui::textoBarraSuperior(), t, rect(r.left + E(s, 32), r.top, tw - E(s, 36), r.bottom - r.top),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    zona(s, r, A_ENFOQUE);
    return tw;
}

// ------------------------------------------------------------ panel de musica

void pintarPanelMusica(Estado* s, HDC dc, RECT cliente) {
    if (!s->panelMusica) return;
    const gui::Tema& tm = gui::tema();
    velo(dc, cliente, RGB(0, 0, 0), tm.oscuro ? 70 : 40);
    zona(s, cliente, A_MUSICA_CERRAR);  // un clic fuera lo cierra
    // Rejilla de dos columnas: «Silencio» y «Mi musica» ocupan una fila entera cada uno
    int w = E(s, 560), filaAlto = E(s, 58);
    int n = musica::N_SONIDOS;
    int ambientes = n - 2;
    int filas = (ambientes + 1) / 2 + 2;
    int alto = E(s, 58) + filas * filaAlto + E(s, 112);
    int x = s->enfoque ? (cliente.right - w) / 2 : cliente.right - w - E(s, 200);
    int y = s->enfoque ? E(s, 70) : E(s, 58);
    x = std::max(E(s, 10), std::min((int)cliente.right - w - E(s, 10), x));
    y = std::max(E(s, 10), std::min(y, (int)cliente.bottom - alto - E(s, 10)));
    RECT panel = rect(x, y, w, alto);
    redondeado(dc, panel, E(s, 18), tm.panel, tm.borde);
    zona(s, panel, A_MUSICA_VOLUMEN, -1);  // un clic en el panel (fuera de sus botones) no lo cierra
    nota(s, dc, x + E(s, 28), y + E(s, 30), E(s, 18), tm.acento);
    texto(dc, s->f.subtitulo, tm.texto, L"Música para estudiar", rect(x + E(s, 46), y + E(s, 14), w - E(s, 60), E(s, 32)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    texto(dc, s->f.mini, tm.textoSuave, L"Ctrl+M  sí / no", rect(x + w - E(s, 160), y + E(s, 14), E(s, 140), E(s, 32)),
          DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    int fy = y + E(s, 54);
    int suena = musica::sonando();
    int colW = (w - E(s, 20)) / 2;
    std::vector<int> orden;
    for (int i = 1; i < n - 1; ++i) orden.push_back(i);
    orden.push_back(musica::MIA);
    orden.push_back(musica::NADA);
    int celda = 0;
    for (int i : orden) {
        bool ancha = i == musica::NADA || i == musica::MIA;
        if (ancha && celda % 2) {  // termina la fila a medias
            ++celda;
            fy += filaAlto;
        }
        RECT r = ancha ? rect(x + E(s, 10), fy, w - E(s, 20), filaAlto - E(s, 4))
                       : rect(x + E(s, 10) + (celda % 2) * colW, fy, colW - E(s, 4), filaAlto - E(s, 4));
        bool activo = i == suena, h = encima(s, A_MUSICA_SONIDO, i);
        COLORREF c = musica::color(i);
        if (activo) redondeado(dc, r, E(s, 12), mezcla(tm.panel, c, tm.oscuro ? 0.20 : 0.14));
        else if (h) redondeado(dc, r, E(s, 12), tm.panelAlt);
        int cx = r.left + E(s, 24), cy = (r.top + r.bottom) / 2;
        circulo(dc, cx, cy, E(s, 15), mezcla(tm.panel, c, activo ? 0.9 : 0.25));
        if (activo && i != musica::NADA) ecualizador(s, dc, cx, cy, E(s, 14), RGB(255, 255, 255));
        else if (i == musica::NADA) linea(dc, (float)cx - E(s, 5), (float)cy, (float)cx + E(s, 5), (float)cy, c, (float)E(s, 2));
        else nota(s, dc, cx, cy, E(s, 13), c);
        std::wstring desc = musica::descripcion(i);
        if (i == musica::MIA) {
            int canciones = musica::canciones();
            if (activo && !musica::cancion().empty()) desc = L"Suena: " + musica::cancion();
            else if (canciones == 0) desc = L"Copia mp3 en la carpeta Música (botón de abajo)";
            else desc = std::to_wstring(canciones) + (canciones == 1 ? L" canción" : L" canciones") + L" en tu carpeta, en orden aleatorio";
        }
        texto(dc, activo ? s->f.negrita : s->f.texto, tm.texto, musica::nombre(i), rect(cx + E(s, 26), r.top + E(s, 7), r.right - cx - E(s, 34), E(s, 22)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.mini, tm.textoSuave, desc, rect(cx + E(s, 26), r.top + E(s, 29), r.right - cx - E(s, 34), E(s, 18)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        zona(s, r, A_MUSICA_SONIDO, i);
        if (ancha) {
            fy += filaAlto;
            celda = 0;
        } else if (++celda % 2 == 0) {
            fy += filaAlto;
        }
    }
    // Volumen
    fy += E(s, 6);
    texto(dc, s->f.peque, tm.textoSuave, L"Volumen", rect(x + E(s, 20), fy, E(s, 80), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int vx = x + E(s, 96), actual = musica::volumen();
    int elegido = 0;
    for (int k = 0; k < 5; ++k)
        if (std::abs(VOLUMENES[k] - actual) < std::abs(VOLUMENES[elegido] - actual)) elegido = k;
    for (int k = 0; k < 5; ++k) {
        int bh = E(s, 8) + k * E(s, 4);
        RECT b = rect(vx + k * E(s, 34), fy + E(s, 26) - bh, E(s, 24), bh);
        bool h = encima(s, A_MUSICA_VOLUMEN, k);
        redondeado(dc, b, E(s, 3), k <= elegido ? tm.acento : h ? mezcla(tm.borde, tm.acento, 0.5) : tm.borde);
        zona(s, rect(vx + k * E(s, 34) - E(s, 4), fy, E(s, 32), E(s, 34)), A_MUSICA_VOLUMEN, k);
    }
    texto(dc, s->f.peque, tm.textoSuave, std::to_wstring(VOLUMENES[elegido]) + L" %",
          rect(vx + 5 * E(s, 34), fy, E(s, 60), E(s, 34)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    fy += E(s, 46);
    int bw = (w - E(s, 50)) / 2;
    boton(s, dc, rect(x + E(s, 20), fy, bw, E(s, 36)), L"Carpeta Música", A_MUSICA_CARPETA, 0, FANTASMA);
    boton(s, dc, rect(x + E(s, 30) + bw, fy, bw, E(s, 36)), L"Siguiente canción", A_MUSICA_SIGUIENTE, 0, FANTASMA,
          suena == musica::MIA && musica::canciones() > 1);
}

// ------------------------------------------------------------ modo enfoque

void pintarCapsulaEnfoque(Estado* s, HDC dc, RECT cliente) {
    if (!s->enfoque) return;
    const gui::Tema& tm = gui::tema();
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(s->ventana, &pt);
    bool despierta = GetTickCount64() - s->movido < 2500 || pt.y < E(s, 90) || s->panelMusica || s->panelTest;
    std::wstring hora = horaActual();
    if (!despierta) {
        // Discreta: solo la hora (y el tiempo de concentracion si esta en marcha)
        std::wstring t = hora;
        if (s->pomodoro) t += L"   ·   " + tiempo(restante(s));
        if (hayTest(s) && !testTerminado(s))
            t += L"   ·   test " + std::to_wstring(s->sesion["respondidas"].comoEntero()) + L"/" + std::to_wstring(s->sesion["n"].comoEntero());
        int tw = anchoTexto(dc, s->f.peque, t) + E(s, 32);
        RECT r = rect((cliente.right - tw) / 2, E(s, 12), tw, E(s, 28));
        redondeado(dc, r, E(s, 14), mezcla(tm.fondo, tm.panel, 0.6));
        texto(dc, s->f.peque, mezcla(tm.textoSuave, tm.fondo, 0.25), t, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    // Completa: hora · concentracion · musica · salir
    int alto = E(s, 44), pad = E(s, 8);
    std::wstring tPom = s->pomodoro == 0 ? L"Concentrarme " + std::to_wstring(gui::estilo().pomodoro) + L" min"
                        : (s->pomodoro == 1 ? L"Concentración  " : L"Descanso  ") + tiempo(restante(s)) + (s->pomodoroPausa ? L"  (pausa)" : L"");
    int suena = musica::sonando();
    std::wstring tMus = suena ? musica::nombre(suena) : L"Música";
    // Objetivo del dia: anillo alrededor de la diana y «35/60 min» junto a la hora
    int objetivo = gui::estilo().objetivoDiario, hoy = minutosHoy(s);
    std::wstring tObj = objetivo > 0 ? std::to_wstring(hoy) + L"/" + std::to_wstring(objetivo) + L" min" : L"";
    int wObj = objetivo > 0 ? anchoTexto(dc, s->f.mini, tObj) + E(s, 12) : 0;
    int wHora = anchoTexto(dc, s->f.negrita, hora) + E(s, 54) + wObj;
    int wPom = anchoTexto(dc, s->f.peque, tPom) + E(s, 44);
    int wMus = anchoTexto(dc, s->f.peque, tMus) + E(s, 44);
    int wSal = anchoTexto(dc, s->f.peque, L"Salir") + E(s, 44);
    // Test: empezar uno sin salir del enfoque, o ver por donde va el que esta en marcha
    bool enCurso = hayTest(s) && !testTerminado(s);
    std::wstring tTest = enCurso ? L"Test  " + std::to_wstring(s->sesion["respondidas"].comoEntero()) + L"/" +
                                       std::to_wstring(s->sesion["n"].comoEntero())
                                 : L"Hacer un test";
    int wTest = anchoTexto(dc, s->f.peque, tTest) + E(s, 44);
    int total = pad + wHora + wTest + E(s, 6) + wPom + E(s, 6) + wMus + E(s, 6) + wSal + pad;
    RECT caja = rect((cliente.right - total) / 2, E(s, 12), total, alto);
    redondeado(dc, caja, alto / 2, tm.panel, mezcla(tm.borde, tm.acento, 0.35));
    zona(s, caja, A_NADA);
    int x = caja.left + pad, cy = caja.top + alto / 2;
    if (objetivo > 0) {
        double fr = std::min(1.0, (double)hoy / objetivo);
        COLORREF c = fr >= 1 ? VERDE : tm.acento;
        arco(dc, x + E(s, 16), cy, E(s, 13), (float)E(s, 2), 0, 360, mezcla(tm.panel, tm.borde, 0.8));
        if (fr > 0) arco(dc, x + E(s, 16), cy, E(s, 13), (float)E(s, 2), -90, (float)(360 * fr), c);
        icono(s, dc, fr >= 1 ? I_CHECK : I_DIANA, x + E(s, 16), cy, E(s, 12), c);
    } else {
        icono(s, dc, I_DIANA, x + E(s, 16), cy, E(s, 16), tm.acento);
    }
    int hw = anchoTexto(dc, s->f.negrita, hora);
    texto(dc, s->f.negrita, tm.texto, hora, rect(x + E(s, 36), caja.top, hw + E(s, 4), alto), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (objetivo > 0)
        texto(dc, s->f.mini, tm.textoSuave, tObj, rect(x + E(s, 36) + hw + E(s, 10), caja.top, wObj, alto), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    x += wHora;
    auto pildora = [&](int ancho, int accion, COLORREF c, const std::wstring& t, int tipo) {
        RECT r = rect(x, caja.top + E(s, 6), ancho, alto - E(s, 12));
        bool h = encima(s, accion);
        redondeado(dc, r, (alto - E(s, 12)) / 2, h ? tm.panelAlt : mezcla(tm.panel, c, 0.10), mezcla(tm.borde, c, 0.45));
        int ix = r.left + E(s, 17), iy = (r.top + r.bottom) / 2;
        if (tipo == 0) icono(s, dc, s->pomodoroPausa ? I_PAUSA : I_RELOJ, ix, iy, E(s, 15), c);
        else if (tipo == 3) icono(s, dc, I_TEST, ix, iy, E(s, 15), c);
        else if (tipo == 1 && suena) ecualizador(s, dc, ix, iy, E(s, 13), c);
        else if (tipo == 1) nota(s, dc, ix, iy, E(s, 13), c);
        texto(dc, s->f.peque, tm.texto, t, rect(r.left + (tipo == 2 ? 0 : E(s, 32)), r.top, ancho - (tipo == 2 ? 0 : E(s, 36)), r.bottom - r.top),
              (tipo == 2 ? DT_CENTER : DT_LEFT) | DT_VCENTER | DT_SINGLELINE);
        zona(s, r, accion);
        x += ancho + E(s, 6);
    };
    pildora(wTest, A_PANEL_TEST, gui::tema().acento, tTest, 3);
    pildora(wPom, A_ENFOQUE_POMODORO, s->pomodoro == 2 ? VERDE : NARANJA, tPom, 0);
    pildora(wMus, A_MUSICA, suena ? musica::color(suena) : tm.textoSuave, tMus, 1);
    pildora(wSal, A_ENFOQUE, tm.textoSuave, L"Salir", 2);
}

void alternarEnfoque(Estado* s) {
    if (!s->enfoque) {
        s->enfoque = true;
        s->completaAntesDeEnfoque = s->pantallaCompleta;
        if (!s->pantallaCompleta) alternarPantallaCompleta(s);
        s->selector = false;
        s->movido = GetTickCount64();
        SetTimer(s->ventana, TIMER_ENFOQUE, 1000, NULL);
    } else {
        s->enfoque = false;
        if (s->pantallaCompleta && !s->completaAntesDeEnfoque) alternarPantallaCompleta(s);
        KillTimer(s->ventana, TIMER_ENFOQUE);
        s->mensaje = L"Has salido del modo enfoque.";
    }
    s->scroll = 0;
    if (s->fondo) DeleteObject(s->fondo);  // el fondo cambia (mas apagado)
    s->fondo = NULL;
    colocarEdit(s);
    InvalidateRect(s->ventana, NULL, FALSE);
}

// ------------------------------------------------------------ pantalla de descanso

int minutosHoy(Estado* s) {
    int m = s->inicio["nivel"]["minutos_hoy"].comoEntero();
    if (s->logros.esObjeto() && s->logros.contiene("minutos_hoy")) m = std::max(m, s->logros["minutos_hoy"].comoEntero());
    if (s->pomodoro == 1) m += (s->pomodoroDuracion - restante(s)) / 60;  // el bloque en curso tambien cuenta
    return m;
}

void abrirDescanso(Estado* s) {
    s->pantallaDescanso = true;
    s->descansoInicio = GetTickCount64();
    s->panelMusica = false;
    s->comandos = false;
    s->atajos = false;
    SetFocus(s->ventana);
    SetTimer(s->ventana, TIMER_ANIMACION, 40, NULL);
    InvalidateRect(s->ventana, NULL, FALSE);
}

void pintarDescanso(Estado* s, HDC dc, RECT cliente) {
    if (!s->pantallaDescanso) return;
    const gui::Tema& tm = gui::tema();
    // Lo de debajo no se puede pulsar; la barra de titulo (por encima de `cliente`) si
    s->zonas.erase(std::remove_if(s->zonas.begin(), s->zonas.end(), [&](const Zona& z) { return z.r.bottom > cliente.top; }),
                   s->zonas.end());
    trozoDeFondo(s, dc, cliente);
    velo(dc, cliente, tm.fondo, tm.oscuro ? 225 : 215);
    pintarAmbiente(s, dc, cliente);
    zona(s, cliente, A_BLOQUEO);
    int cx = cliente.right / 2;
    int alto = cliente.bottom - cliente.top;
    bool terminado = s->pomodoro != 2;  // el descanso ya acabo (o se paro)

    // Respiracion: 4 s inspirar, 2 s mantener, 6 s soltar (ciclo de 12 s)
    double t = (GetTickCount64() - s->descansoInicio) / 1000.0;
    double ciclo = std::fmod(t, 12.0), escala;
    std::wstring fase;
    int quedan;
    auto suave = [](double x) { return 0.5 - 0.5 * std::cos(3.14159265358979 * x); };
    if (ciclo < 4) {
        escala = 0.55 + 0.45 * suave(ciclo / 4);
        fase = L"Inspira";
        quedan = (int)std::ceil(4 - ciclo);
    } else if (ciclo < 6) {
        escala = 1.0;
        fase = L"Mantén";
        quedan = (int)std::ceil(6 - ciclo);
    } else {
        escala = 1.0 - 0.45 * suave((ciclo - 6) / 6);
        fase = L"Suelta el aire";
        quedan = (int)std::ceil(12 - ciclo);
    }
    int R = std::min(E(s, 120), alto / 6);
    int cy = cliente.top + alto * 46 / 100;

    // Encabezado
    int y = std::max((int)cliente.top + E(s, 30), cy - R * 3 / 2 - E(s, 150));
    {
        std::wstring et = terminado ? L"DESCANSO TERMINADO" : L"DESCANSO";
        int ew = anchoTexto(dc, s->f.peque, et), lado = E(s, 22), total = lado + E(s, 8) + ew;
        int ex = (int)(cliente.right - total) / 2;
        glifo(s, dc, I_FUEGO, ex + lado / 2, y + E(s, 9), lado, VERDE, L"cafe");
        texto(dc, s->f.peque, VERDE, et, rect(ex + lado + E(s, 8), y, ew + E(s, 4), E(s, 22)), DT_LEFT | DT_SINGLELINE);
    }
    if (terminado) {
        texto(dc, s->f.titulo, tm.texto, L"¡Listo para otro bloque!", rect(0, y + E(s, 30), cliente.right, E(s, 60)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        texto(dc, s->f.enorme, tm.texto, tiempo(restante(s)), rect(0, y + E(s, 26), cliente.right, E(s, 76)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Circulo que respira, con halos
    if (!terminado) {
        int r = (int)(R * escala);
        circulo(dc, cx, cy, (int)(r * 1.40), mezcla(tm.fondo, VERDE, tm.oscuro ? 0.05 : 0.06));
        circulo(dc, cx, cy, (int)(r * 1.20), mezcla(tm.fondo, VERDE, tm.oscuro ? 0.10 : 0.12));
        circulo(dc, cx, cy, r, mezcla(tm.fondo, VERDE, tm.oscuro ? 0.32 : 0.30), mezcla(tm.fondo, VERDE, 0.6), 1.5f);
        // Anillo exterior con lo que queda de descanso
        double fr = (double)restante(s) / std::max(1, s->pomodoroDuracion);
        arco(dc, cx, cy, R * 3 / 2, (float)E(s, 3), 0, 360, mezcla(tm.fondo, tm.borde, 0.7));
        arco(dc, cx, cy, R * 3 / 2, (float)E(s, 3), -90, (float)(360 * fr), VERDE);
        texto(dc, s->f.subtitulo, tm.texto, fase, rect(cx - R, cy - E(s, 20), 2 * R, E(s, 28)), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.peque, mezcla(tm.texto, tm.fondo, 0.3), std::to_wstring(quedan), rect(cx - R, cy + E(s, 10), 2 * R, E(s, 22)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        circulo(dc, cx, cy, R, mezcla(tm.fondo, VERDE, tm.oscuro ? 0.18 : 0.16));
        marca(dc, cx, cy, R / 2, true);
    }

    // Consejo (cambia cada 15 s) o la pregunta de seguir
    static const wchar_t* CONSEJOS[] = {
        L"Levántate y estira la espalda y el cuello.",
        L"Mira algo lejano por la ventana: tus ojos descansan.",
        L"Bebe un vaso de agua.",
        L"Deja el móvil: el descanso es para tu cabeza.",
        L"Camina un poco por la habitación.",
        L"Relaja los hombros y suelta la mandíbula.",
        L"Repasa mentalmente una idea de lo que acabas de estudiar.",
        L"Abre la ventana y respira aire fresco.",
    };
    int ty = cy + R * 3 / 2 + E(s, 34);
    std::wstring consejo = terminado ? L"Otro bloque de " + std::to_wstring(gui::estilo().pomodoro) + L" minutos y vas sumando."
                                     : CONSEJOS[(int)(t / 15) % (int)(sizeof(CONSEJOS) / sizeof(CONSEJOS[0]))];
    texto(dc, s->f.texto, tm.textoSuave, consejo, rect(cliente.left + E(s, 40), ty, cliente.right - E(s, 80), E(s, 26)),
          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    ty += E(s, 40);

    // Lo estudiado hoy frente al objetivo
    int objetivo = gui::estilo().objetivoDiario, hoy = minutosHoy(s);
    if (objetivo > 0) {
        int bw = E(s, 320);
        double fr = std::min(1.0, (double)hoy / objetivo);
        std::wstring t2 = fr >= 1 ? L"¡Objetivo de hoy cumplido!  " + std::to_wstring(hoy) + L" min"
                                  : L"Hoy: " + std::to_wstring(hoy) + L" de " + std::to_wstring(objetivo) + L" min de concentración";
        texto(dc, s->f.peque, tm.texto, t2, rect(cx - bw, ty, 2 * bw, E(s, 22)), DT_CENTER | DT_SINGLELINE);
        barra(dc, rect(cx - bw / 2, ty + E(s, 28), bw, E(s, 6)), fr, fr >= 1 ? VERDE : tm.acento);
        ty += E(s, 52);
    } else if (hoy > 0) {
        texto(dc, s->f.peque, tm.texto, L"Hoy llevas " + std::to_wstring(hoy) + L" min de concentración",
              rect(cx - E(s, 300), ty, E(s, 600), E(s, 22)), DT_CENTER | DT_SINGLELINE);
        ty += E(s, 40);
    }

    // Botones
    int bw = E(s, 230), bh = E(s, 46);
    ty = std::min(ty + E(s, 6), (int)cliente.bottom - bh - E(s, 30));
    if (terminado) {
        boton(s, dc, rect(cx - bw - E(s, 6), ty, bw, bh), L"Empezar otro bloque", A_DESCANSO_ESTUDIAR, 0, PRIMARIO);
        boton(s, dc, rect(cx + E(s, 6), ty, bw, bh), L"Ahora no", A_DESCANSO_CERRAR, 0, SECUNDARIO);
    } else {
        boton(s, dc, rect(cx - bw - E(s, 6), ty, bw, bh), L"Volver a estudiar ya", A_DESCANSO_ESTUDIAR, 0, SECUNDARIO);
        boton(s, dc, rect(cx + E(s, 6), ty, bw, bh), L"Ocultar  ·  Esc", A_DESCANSO_CERRAR, 0, FANTASMA);
    }
}

bool enfoqueAccion(Estado* s, int accion, int dato) {
    switch (accion) {
        case A_ENFOQUE: alternarEnfoque(s); return true;
        case A_DESCANSO:
            if (s->pomodoro == 2) abrirDescanso(s);
            else if (s->pomodoro == 0) empezarDescanso(s);
            return true;
        case A_DESCANSO_CERRAR:
            s->pantallaDescanso = false;
            InvalidateRect(s->ventana, NULL, FALSE);
            return true;
        case A_DESCANSO_ESTUDIAR:
            s->pantallaDescanso = false;
            if (s->pomodoro) herramientasAccion(s, A_POMODORO_PARAR, 0);
            empezarConcentracion(s);
            InvalidateRect(s->ventana, NULL, FALSE);
            return true;
        case A_ENFOQUE_POMODORO:
            // Igual que el boton de concentracion de la barra: empieza, pausa o reanuda
            if (s->pomodoro == 0) empezarConcentracion(s);
            else herramientasAccion(s, A_POMODORO, 0);
            break;
        case A_MUSICA: s->panelMusica = !s->panelMusica; break;
        case A_MUSICA_CERRAR: s->panelMusica = false; break;
        case A_MUSICA_SONIDO:
            if (dato == musica::NADA) musica::parar();
            else if (dato == musica::MIA && musica::canciones() == 0) gui::abrirEnSistema(s->ventana, W(musica::carpeta()));
            else musica::tocar(dato);
            break;
        case A_MUSICA_VOLUMEN:
            if (dato >= 0) musica::ponerVolumen(VOLUMENES[std::min(4, dato)]);
            break;
        case A_MUSICA_CARPETA: gui::abrirEnSistema(s->ventana, W(musica::carpeta())); break;
        case A_MUSICA_SIGUIENTE: musica::siguiente(); break;
        default: return false;
    }
    if (musica::sonando()) SetTimer(s->ventana, TIMER_ANIMACION, 110, NULL);  // el ecualizador se mueve
    InvalidateRect(s->ventana, NULL, FALSE);
    return true;
}

}  // namespace ui
}  // namespace profesor
