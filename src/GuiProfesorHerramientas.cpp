// GuiProfesorHerramientas.cpp - Mas herramientas de la ventana del profesor:
// buscador y glosario de los apuntes, mapa mental de cada tema, calendario hasta el
// examen, logros y nivel, temporizador de estudio (pomodoro), exportacion, avisos
// flotantes y cambio de asignatura sin cerrar el profesor.
//
// La logica vive en profesor/herramientas.py; aqui se pinta y se piden cosas al motor.
#include "ProfesorUi.hpp"

#include "Musica.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwctype>

namespace profesor {
namespace ui {

namespace {

const COLORREF VIOLETA = RGB(167, 139, 250);
const COLORREF TURQUESA = RGB(45, 212, 191);
const COLORREF ORO = RGB(250, 204, 21);
const COLORREF ROSA = RGB(244, 114, 182);
// Colores de las ramas del mapa mental.
const COLORREF RAMAS[] = {RGB(96, 165, 250), RGB(52, 211, 153), RGB(251, 146, 60), RGB(167, 139, 250),
                          RGB(244, 114, 182), RGB(250, 204, 21)};

// Duraciones del temporizador: salen de los Ajustes (25 y 5 minutos por defecto).
int segundosEstudio() { return std::max(5, std::min(90, gui::estilo().pomodoro)) * 60; }
int segundosDescanso() { return std::max(1, std::min(30, gui::estilo().descanso)) * 60; }

std::wstring textoDe(HWND campo) {
    int largo = GetWindowTextLengthW(campo);
    std::vector<wchar_t> b((std::size_t)largo + 1);
    GetWindowTextW(campo, b.data(), largo + 1);
    return b.data();
}

// Inicial sin tilde y en mayuscula, para agrupar el glosario.
wchar_t inicial(const std::wstring& t) {
    if (t.empty()) return L'#';
    wchar_t c = (wchar_t)towupper(t[0]);
    switch (c) {
        case L'Á': return L'A';
        case L'É': return L'E';
        case L'Í': return L'I';
        case L'Ó': return L'O';
        case L'Ú': case L'Ü': return L'U';
        default: return iswalpha(c) ? c : L'#';
    }
}

std::wstring minusculas(std::wstring t) {
    for (wchar_t& c : t) {
        c = (wchar_t)towlower(c);
        switch (c) {
            case L'á': c = L'a'; break;
            case L'é': c = L'e'; break;
            case L'í': c = L'i'; break;
            case L'ó': c = L'o'; break;
            case L'ú': case L'ü': c = L'u'; break;
            default: break;
        }
    }
    return t;
}

int restantePomodoro(Estado* s) {
    if (!s->pomodoro) return 0;
    ULONGLONG ahora = s->pomodoroPausa ? s->pomodoroPausadoEn : GetTickCount64();
    int pasados = (int)((ahora - s->pomodoroInicio) / 1000);
    return std::max(0, s->pomodoroDuracion - pasados);
}

void empezarPomodoro(Estado* s, int estado, int segundos) {
    s->pomodoro = estado;
    s->pomodoroPausa = false;
    s->pomodoroInicio = GetTickCount64();
    s->pomodoroDuracion = segundos;
    SetTimer(s->ventana, TIMER_POMODORO, 1000, NULL);
}

void registrarMinutos(Estado* s, int minutos) {
    js::Valor p = js::Valor::objeto();
    p["minutos"] = minutos;
    enviar(s, "registrar_estudio", p);
}

void pedirBusqueda(Estado* s) {
    KillTimer(s->ventana, TIMER_BUSQUEDA);
    if (s->modoBusqueda != 0) return;  // el glosario se filtra aqui mismo
    std::wstring t = textoDe(s->busqueda);
    if (t.size() < 2) {
        s->resultados = js::Valor();
        InvalidateRect(s->ventana, NULL, FALSE);
        return;
    }
    js::Valor p = js::Valor::objeto();
    p["texto"] = gui::aUtf8(t);
    enviar(s, "buscar", p);
}

void pedirMapa(Estado* s, bool conIa, bool rehacer) {
    if (s->resumenRuta.empty() || s->tareaMapa.activa()) return;
    js::Valor p = js::Valor::objeto();
    p["ruta"] = s->resumenRuta;
    p["ia"] = conIa;
    p["rehacer"] = rehacer;
    int id = enviar(s, "mapa", p);
    if (conIa && iaDisponible(s)) {
        s->tareaMapa.id = id;
        s->tareaMapa.parcial.clear();
        s->tareaMapa.caracteres = 0;
        s->tareaMapa.inicio = GetTickCount64();
        s->mensaje = L"Estoy dibujando el mapa mental del tema con la IA. Tarda un minuto o dos.";
        SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    } else {
        s->tareaMapa.id = id;  // sin IA llega al instante, pero asi se sabe que esta pedido
        s->tareaMapa.inicio = 0;
    }
}

// Deja la ventana como recien abierta y pide al motor la asignatura nueva.
void cambiarAsignatura(Estado* s, const Asignatura& a) {
    js::Valor cancelar = js::Valor::objeto();
    enviar(s, "cancelar", cancelar);  // sin «tarea»: cancela todo lo de la IA
    enviar(s, "detener_resumenes");
    s->asignatura = a;
    s->hola = s->config = s->sesion = s->pregunta = s->resultado = s->progreso = s->generacion = s->fallos = js::Valor();
    s->inicio = s->tarjetas = s->mazo = s->resumenes = s->resumen = s->resumenesFondo = js::Valor();
    s->desarrollo = s->abierta = s->correccion = s->analisis = js::Valor();
    s->temasIa.clear();
    s->resultados = s->glosario = s->mapa = s->logros = js::Valor();
    s->temas.clear();
    s->chat.clear();
    s->chatEnCurso = false;
    s->trucos.clear();
    s->historial.clear();
    s->contextoIndice = -1;
    s->contextoPregunta.clear();
    s->resumenRuta.clear();
    s->desarrolloRuta.clear();
    s->tarjetasFin = false;
    s->verMapa = false;
    for (TareaIA* t : {&s->truco, &s->tareaResumen, &s->tareaAbierta, &s->tareaCorreccion, &s->tareaAnalisis, &s->tareaMapa})
        *t = TareaIA();
    s->selector = false;
    SetWindowTextW(s->busqueda, L"");
    SetWindowTextW(s->respuesta, L"");
    SetWindowTextW(s->ventana, (L"Profesor · " + W(a.nombre)).c_str());
    js::Valor p = js::Valor::objeto();
    p["codigo"] = a.codigo;
    p["nombre"] = a.nombre;
    p["temario"] = carpetaDeAsignatura(a, true);
    js::Valor examenes = js::Valor::array();
    for (const Examen& e : a.examenes) {
        if (!e.fecha.valida()) continue;
        char fecha[16];
        snprintf(fecha, sizeof(fecha), "%04d-%02d-%02d", e.fecha.anio, e.fecha.mes, e.fecha.dia);
        js::Valor x = js::Valor::objeto();
        x["nombre"] = e.nombre;
        x["fecha"] = std::string(fecha);
        x["peso"] = e.ponderacion;
        x["calificado"] = e.calificado;
        examenes.agregar(x);
    }
    p["examenes"] = examenes;
    s->vista = V_ARRANCANDO;
    enviar(s, "hola", p, true, L"Abriendo " + W(a.nombre) + L"…");
}

}  // namespace

// ================================================================== avisos

void aviso(Estado* s, const std::wstring& t) {
    s->toast = t;
    s->toastHasta = GetTickCount64() + 4500;
    SetTimer(s->ventana, TIMER_ANIMACION, 50, NULL);
    InvalidateRect(s->ventana, NULL, FALSE);
}

void pintarToast(Estado* s, HDC dc, RECT cuerpo) {
    if (s->toast.empty() || GetTickCount64() >= s->toastHasta) return;
    const gui::Tema& tm = gui::tema();
    // Arriba a la derecha (en el centro tapaba el título de la pantalla) y entrando con un desliz
    int tw = std::min(anchoTexto(dc, s->f.negrita, s->toast) + E(s, 70), std::min(E(s, 560), (int)(cuerpo.right - cuerpo.left) - E(s, 40)));
    long long resta = (long long)s->toastHasta - (long long)GetTickCount64();
    int entrada = (int)std::max(0LL, std::min(4500LL, 4500 - resta));  // ms desde que salio
    double t = std::min(1.0, entrada / 220.0);
    int desliz = (int)((1 - t) * (1 - t) * E(s, 24));
    RECT r = rect(cuerpo.right - E(s, 24) - tw + desliz, cuerpo.top + E(s, 16), tw, E(s, 48));
    // Logros y niveles con trofeo; el resto, con una marca discreta
    bool logro = s->toast.rfind(L"Logro", 0) == 0 || s->toast.rfind(L"¡Subes", 0) == 0;
    // Sombra suave y pastilla
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        for (int i = 4; i >= 1; --i) {
            RECT sr = r;
            OffsetRect(&sr, 0, i * 2);
            InflateRect(&sr, i, i);
            Gdiplus::GraphicsPath ps;
            caminoRedondeado(ps, sr, E(s, 24) + i);
            Gdiplus::SolidBrush sombra(Gdiplus::Color(18, 0, 0, 0));
            g.FillPath(&sombra, &ps);
        }
    }
    COLORREF marca = logro ? ORO : tm.acento;
    redondeado(dc, r, E(s, 24), tm.oscuro ? RGB(38, 42, 54) : RGB(255, 255, 255), mezcla(tm.borde, marca, logro ? 0.6 : 0.35), logro ? 2 : 1);
    if (logro) glifo(s, dc, I_TROFEO, r.left + E(s, 28), r.top + E(s, 24), E(s, 20), ORO);
    else {
        redondeado(dc, rect(r.left + E(s, 18), r.top + E(s, 14), E(s, 20), E(s, 20)), E(s, 10), mezcla(tm.oscuro ? RGB(38, 42, 54) : RGB(255, 255, 255), marca, 0.22));
        redondeado(dc, rect(r.left + E(s, 24), r.top + E(s, 20), E(s, 8), E(s, 8)), E(s, 4), marca);
    }
    texto(dc, s->f.negrita, tm.texto, s->toast, rect(r.left + E(s, 50), r.top, tw - E(s, 64), E(s, 48)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ================================================================ buscador

int pintarBuscar(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w;
    const js::Valor& glosario = s->glosario["glosario"];
    y += cabecera(s, dc, x, y, w, I_BUSCAR, tm.acento, L"Buscar en tus apuntes",
                  L"Escribe un concepto y te enseño en qué tema y página aparece, con la frase exacta. El glosario "
                  L"reúne todas las definiciones de tu temario (y crece con cada resumen que hagas).");
    {
        int px = x;
        px += chip(s, dc, px, y, L"Buscar en los apuntes", s->modoBusqueda == 0, A_B_MODO, 0) + E(s, 8);
        chip(s, dc, px, y, L"Glosario  ·  " + plural((int)glosario.tam(), L"concepto", L"conceptos"), s->modoBusqueda == 1, A_B_MODO, 1);
        y += E(s, 50);
    }
    // Campo de busqueda (el EDIT nativo va dentro)
    RECT campo = rect(x, y, w, E(s, 52));
    bool foco = GetFocus() == s->busqueda;
    redondeado(dc, campo, E(s, 26), tm.panel, foco ? tm.acento : tm.borde, foco ? 2 : 1);
    icono(s, dc, I_BUSCAR, x + E(s, 28), y + E(s, 26), E(s, 18), foco ? tm.acento : tm.textoSuave);
    s->rectBusqueda = {x + E(s, 52), y + E(s, 15), x + w - E(s, 20), y + E(s, 40)};
    y = campo.bottom + E(s, 22);
    std::wstring consulta = textoDe(s->busqueda);

    if (s->modoBusqueda == 1) {
        // ---- Glosario: letras y lista filtrada
        std::wstring filtro = minusculas(consulta);
        std::wstring letras;
        for (std::size_t i = 0; i < glosario.tam(); ++i) {
            wchar_t l = inicial(W(glosario[i]["termino"]));
            if (letras.find(l) == std::wstring::npos) letras += l;
        }
        std::sort(letras.begin(), letras.end());
        int px = x;
        px += chip(s, dc, px, y, L"Todas", s->letra.empty(), A_B_LETRA, 0) + E(s, 6);
        for (std::size_t k = 0; k < letras.size(); ++k) {
            std::wstring l(1, letras[k]);
            int cw = anchoTexto(dc, s->f.negrita, l) + E(s, 28);
            if (px + cw > x + w) {
                px = x;
                y += E(s, 42);
            }
            px += chip(s, dc, px, y, l, s->letra == l, A_B_LETRA, (int)letras[k]) + E(s, 6);
        }
        y += E(s, 52);
        if (glosario.tam() == 0) {
            texto(dc, s->f.texto, tm.textoSuave, L"Aún no he encontrado definiciones en tu temario. Haz algún resumen y aparecerán aquí.",
                  rect(x, y, w, E(s, 24)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            return y + E(s, 30) - a.y;
        }
        wchar_t actual = 0;
        int mostrados = 0;
        int colTermino = std::min(E(s, 280), w * 32 / 100);
        for (std::size_t i = 0; i < glosario.tam(); ++i) {
            const js::Valor& g = glosario[i];
            std::wstring termino = W(g["termino"]), def = W(g["definicion"]);
            wchar_t l = inicial(termino);
            if (!s->letra.empty() && l != s->letra[0]) continue;
            if (!filtro.empty() && minusculas(termino).find(filtro) == std::wstring::npos &&
                minusculas(def).find(filtro) == std::wstring::npos)
                continue;
            if (l != actual) {
                actual = l;
                texto(dc, s->f.titulo, tm.acento, std::wstring(1, l), rect(x, y, w, E(s, 36)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                y += E(s, 42);
            }
            int dw = w - colTermino - E(s, 48);
            int hd = altoTexto(dc, s->f.texto, def, dw);
            int ht = altoTexto(dc, s->f.negrita, termino, colTermino - E(s, 10));
            int alto = std::max(hd, ht) + E(s, 46);
            RECT fila = rect(x, y, w, alto);
            bool h = encima(s, A_B_CONCEPTO, (int)i);
            cristal(dc, fila, E(s, 12), h ? tm.panelAlt : CLR_INVALID);
            texto(dc, s->f.negrita, tm.texto, termino, rect(x + E(s, 18), y + E(s, 14), colTermino - E(s, 10), ht));
            texto(dc, s->f.texto, tm.texto, def, rect(x + colTermino + E(s, 28), y + E(s, 14), dw, hd));
            std::wstring fuente = W(g["tema"]) + L"  ·  " + W(g["ubicacion"]);
            texto(dc, s->f.mini, tm.textoSuave, fuente, rect(x + colTermino + E(s, 28), y + E(s, 18) + hd, dw, E(s, 18)),
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            zona(s, fila, A_B_CONCEPTO, (int)i);
            y += alto + E(s, 8);
            ++mostrados;
        }
        if (!mostrados)
            texto(dc, s->f.texto, tm.textoSuave, L"Ningún concepto coincide con el filtro.", rect(x, y, w, E(s, 24)),
                  DT_LEFT | DT_SINGLELINE);
        return y + E(s, 30) - a.y;
    }

    // ---- Buscar en los apuntes
    if (consulta.size() < 2 || s->resultados.esNulo()) {
        texto(dc, s->f.subtitulo, tm.texto, L"Prueba con…", rect(x, y, w, E(s, 26)), DT_LEFT | DT_SINGLELINE);
        y += E(s, 36);
        // Conceptos repartidos por todo el glosario (un salto que no comparta divisores con el total).
        std::size_t n = glosario.tam(), paso = 1;
        for (std::size_t c : {7u, 5u, 3u, 11u})
            if (n % c != 0) {
                paso = c;
                break;
            }
        int px = x, filas = 0;
        for (std::size_t i = 0; i < n && i < 14; ++i) {
            std::size_t k = (i * paso + 1) % n;
            std::wstring t = W(glosario[k]["termino"]);
            int cw = anchoTexto(dc, s->f.negrita, t) + E(s, 28);
            if (px + cw > x + w) {
                if (++filas == 3) break;
                px = x;
                y += E(s, 42);
            }
            px += chip(s, dc, px, y, t, false, A_B_CONCEPTO, 10000 + (int)k) + E(s, 8);
        }
        y += E(s, 56);
        std::wstring ayudaTexto = std::wstring(L"Busca por palabras («matriz de confusión», «overfitting»…). Pulsa un resultado "
                                               L"para abrir el apunte.") +
                                  (iaDisponible(s) ? L" Si lo que buscas no está claro, pregúntaselo al profesor en «Dudas»." : L"");
        int ha = altoTexto(dc, s->f.texto, ayudaTexto, w - E(s, 84));
        RECT ayuda = rect(x, y, w, ha + E(s, 36));
        redondeado(dc, ayuda, E(s, 14), mezcla(tm.panel, tm.acento, 0.07), mezcla(tm.borde, tm.acento, 0.35));
        icono(s, dc, I_BOMBILLA, x + E(s, 30), y + (ha + E(s, 36)) / 2, E(s, 22), tm.medio);
        texto(dc, s->f.texto, tm.textoSuave, ayudaTexto, rect(x + E(s, 62), y + E(s, 18), w - E(s, 84), ha));
        return ayuda.bottom - a.y + E(s, 20);
    }
    const js::Valor& conceptos = s->resultados["conceptos"];
    const js::Valor& res = s->resultados["resultados"];
    if (conceptos.tam() > 0) {
        tituloSeccion(s, dc, x, y, w, L"Conceptos");
        int columnas = w >= E(s, 760) ? 2 : 1, hueco = E(s, 12);
        int cw = (w - (columnas - 1) * hueco) / columnas;
        int filaY = y, maxAlto = 0;
        for (std::size_t i = 0; i < conceptos.tam(); ++i) {
            const js::Valor& c = conceptos[i];
            std::wstring def = W(c["definicion"]);
            int hd = std::min(altoTexto(dc, s->f.texto, def, cw - E(s, 36)), E(s, 88));
            int alto = hd + E(s, 74);
            int col = (int)(i % columnas);
            if (col == 0 && i > 0) {
                filaY += maxAlto + hueco;
                maxAlto = 0;
            }
            RECT r = rect(x + col * (cw + hueco), filaY, cw, alto);
            bool h = encima(s, A_B_CONCEPTO, 20000 + (int)i);
            cristal(dc, r, E(s, 14), h ? tm.panelAlt : CLR_INVALID, mezcla(tm.borde, tm.acento, 0.45));
            texto(dc, s->f.subtitulo, tm.texto, W(c["termino"]), rect(r.left + E(s, 18), r.top + E(s, 14), cw - E(s, 36), E(s, 24)),
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            texto(dc, s->f.texto, tm.texto, def, rect(r.left + E(s, 18), r.top + E(s, 44), cw - E(s, 36), hd),
                  DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
            texto(dc, s->f.mini, tm.textoSuave, W(c["tema"]), rect(r.left + E(s, 18), r.bottom - E(s, 26), cw - E(s, 36), E(s, 18)),
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            zona(s, r, A_B_CONCEPTO, 20000 + (int)i);
            maxAlto = std::max(maxAlto, alto);
        }
        y = filaY + maxAlto + E(s, 24);
    }
    tituloSeccion(s, dc, x, y, w, L"En tus apuntes", res.tam() ? std::to_wstring(res.tam()) + L" fragmentos, del más relevante al menos"
                                                               : L"No aparece en tu temario. Prueba con otras palabras.");
    for (std::size_t i = 0; i < res.tam(); ++i) {
        const js::Valor& r = res[i];
        std::wstring md = W(r["extracto"]);
        int pad = E(s, 18);
        int h = textoRico(s, dc, md, 0, 0, w - 2 * pad - E(s, 120), false);
        RECT fila = rect(x, y, w, h + E(s, 58));
        bool hov = encima(s, A_B_RESULTADO, (int)i);
        cristal(dc, fila, E(s, 14), hov ? tm.panelAlt : CLR_INVALID, hov ? tm.acento : CLR_INVALID);
        int px = x + pad;
        px += etiqueta(s, dc, px, y + E(s, 12), W(r["tema"]), tm.acento) + E(s, 8);
        texto(dc, s->f.peque, tm.textoSuave, W(r["ubicacion"]), rect(px, y + E(s, 12), E(s, 160), E(s, 26)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        textoRico(s, dc, md, x + pad, y + E(s, 46), w - 2 * pad - E(s, 120));
        texto(dc, s->f.peque, hov ? tm.acento : tm.textoSuave, L"Abrir  →", rect(fila.right - E(s, 110), y, E(s, 92), fila.bottom - fila.top),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        zona(s, fila, A_B_RESULTADO, (int)i);
        y = fila.bottom + E(s, 10);
    }
    return y + E(s, 20) - a.y;
}

void colocarBusqueda(Estado* s) {
    if (!s->busqueda) return;
    RECT r;
    IntersectRect(&r, &s->rectBusqueda, &s->cuerpo);
    bool ver = s->vista == V_BUSCAR && r.bottom - r.top > 10 && !s->ocupado && s->zoom.empty() && !s->selector && !hayCapa(s);
    if (ver) {
        RECT actual;
        GetWindowRect(s->busqueda, &actual);
        MapWindowPoints(NULL, s->ventana, (POINT*)&actual, 2);
        if (!EqualRect(&actual, &r)) MoveWindow(s->busqueda, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
        if (!IsWindowVisible(s->busqueda)) {
            ShowWindow(s->busqueda, SW_SHOW);
            SetFocus(s->busqueda);
        }
    } else if (IsWindowVisible(s->busqueda)) {
        ShowWindow(s->busqueda, SW_HIDE);
    }
}

void tickBusqueda(Estado* s) { pedirBusqueda(s); }

// ============================================================== mapa mental

int pintarMapa(Estado* s, HDC dc, int x, int y, int w) {
    const gui::Tema& tm = gui::tema();
    int y0 = y;
    bool cargado = s->mapa["ruta"].comoCadena() == s->resumenRuta && !s->tareaMapa.activa();
    if (!cargado) {
        RECT r = rect(x, y, w, E(s, 200));
        cristal(dc, r, E(s, 18));
        spinner(s, dc, x + w / 2, y + E(s, 70), E(s, 22));
        std::wstring t = s->tareaMapa.inicio ? L"La IA está dibujando el mapa mental…  " +
                                                   tiempo((int)((GetTickCount64() - s->tareaMapa.inicio) / 1000))
                                             : L"Preparando el mapa…";
        texto(dc, s->f.subtitulo, tm.texto, t, rect(x, y + E(s, 110), w, E(s, 28)), DT_CENTER | DT_SINGLELINE);
        if (s->tareaMapa.inicio && s->tareaMapa.caracteres)
            texto(dc, s->f.peque, tm.textoSuave, std::to_wstring(std::min(99, s->tareaMapa.caracteres * 100 / 900)) + L" %",
                  rect(x, y + E(s, 142), w, E(s, 20)), DT_CENTER | DT_SINGLELINE);
        return r.bottom - y0;
    }
    const js::Valor& ramas = s->mapa["ramas"];
    int n = (int)ramas.tam();
    int pad = E(s, 22);
    int anchoCentro = std::min(E(s, 230), w / 4);
    int cw = (w - anchoCentro - 2 * pad - 2 * E(s, 56)) / 2;
    int izquierdas = (n + 1) / 2;
    // Alto de cada tarjeta de rama
    std::vector<int> altos((std::size_t)n);
    for (int i = 0; i < n; ++i) {
        int h = E(s, 50);
        const js::Valor& ideas = ramas[(std::size_t)i]["ideas"];
        for (std::size_t k = 0; k < ideas.tam(); ++k) h += altoTexto(dc, s->f.peque, W(ideas[k]), cw - E(s, 44)) + E(s, 6);
        altos[(std::size_t)i] = h + E(s, 10);
    }
    int hueco = E(s, 16);
    int altoIzq = 0, altoDer = 0;
    for (int i = 0; i < n; ++i) (i < izquierdas ? altoIzq : altoDer) += altos[(std::size_t)i] + hueco;
    int alto = std::max({altoIzq, altoDer, E(s, 220)}) + 2 * pad;
    RECT lienzo = rect(x, y, w, alto);
    cristal(dc, lienzo, E(s, 18));
    int cx = x + w / 2, cy = y + alto / 2;
    RECT centro = rect(cx - anchoCentro / 2, cy - E(s, 38), anchoCentro, E(s, 76));
    std::wstring nombreCentro = W(s->mapa["centro"]);
    // Primero las conexiones (por debajo de las tarjetas)
    std::vector<RECT> tarjetas((std::size_t)n);
    int yi = y + pad + (alto - 2 * pad - altoIzq) / 2, yd = y + pad + (alto - 2 * pad - altoDer) / 2;
    for (int i = 0; i < n; ++i) {
        bool izq = i < izquierdas;
        int h = altos[(std::size_t)i];
        int tx = izq ? x + pad : x + w - pad - cw;
        int& ty = izq ? yi : yd;
        tarjetas[(std::size_t)i] = rect(tx, ty, cw, h);
        ty += h + hueco;
        COLORREF c = RAMAS[i % 6];
        float x1 = (float)(izq ? centro.left : centro.right), y1 = (float)cy;
        float x2 = (float)(izq ? tarjetas[(std::size_t)i].right : tarjetas[(std::size_t)i].left);
        float y2 = (float)(tarjetas[(std::size_t)i].top + E(s, 22));
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen lapiz(gc(mezcla(tm.panel, c, 0.75)), (float)E(s, 3));
        lapiz.SetStartCap(Gdiplus::LineCapRound);
        lapiz.SetEndCap(Gdiplus::LineCapRound);
        float medio = (x1 + x2) / 2;
        g.DrawBezier(&lapiz, x1, y1, medio, y1, medio, y2, x2, y2);
        Gdiplus::SolidBrush punto(gc(c));
        g.FillEllipse(&punto, x2 - E(s, 5), y2 - E(s, 5), (float)E(s, 10), (float)E(s, 10));
    }
    // Centro
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath camino;
        caminoRedondeado(camino, centro, E(s, 38));
        Gdiplus::LinearGradientBrush b(Gdiplus::PointF((float)centro.left, (float)centro.top),
                                       Gdiplus::PointF((float)centro.right, (float)centro.bottom), gc(tm.acento), gc(VIOLETA));
        g.FillPath(&b, &camino);
    }
    {
        // DT_VCENTER no centra texto de varias lineas: se calcula a mano.
        int hc = std::min(altoTexto(dc, s->f.subtitulo, nombreCentro, anchoCentro - E(s, 28)), E(s, 64));
        texto(dc, s->f.subtitulo, RGB(255, 255, 255), nombreCentro,
              rect(centro.left + E(s, 14), cy - hc / 2, anchoCentro - E(s, 28), hc), DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
    // Tarjetas de las ramas
    for (int i = 0; i < n; ++i) {
        const js::Valor& r = ramas[(std::size_t)i];
        RECT t = tarjetas[(std::size_t)i];
        COLORREF c = RAMAS[i % 6];
        redondeado(dc, t, E(s, 14), mezcla(tm.panel, c, tm.oscuro ? 0.10 : 0.07), mezcla(tm.borde, c, 0.6));
        redondeado(dc, rect(t.left, t.top + E(s, 12), E(s, 4), E(s, 22)), E(s, 2), c);
        texto(dc, s->f.negrita, mezcla(c, tm.texto, tm.oscuro ? 0.35 : 0.55), W(r["titulo"]),
              rect(t.left + E(s, 16), t.top + E(s, 10), cw - E(s, 28), E(s, 26)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        int iy = t.top + E(s, 46);
        const js::Valor& ideas = r["ideas"];
        for (std::size_t k = 0; k < ideas.tam(); ++k) {
            std::wstring idea = W(ideas[k]);
            int h = altoTexto(dc, s->f.peque, idea, cw - E(s, 44));
            circulo(dc, t.left + E(s, 22), iy + E(s, 8), E(s, 3), c);
            texto(dc, s->f.peque, tm.texto, idea, rect(t.left + E(s, 34), iy, cw - E(s, 44), h));
            iy += h + E(s, 6);
        }
    }
    return alto + E(s, 10);
}

// ============================================================ calendario

int pintarCalendario(Estado* s, HDC dc, int x, int y, int w) {
    const gui::Tema& tm = gui::tema();
    const js::Valor& cal = s->inicio["calendario"];
    if (cal.esNulo() || cal["dias"].tam() == 0) return 0;
    const js::Valor& dias = cal["dias"];
    int y0 = y;
    tituloSeccion(s, dc, x, y, w, L"Camino a " + W(cal["examen"]["nombre"]),
                  L"Tus temas repartidos por días, los más flojos primero. Pulsa un día para hacer un test de sus temas.");
    int n = (int)dias.tam();
    int hueco = E(s, 10);
    int columnas = std::max(1, std::min(n, (w + hueco) / (E(s, 190) + hueco)));
    int cw = (w - (columnas - 1) * hueco) / columnas;
    int filaY = y;
    for (int fila = 0; fila * columnas < n; ++fila) {
        int maxAlto = 0;
        for (int c = 0; c < columnas && fila * columnas + c < n; ++c) {
            const js::Valor& d = dias[(std::size_t)(fila * columnas + c)];
            const js::Valor& tareas = d["tareas"];
            int h = E(s, 52);
            for (std::size_t k = 0; k < tareas.tam(); ++k) h += altoTexto(dc, s->f.peque, W(tareas[k]), cw - E(s, 30)) + E(s, 6);
            maxAlto = std::max(maxAlto, h + E(s, 8));
        }
        for (int c = 0; c < columnas && fila * columnas + c < n; ++c) {
            int i = fila * columnas + c;
            const js::Valor& d = dias[(std::size_t)i];
            std::string tipo = d["tipo"].comoCadena();
            bool hoy = d["hoy"].comoBool();
            COLORREF color = tipo == "examen" ? tm.mal : tipo == "simulacro" ? ROSA : hoy ? tm.acento : tm.textoSuave;
            RECT r = rect(x + c * (cw + hueco), filaY, cw, maxAlto);
            bool pulsable = tipo != "examen";
            bool h = pulsable && encima(s, A_CAL_DIA, i);
            COLORREF fondo = tipo == "examen" ? mezcla(tm.panel, tm.mal, 0.12) : hoy ? mezcla(tm.panel, tm.acento, 0.12) : tm.panel;
            cristal(dc, r, E(s, 14), h ? tm.panelAlt : fondo, h || hoy || tipo == "examen" ? mezcla(tm.borde, color, 0.7) : CLR_INVALID,
                    hoy ? 2 : 1);
            icono(s, dc, tipo == "examen" ? I_DIANA : tipo == "simulacro" ? I_RELOJ : I_CALENDARIO, r.left + E(s, 24),
                  r.top + E(s, 24), E(s, 16), color);
            texto(dc, s->f.negrita, hoy || tipo == "examen" ? color : tm.texto, W(d["etiqueta"]),
                  rect(r.left + E(s, 42), r.top + E(s, 10), cw - E(s, 52), E(s, 28)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            int ty = r.top + E(s, 46);
            const js::Valor& tareas = d["tareas"];
            for (std::size_t k = 0; k < tareas.tam(); ++k) {
                std::wstring t = W(tareas[k]);
                int th = altoTexto(dc, s->f.peque, t, cw - E(s, 30));
                texto(dc, s->f.peque, tipo == "examen" ? tm.texto : tm.textoSuave, t, rect(r.left + E(s, 16), ty, cw - E(s, 30), th));
                ty += th + E(s, 6);
            }
            if (pulsable) zona(s, r, A_CAL_DIA, i);
        }
        filaY += maxAlto + hueco;
    }
    return filaY - y0 + E(s, 16);
}

// ================================================================ logros

int pintarLogros(Estado* s, HDC dc, int x, int y, int w) {
    const gui::Tema& tm = gui::tema();
    const js::Valor& l = s->logros;
    if (l.esNulo()) return 0;
    int y0 = y;
    tituloSeccion(s, dc, x, y, w, L"Nivel y logros", L"Ganas experiencia con cada pregunta, tarjeta, respuesta de desarrollo, "
                                                     L"test y minuto de estudio.");
    // --- Nivel
    {
        int alto = E(s, 120);
        RECT r = rect(x, y, w, alto);
        {
            Gdiplus::Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath camino;
            caminoRedondeado(camino, r, E(s, 18));
            Gdiplus::LinearGradientBrush b(Gdiplus::PointF((float)r.left, 0), Gdiplus::PointF((float)r.right, 0),
                                           gc(mezcla(tm.panel, ORO, tm.oscuro ? 0.16 : 0.12)),
                                           gc(mezcla(tm.panel, VIOLETA, tm.oscuro ? 0.14 : 0.08)));
            g.FillPath(&b, &camino);
        }
        contorno(dc, r, E(s, 18), mezcla(tm.borde, ORO, 0.5));
        int cx = x + E(s, 64), cy = y + alto / 2, radio = E(s, 40);
        int xp = l["xp"].comoEntero(), base = l["xp_nivel"].comoEntero(), sig = l["xp_siguiente"].comoEntero();
        double fr = (double)(xp - base) / std::max(1, sig - base);
        arco(dc, cx, cy, radio, (float)E(s, 7), 0, 360, mezcla(tm.panel, tm.fondo, 0.4));
        arco(dc, cx, cy, radio, (float)E(s, 7), -90, (float)(360.0 * std::max(0.03, fr)), ORO);
        texto(dc, s->f.titulo, tm.texto, std::to_wstring(l["nivel"].comoEntero()), rect(cx - radio, cy - E(s, 22), 2 * radio, E(s, 32)),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.mini, tm.textoSuave, L"nivel", rect(cx - radio, cy + E(s, 10), 2 * radio, E(s, 16)), DT_CENTER | DT_SINGLELINE);
        int tx = x + E(s, 126), tw = w - E(s, 146);
        wchar_t b[160];
        swprintf(b, 160, L"%d XP  ·  te faltan %d para el nivel %d", xp, sig - xp, l["nivel"].comoEntero() + 1);
        texto(dc, s->f.subtitulo, tm.texto, b, rect(tx, y + E(s, 22), tw, E(s, 26)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        barra(dc, rect(tx, y + E(s, 56), tw, E(s, 8)), fr, ORO);
        int hoy = l["minutos_hoy"].comoEntero(), semana = l["minutos_semana"].comoEntero(), total = l["minutos"].comoEntero();
        swprintf(b, 160, L"%d de %d logros  ·  tiempo con el temporizador: %d min hoy, %d esta semana, %d h %d min en total",
                 l["conseguidos"].comoEntero(), (int)l["logros"].tam(), hoy, semana, total / 60, total % 60);
        texto(dc, s->f.peque, tm.textoSuave, b, rect(tx, y + E(s, 78), tw, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        y = r.bottom + E(s, 14);
    }
    // --- Tiempo de concentracion de las dos ultimas semanas, con la linea del objetivo diario
    const js::Valor& dias = l["minutos_dias"];
    if (dias.tam() > 0) {
        RECT r = rect(x, y, w, E(s, 210));
        cristal(dc, r, E(s, 18));
        int objetivo = gui::estilo().objetivoDiario, maximo = std::max(objetivo, 30), total = 0, cumplidos = 0;
        for (std::size_t i = 0; i < dias.tam(); ++i) {
            int m = dias[i]["minutos"].comoEntero();
            maximo = std::max(maximo, m);
            total += m;
            if (objetivo > 0 && m >= objetivo) ++cumplidos;
        }
        icono(s, dc, I_FUEGO, x + E(s, 30), y + E(s, 32), E(s, 20), RGB(251, 146, 60));
        texto(dc, s->f.negrita, tm.texto, L"Tiempo de concentración", rect(x + E(s, 52), y + E(s, 18), w - E(s, 70), E(s, 26)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        wchar_t b[160];
        if (objetivo > 0)
            swprintf(b, 160, L"Últimos 14 días: %d h %d min  ·  objetivo de %d min cumplido %d %ls", total / 60, total % 60, objetivo, cumplidos,
                     cumplidos == 1 ? L"día" : L"días");
        else
            swprintf(b, 160, L"Últimos 14 días: %d h %d min con el temporizador", total / 60, total % 60);
        texto(dc, s->f.peque, tm.textoSuave, b, rect(x + E(s, 52), y + E(s, 42), w - E(s, 70), E(s, 20)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        int gx = x + E(s, 24), gw = w - E(s, 48), base = r.bottom - E(s, 34), altoMax = E(s, 96);
        int n = (int)dias.tam(), hueco = E(s, 8), bw = (gw - (n - 1) * hueco) / n;
        static const wchar_t* INICIAL[] = {L"L", L"M", L"X", L"J", L"V", L"S", L"D"};
        for (int i = 0; i < n; ++i) {
            const js::Valor& d = dias[(std::size_t)i];
            int m = d["minutos"].comoEntero();
            int bx = gx + i * (bw + hueco);
            int bh = m > 0 ? std::max(E(s, 4), altoMax * m / maximo) : E(s, 3);
            bool hoy = i == n - 1;
            COLORREF c = m == 0 ? mezcla(tm.panel, tm.borde, 0.9)
                         : objetivo > 0 && m >= objetivo ? RGB(52, 211, 153) : RGB(251, 146, 60);
            redondeado(dc, rect(bx, base - bh, bw, bh), std::min(E(s, 6), bw / 2), hoy && m == 0 ? tm.acento : c);
            if (m > 0)
                texto(dc, s->f.mini, tm.textoSuave, std::to_wstring(m), rect(bx - E(s, 6), base - bh - E(s, 18), bw + E(s, 12), E(s, 16)),
                      DT_CENTER | DT_SINGLELINE);
            // Dia de la semana debajo (la fecha llega como AAAA-MM-DD)
            Fecha f;
            std::wstring etiqueta = hoy ? L"hoy" : L"";
            if (!hoy && Fecha::desdeTexto(d["fecha"].comoCadena(), f)) etiqueta = INICIAL[f.diaSemana()];
            texto(dc, s->f.mini, hoy ? tm.texto : tm.textoSuave, etiqueta, rect(bx - E(s, 6), base + E(s, 8), bw + E(s, 12), E(s, 16)),
                  DT_CENTER | DT_SINGLELINE);
        }
        if (objetivo > 0) {  // linea del objetivo
            int oy = base - altoMax * objetivo / maximo;
            for (int px = gx; px < gx + gw; px += E(s, 10))
                linea(dc, (float)px, (float)oy, (float)std::min(px + E(s, 5), gx + gw), (float)oy, mezcla(tm.panel, RGB(52, 211, 153), 0.7), 1.2f);
            texto(dc, s->f.mini, mezcla(tm.textoSuave, RGB(52, 211, 153), 0.5), L"objetivo " + std::to_wstring(objetivo) + L" min",
                  rect(gx + gw - E(s, 140), oy - E(s, 18), E(s, 140), E(s, 16)), DT_RIGHT | DT_SINGLELINE);
        }
        y = r.bottom + E(s, 14);
    }
    // --- Insignias
    const js::Valor& lista = l["logros"];
    int columnas = w >= E(s, 900) ? 4 : w >= E(s, 600) ? 3 : 2, hueco = E(s, 12);
    int cw = (w - (columnas - 1) * hueco) / columnas, ch = E(s, 116);
    for (std::size_t i = 0; i < lista.tam(); ++i) {
        const js::Valor& g = lista[i];
        bool ok = g["conseguido"].comoBool();
        RECT r = rect(x + (int)(i % columnas) * (cw + hueco), y + (int)(i / columnas) * (ch + hueco), cw, ch);
        cristal(dc, r, E(s, 14), ok ? mezcla(tm.panel, ORO, tm.oscuro ? 0.10 : 0.08) : CLR_INVALID,
                ok ? mezcla(tm.borde, ORO, 0.6) : CLR_INVALID);
        int cx = r.left + E(s, 32), cy = r.top + E(s, 34);
        circulo(dc, cx, cy, E(s, 19), ok ? mezcla(tm.panel, ORO, 0.30) : tm.panelAlt);
        glifo(s, dc, I_TROFEO, cx, cy, E(s, 20), ok ? ORO : mezcla(tm.textoSuave, tm.panel, 0.3));
        texto(dc, s->f.negrita, ok ? tm.texto : tm.textoSuave, W(g["nombre"]), rect(r.left + E(s, 60), r.top + E(s, 14), cw - E(s, 70), E(s, 22)),
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.mini, tm.textoSuave, W(g["descripcion"]), rect(r.left + E(s, 60), r.top + E(s, 38), cw - E(s, 70), E(s, 34)),
              DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        if (ok) {
            texto(dc, s->f.peque, mezcla(ORO, tm.texto, 0.35), L"✓ Conseguido", rect(r.left + E(s, 16), r.bottom - E(s, 30), cw - E(s, 32), E(s, 20)),
                  DT_LEFT | DT_SINGLELINE);
        } else {
            barra(dc, rect(r.left + E(s, 16), r.bottom - E(s, 22), cw - E(s, 80), E(s, 6)), g["progreso"].comoNumero(), ORO);
            int valor = std::min(g["valor"].comoEntero(), g["objetivo"].comoEntero());
            texto(dc, s->f.mini, tm.textoSuave, std::to_wstring(valor) + L"/" + std::to_wstring(g["objetivo"].comoEntero()),
                  rect(r.right - E(s, 60), r.bottom - E(s, 30), E(s, 46), E(s, 20)), DT_RIGHT | DT_SINGLELINE);
        }
    }
    y += (int)((lista.tam() + columnas - 1) / columnas) * (ch + hueco) + E(s, 18);
    // --- Exportar
    {
        RECT r = rect(x, y, w, E(s, 92));
        cristal(dc, r, E(s, 16));
        icono(s, dc, I_EXPORTAR, x + E(s, 34), y + E(s, 46), E(s, 22), tm.acento);
        texto(dc, s->f.negrita, tm.texto, L"Llévatelo contigo", rect(x + E(s, 64), y + E(s, 20), w - E(s, 520), E(s, 24)),
              DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.peque, tm.textoSuave, L"Un dossier para leer o imprimir, o tus tarjetas para Anki en el móvil.",
              rect(x + E(s, 64), y + E(s, 48), w - E(s, 520), E(s, 22)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        botonIcono(s, dc, rect(r.right - E(s, 440), y + E(s, 24), E(s, 210), E(s, 44)), I_LIBRO, L"Dossier de estudio", A_EXPORTAR, 0);
        botonIcono(s, dc, rect(r.right - E(s, 220), y + E(s, 24), E(s, 200), E(s, 44)), I_TARJETAS, L"Tarjetas a Anki", A_EXPORTAR, 1);
        y = r.bottom + E(s, 10);
    }
    return y - y0;
}

// ============================================================== pomodoro

int pintarPomodoro(Estado* s, HDC dc, int derecha, int alto) {
    const COLORREF tinta = gui::textoBarraSuperior(), fondoChip = mezcla(gui::colorBarraSuperior(), RGB(255, 255, 255), 0.06),
                   encimaFondo = mezcla(gui::colorBarraSuperior(), RGB(255, 255, 255), 0.14);
    COLORREF color = s->pomodoro == 1 ? RGB(251, 146, 60) : s->pomodoro == 2 ? RGB(52, 211, 153) : RGB(150, 156, 168);
    std::wstring t;
    int resto = restantePomodoro(s);
    if (s->pomodoro == 0) t = L"Concentración";
    else t = (s->pomodoro == 1 ? L"Estudiando  " : L"Descanso  ") + tiempo(resto);
    int tw = anchoTexto(dc, s->f.peque, t) + E(s, 48) + (s->pomodoro ? E(s, 26) : 0);
    RECT r = rect(derecha - tw, (alto - E(s, 30)) / 2, tw, E(s, 30));
    bool h = encima(s, A_POMODORO);
    redondeado(dc, r, E(s, 15), h ? encimaFondo : fondoChip, mezcla(fondoChip, color, 0.5));
    if (s->pomodoro == 1 && !s->pomodoroPausa) {
        // Anillo con lo que queda del pomodoro
        int cx = r.left + E(s, 17), cy = (r.top + r.bottom) / 2;
        arco(dc, cx, cy, E(s, 8), (float)E(s, 2), 0, 360, mezcla(fondoChip, color, 0.3));
        arco(dc, cx, cy, E(s, 8), (float)E(s, 2), -90, (float)(360.0 * resto / std::max(1, s->pomodoroDuracion)), color);
    } else {
        icono(s, dc, s->pomodoroPausa ? I_PAUSA : s->pomodoro == 2 ? I_BOMBILLA : I_RELOJ, r.left + E(s, 17), (r.top + r.bottom) / 2,
              E(s, 15), color);
    }
    texto(dc, s->f.peque, tinta, s->pomodoroPausa ? t + L"  (pausa)" : t, rect(r.left + E(s, 32), r.top, tw - E(s, 32), r.bottom - r.top),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    zona(s, s->pomodoro ? rect(r.left, r.top, tw - E(s, 26), r.bottom - r.top) : r, A_POMODORO);
    if (s->pomodoro) {
        RECT x = rect(r.right - E(s, 26), r.top, E(s, 22), r.bottom - r.top);
        COLORREF cx = encima(s, A_POMODORO_PARAR) ? tinta : RGB(150, 156, 168);
        int mx = (x.left + x.right) / 2, my = (x.top + x.bottom) / 2, k = E(s, 4);
        linea(dc, (float)(mx - k), (float)(my - k), (float)(mx + k), (float)(my + k), cx, 1.6f);
        linea(dc, (float)(mx + k), (float)(my - k), (float)(mx - k), (float)(my + k), cx, 1.6f);
        zona(s, x, A_POMODORO_PARAR);
    }
    return tw;
}

void tickPomodoro(Estado* s) {
    if (!s->pomodoro || s->pomodoroPausa) return;
    if (restantePomodoro(s) > 0) return;
    if (gui::estilo().campana) musica::campana(s->pomodoro == 1 ? 0 : 1);
    else MessageBeep(MB_ICONASTERISK);
    if (s->pomodoro == 1) {
        registrarMinutos(s, s->pomodoroDuracion / 60);
        s->mensaje = L"¡Pomodoro completado! Levántate, bebe agua y descansa " +
                     std::to_wstring(segundosDescanso() / 60) + (segundosDescanso() == 60 ? L" minuto." : L" minutos.");
        aviso(s, L"Pomodoro completado: +" + std::to_wstring(s->pomodoroDuracion / 60) + L" minutos de estudio");
        empezarPomodoro(s, 2, segundosDescanso());
        if (gui::estilo().respiracion) abrirDescanso(s);
    } else {
        s->pomodoro = 0;
        KillTimer(s->ventana, TIMER_POMODORO);
        s->mensaje = L"Descanso terminado. ¿Vamos con otro pomodoro?";
        aviso(s, L"Descanso terminado: ¡a por el siguiente!");
    }
}

// ============================================================== selector

void pintarSelector(Estado* s, HDC dc, RECT lado) {
    if (!s->selector) return;
    const gui::Tema& tm = gui::tema();
    RECT cliente;
    GetClientRect(s->ventana, &cliente);
    // Velo sobre todo; un clic fuera cierra el selector.
    velo(dc, rect(lado.left, lado.top, cliente.right - lado.left, cliente.bottom - lado.top), RGB(0, 0, 0), tm.oscuro ? 90 : 50);
    zona(s, cliente, A_CERRAR_SELECTOR);
    int x = lado.left + E(s, 12), w = std::max((int)(lado.right - lado.left) - E(s, 24), E(s, 320));
    int filaAlto = E(s, 52);
    int n = (int)s->todas.size();
    RECT panel = rect(x, lado.top + E(s, 80), w, E(s, 56) + n * filaAlto);
    redondeado(dc, panel, E(s, 16), tm.panel, tm.borde);  // opaco: no se ve lo de debajo
    texto(dc, s->f.mini, tm.textoSuave, L"CAMBIAR DE ASIGNATURA", rect(x + E(s, 18), panel.top + E(s, 16), w, E(s, 18)),
          DT_LEFT | DT_SINGLELINE);
    int y = panel.top + E(s, 44);
    for (int i = 0; i < n; ++i) {
        const Asignatura& a = s->todas[(std::size_t)i];
        RECT r = rect(x + E(s, 8), y, w - E(s, 16), filaAlto - E(s, 4));
        bool actual = a.codigo == s->asignatura.codigo;
        bool h = encima(s, A_ASIGNATURA, i);
        if (actual) redondeado(dc, r, E(s, 10), mezcla(tm.panel, tm.acento, 0.16));
        else if (h) redondeado(dc, r, E(s, 10), tm.panelAlt);
        COLORREF c = gui::colorDeAsignatura(a.color, (std::size_t)i);
        circulo(dc, r.left + E(s, 18), (r.top + r.bottom) / 2, E(s, 6), c);
        texto(dc, s->f.negrita, tm.texto, W(a.codigo), rect(r.left + E(s, 34), r.top + E(s, 4), E(s, 70), E(s, 22)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.peque, tm.textoSuave, W(a.nombre), rect(r.left + E(s, 34), r.top + E(s, 24), r.right - r.left - E(s, 70), E(s, 20)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (actual) icono(s, dc, I_CHECK, r.right - E(s, 20), (r.top + r.bottom) / 2, E(s, 14), tm.acento);
        else zona(s, r, A_ASIGNATURA, i);
        y += filaAlto;
    }
}

// ========================================================= mensajes y acciones

bool animandoHerramientas(Estado* s) {
    return s->tareaMapa.activa() || (!s->toast.empty() && GetTickCount64() < s->toastHasta + 100);
}

void abrirHerramientas(Estado* s, Vista v) {
    if (v != V_BUSCAR) return;
    irAVista(s, V_BUSCAR);
    if (s->glosario.esNulo()) enviar(s, "glosario");
    s->mensaje = L"Busca cualquier cosa en tus apuntes, o repasa el glosario de conceptos.";
}

void buscarEnTemario(Estado* s, const std::wstring& texto) {
    abrirHerramientas(s, V_BUSCAR);
    s->modoBusqueda = 0;
    s->resultados = js::Valor();
    SetWindowTextW(s->busqueda, texto.c_str());
    pedirBusqueda(s);
}

void empezarDescanso(Estado* s) {
    empezarPomodoro(s, 2, segundosDescanso());
    s->mensaje = L"Descanso de " + std::to_wstring(segundosDescanso() / 60) + L" minutos. Aléjate un poco de la pantalla.";
    abrirDescanso(s);
}

void empezarConcentracion(Estado* s) {
    s->pantallaDescanso = false;
    empezarPomodoro(s, 1, segundosEstudio());
    s->mensaje = L"Pomodoro en marcha: " + std::to_wstring(segundosEstudio() / 60) +
                 L" minutos de concentración. Silencia el móvil.";
    if (gui::estilo().musicaAlConcentrarse && musica::sonando() == musica::NADA) musica::tocar(musica::ultimo());
}

bool herramientasRespuesta(Estado* s, const std::string& orden, const js::Valor& r) {
    if (orden == "inicio" || orden == "examenes") {
        // Logros y subidas de nivel desde la ultima vez
        const js::Valor& antes = s->inicio["nivel"];
        const js::Valor& ahora = r["nivel"];
        if (!antes.esNulo() && !ahora.esNulo()) {
            if (ahora["nivel"].comoEntero() > antes["nivel"].comoEntero())
                aviso(s, L"¡Subes al nivel " + std::to_wstring(ahora["nivel"].comoEntero()) + L"!");
            else if (ahora["conseguidos"].comoEntero() > antes["conseguidos"].comoEntero())
                aviso(s, L"Logro conseguido: " + W(ahora["ultimo"]));
        }
        return false;  // el resto lo atiende estudioRespuesta
    }
    if (orden == "progreso") {
        enviar(s, "logros");
        return false;
    }
    if (orden == "hola") {
        enviar(s, "glosario");  // para el buscador y la cifra de Inicio
        return false;
    }
    if (orden == "glosario") {
        s->glosario = r;
        return true;
    }
    if (orden == "buscar") {
        if (r["consulta"].comoCadena() == gui::aUtf8(textoDe(s->busqueda))) s->resultados = r;
        return true;
    }
    if (orden == "mapa") {
        s->tareaMapa = TareaIA();
        s->mapa = r;
        if (r["origen"].comoCadena() == "ia") s->mensaje = L"Aquí tienes el mapa mental. Intenta reproducirlo de memoria en un papel.";
        return true;
    }
    if (orden == "logros") {
        s->logros = r;
        return true;
    }
    if (orden == "registrar_estudio") {
        if (r.contiene("logros")) s->logros = r["logros"];
        enviar(s, "inicio");
        return true;
    }
    if (orden == "exportar") {
        std::wstring archivo = W(r["archivo"]);
        if (r.contiene("cantidad")) {
            std::wstring carpeta = archivo.substr(0, archivo.find_last_of(L"\\/"));
            gui::abrirEnSistema(s->ventana, carpeta);
            aviso(s, plural(r["cantidad"].comoEntero(), L"tarjeta exportada", L"tarjetas exportadas") + L" para Anki");
            s->mensaje = L"En Anki: Archivo › Importar y elige el fichero «tarjetas-anki». Se separan por temas con etiquetas.";
        } else {
            gui::abrirEnSistema(s->ventana, archivo);
            aviso(s, L"Dossier creado: se abre en tu navegador");
            s->mensaje = L"Desde el navegador puedes imprimirlo o guardarlo en PDF (Ctrl+P).";
        }
        return true;
    }
    return false;
}

bool herramientasError(Estado* s, const std::string& orden, const std::wstring& m) {
    if (orden == "mapa") {
        s->tareaMapa = TareaIA();
        s->mensaje = m;
        return true;
    }
    if (orden == "buscar" || orden == "glosario" || orden == "logros" || orden == "registrar_estudio") {
        s->mensaje = m;
        return true;
    }
    if (orden == "exportar") {
        gui::avisar(s->ventana, m);
        return true;
    }
    return false;
}

bool herramientasEvento(Estado* s, const js::Valor& ev) {
    if (ev["evento"].comoCadena() == "parcial" && s->tareaMapa.activa() && ev["id"].comoEntero() == s->tareaMapa.id) {
        s->tareaMapa.caracteres = ev["caracteres"].comoEntero();
        return true;
    }
    if (ev["evento"].comoCadena() == "temas") {  // la IA ha puesto nombre a los temas
        std::string estado = ev["estado"].comoCadena();
        s->temasIa = estado == "trabajando" ? "trabajando" : "";
        if (estado == "listo") {
            const js::Valor& g = ev["grupos"];
            for (js::Valor* v : {&s->hola, &s->resumenes, &s->desarrollo, &s->progreso, &s->temario})
                if (v->esObjeto()) aplicarGrupos((*v)[v == &s->hola ? "archivos" : "temas"], g);
            aviso(s, L"He ordenado tus apuntes por temas.");
        }
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    return false;
}

bool herramientasAccion(Estado* s, int accion, int dato) {
    HWND w = s->ventana;
    switch (accion) {
        case A_B_MODO:
            if (s->modoBusqueda != dato) SetWindowTextW(s->busqueda, L"");  // buscar y filtrar son cosas distintas
            s->modoBusqueda = dato;
            s->scroll = 0;
            s->resultados = js::Valor();
            if (dato == 1 && s->glosario.esNulo()) enviar(s, "glosario");
            if (dato == 0) pedirBusqueda(s);
            SetFocus(s->busqueda);
            return true;
        case A_B_LETRA:
            s->letra = dato ? std::wstring(1, (wchar_t)dato) : L"";
            return true;
        case A_B_RESULTADO: {
            const js::Valor& r = s->resultados["resultados"][(std::size_t)dato];
            if (!gui::abrirEnSistema(w, W(r["archivo"]))) s->mensaje = L"No he podido abrir el documento.";
            else s->mensaje = L"Lo encontrarás en " + W(r["ubicacion"]) + L" de «" + W(r["tema"]) + L"».";
            return true;
        }
        case A_B_CONCEPTO: {
            if (dato >= 10000 && dato < 20000) {  // sugerencia: buscarla
                SetWindowTextW(s->busqueda, W(s->glosario["glosario"][(std::size_t)(dato - 10000)]["termino"]).c_str());
                pedirBusqueda(s);
                return true;
            }
            const js::Valor& c = dato >= 20000 ? s->resultados["conceptos"][(std::size_t)(dato - 20000)]
                                               : s->glosario["glosario"][(std::size_t)dato];
            if (!gui::abrirEnSistema(w, W(c["archivo"]))) s->mensaje = L"No he podido abrir el documento.";
            else s->mensaje = L"«" + W(c["termino"]) + L"» sale en " + W(c["tema"]) + L", " + W(c["ubicacion"]) + L".";
            return true;
        }
        case A_R_VISTA:
            if (dato == 101) {  // desde Inicio: abrir Resumenes ya con el mapa
                s->verMapa = true;
                abrirEstudio(s, V_RESUMENES);
                if (!s->resumenes.esNulo()) herramientasAccion(s, A_R_VISTA, 1);
                return true;
            }
            s->verMapa = dato == 1;
            if (s->verMapa && s->mapa["ruta"].comoCadena() != s->resumenRuta) pedirMapa(s, false, false);
            return true;
        case A_R_MAPA: pedirMapa(s, true, dato == 1); return true;
        case A_CAL_DIA: {
            const js::Valor& d = s->inicio["calendario"]["dias"][(std::size_t)dato];
            js::Valor c = configParaEnviar(s);
            if (d["tipo"].comoCadena() == "simulacro") {
                c["simulacro"] = true;
                c["n"] = 20;
                empezarCon(s, c, L"Preparando tu simulacro de examen…");
                return true;
            }
            js::Valor temas = js::Valor::array();
            for (std::size_t i = 0; i < d["rutas"].tam(); ++i) temas.agregar(d["rutas"][i].comoCadena());
            c["temas"] = temas;
            empezarCon(s, c, L"Preparando un test de los temas del día…");
            return true;
        }
        case A_POMODORO:
            if (s->pomodoro == 0) {
                empezarConcentracion(s);
            } else if (s->pomodoro == 2) {
                s->pomodoro = 0;
                KillTimer(w, TIMER_POMODORO);
            } else if (s->pomodoroPausa) {
                s->pomodoroInicio += GetTickCount64() - s->pomodoroPausadoEn;
                s->pomodoroPausa = false;
            } else {
                s->pomodoroPausadoEn = GetTickCount64();
                s->pomodoroPausa = true;
            }
            return true;
        case A_POMODORO_PARAR:
            if (s->pomodoro == 1) {
                int minutos = (s->pomodoroDuracion - restantePomodoro(s)) / 60;
                if (minutos >= 5) {
                    registrarMinutos(s, minutos);
                    aviso(s, L"+" + std::to_wstring(minutos) + L" minutos de estudio guardados");
                }
            }
            s->pomodoro = 0;
            s->pomodoroPausa = false;
            KillTimer(w, TIMER_POMODORO);
            return true;
        case A_EXPORTAR: {
            js::Valor p = js::Valor::objeto();
            p["tipo"] = dato == 1 ? "anki" : "dossier";
            enviar(s, "exportar", p, true, dato == 1 ? L"Exportando tus tarjetas…" : L"Preparando tu dossier…");
            return true;
        }
        case A_SELECTOR:
            s->selector = !s->selector && s->todas.size() > 1;
            return true;
        case A_CERRAR_SELECTOR: s->selector = false; return true;
        case A_ASIGNATURA:
            if (dato >= 0 && dato < (int)s->todas.size()) cambiarAsignatura(s, s->todas[(std::size_t)dato]);
            return true;
        default: return false;
    }
}

}  // namespace ui
}  // namespace profesor
