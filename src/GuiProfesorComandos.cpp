// GuiProfesorComandos.cpp - Paleta de comandos (Ctrl+K) y ayuda de atajos (F1) del profesor.
//
// La paleta es una caja de busqueda flotante: escribes unas letras («resu ia», «tormenta»,
// «simul») y aparece todo lo que coincide: pantallas, acciones, temas de tus apuntes (test o
// resumen de cada uno), musica de fondo y cambio de asignatura. Flechas para moverse, Intro
// para abrir y Esc para cerrar. No distingue mayusculas ni tildes.
#include "ProfesorUi.hpp"

#include <algorithm>
#include <cwctype>
#include <functional>

#include "Musica.hpp"

namespace profesor {
namespace ui {

namespace {

const int FILAS_VISIBLES = 8;
const COLORREF ORO = RGB(250, 204, 21);

struct Comando {
    std::wstring titulo;
    std::wstring detalle;
    std::wstring grupo;
    Icono icono;
    COLORREF color;
    std::wstring atajo;
    std::function<void(Estado*)> hacer;
    bool soloBuscando = false;  // no sale con la caja vacia (temas: serian demasiados)
    int puntos = 0;
};

wchar_t sinTilde(wchar_t c) {
    c = (wchar_t)towlower(c);
    switch (c) {
        case L'á': case L'à': case L'ä': return L'a';
        case L'é': case L'è': case L'ë': return L'e';
        case L'í': case L'ì': case L'ï': return L'i';
        case L'ó': case L'ò': case L'ö': return L'o';
        case L'ú': case L'ù': case L'ü': return L'u';
        default: return c;
    }
}

std::wstring normal(const std::wstring& t) {
    std::wstring r = t;
    for (wchar_t& c : r) c = sinTilde(c);
    return r;
}

std::vector<std::wstring> palabras(const std::wstring& t) {
    std::vector<std::wstring> v;
    std::wstring actual;
    for (wchar_t c : normal(t)) {
        if (iswspace(c) || c == L'«' || c == L'»' || c == L'·') {
            if (!actual.empty()) v.push_back(actual);
            actual.clear();
        } else {
            actual += c;
        }
    }
    if (!actual.empty()) v.push_back(actual);
    return v;
}

// Puntuacion de un comando para lo escrito (-1 = no coincide). Todas las palabras tienen que
// aparecer; puntua mas si estan al principio del titulo o de una palabra del titulo.
int puntuar(const Comando& c, const std::wstring& consulta) {
    std::vector<std::wstring> ps = palabras(consulta);
    if (ps.empty()) return c.soloBuscando ? -1 : 0;
    std::wstring titulo = normal(c.titulo), todo = titulo + L" " + normal(c.detalle) + L" " + normal(c.grupo);
    int puntos = 0;
    for (const std::wstring& p : ps) {
        std::size_t en = todo.find(p);
        if (en == std::wstring::npos) return -1;
        std::size_t enTitulo = titulo.find(p);
        if (enTitulo == 0) puntos += 60;
        else if (enTitulo != std::wstring::npos && !iswalnum(titulo[enTitulo - 1])) puntos += 40;
        else if (enTitulo != std::wstring::npos) puntos += 20;
        else puntos += 5;
    }
    if (titulo.find(normal(consulta)) == 0) puntos += 50;
    return puntos;
}

void irA(Estado* s, Vista v) { ejecutar(s, A_PESTANA, v); }

void testDeTema(Estado* s, const std::string& ruta, const std::wstring& tema) {
    js::Valor c = configParaEnviar(s);
    js::Valor t = js::Valor::array();
    t.agregar(ruta);
    c["temas"] = t;
    empezarCon(s, c, L"Preparando un test de «" + tema + L"»…");
}

void resumenDeTema(Estado* s, const std::string& ruta) {
    if (s->resumenes.esObjeto()) {
        const js::Valor& temas = s->resumenes["temas"];
        abrirEstudio(s, V_RESUMENES);
        for (std::size_t i = 0; i < temas.tam(); ++i)
            if (temas[i]["ruta"].comoCadena() == ruta) ejecutar(s, A_R_TEMA, (int)i);
        return;
    }
    s->resumenRuta = ruta;  // se pide al llegar la lista de temas
    abrirEstudio(s, V_RESUMENES);
}

std::vector<Comando> todos(Estado* s) {
    const gui::Tema& tm = gui::tema();
    const COLORREF VIOLETA = RGB(167, 139, 250), TURQUESA = RGB(45, 212, 191), NARANJA = RGB(251, 146, 60);
    std::vector<Comando> v;
    auto add = [&](const std::wstring& t, const std::wstring& d, const std::wstring& g, Icono i, COLORREF c,
                   const std::wstring& atajo, std::function<void(Estado*)> f) {
        Comando k;
        k.titulo = t;
        k.detalle = d;
        k.grupo = g;
        k.icono = i;
        k.color = c;
        k.atajo = atajo;
        k.hacer = f;
        v.push_back(k);
    };
    bool listo = s->vista != V_ARRANCANDO && s->vista != V_ERROR;
    const js::Valor& res = s->hola["resumen"];
    if (listo) {
        // --- Ir a
        add(L"Inicio", L"Tu plan de hoy y el camino al próximo examen", L"Ir a", I_INICIO, tm.acento, L"", [](Estado* e) { irA(e, V_HUB); });
        if (hayTest(s) && !testTerminado(s))
            add(L"Seguir con el test", std::to_wstring(s->sesion["respondidas"].comoEntero()) + L" de " +
                                           std::to_wstring(s->sesion["n"].comoEntero()) + L" preguntas respondidas",
                L"Ir a", I_JUGAR, tm.acento, L"", [](Estado* e) { irA(e, V_PREGUNTA); });
        add(L"Nuevo test", L"Elige temas, número de preguntas y dificultad", L"Ir a", I_TEST, tm.acento, L"",
            [](Estado* e) { irA(e, V_INICIO); });
        add(L"Tarjetas", L"Repaso espaciado de conceptos", L"Ir a", I_TARJETAS, ORO, L"", [](Estado* e) { irA(e, V_TARJETAS); });
        add(L"Resúmenes", L"El resumen de cada tema", L"Ir a", I_RESUMEN, TURQUESA, L"", [](Estado* e) { irA(e, V_RESUMENES); });
        add(L"Mapas mentales", L"Cada tema de un vistazo", L"Ir a", I_MAPA, TURQUESA, L"",
            [](Estado* e) { herramientasAccion(e, A_R_VISTA, 101); });
        add(L"Temario", L"Lee tus apuntes aquí mismo, tema a tema", L"Ir a", I_LIBRO, RGB(96, 165, 250), L"",
            [](Estado* e) { irA(e, V_TEMARIO); });
        {
            // El documento que se estaba leyendo, si se quedo a medias
            const js::Valor& docs = s->temario["temas"];
            std::string ultimo = s->temario["ultimo"].comoCadena();
            for (std::size_t i = 0; i < docs.tam() && !ultimo.empty(); ++i) {
                const js::Valor& d = docs[i];
                double pos = d["posicion"].comoNumero();
                if (d["ruta"].comoCadena() != ultimo || d["leido"].comoBool() || pos <= 0.02) continue;
                add(L"Seguir leyendo «" + W(d["tema"]) + L"»", L"Vas por el " + std::to_wstring((int)(pos * 100 + 0.5)) + L" %",
                    L"Ir a", I_LIBRO, RGB(96, 165, 250), L"", [ultimo](Estado* e) { abrirTemario(e, ultimo); });
            }
        }
        add(L"Desarrollo", L"Preguntas para responder por escrito, corregidas por la IA", L"Ir a", I_DESARROLLO, VIOLETA, L"",
            [](Estado* e) { irA(e, V_DESARROLLO); });
        add(L"Buscar y glosario", L"Encuentra un concepto en tus apuntes", L"Ir a", I_BUSCAR, tm.acento, L"Ctrl+F",
            [](Estado* e) { irA(e, V_BUSCAR); });
        if (iaDisponible(s)) add(L"Dudas", L"Pregúntale al profesor", L"Ir a", I_DUDAS, tm.acento, L"", [](Estado* e) { irA(e, V_CHAT); });
        add(L"Progreso", L"Notas, actividad, temas flojos y logros", L"Ir a", I_PROGRESO, tm.ok, L"", [](Estado* e) { irA(e, V_PROGRESO); });
        int fallos = res["mis_fallos"].comoEntero();
        add(L"Mis fallos", fallos ? plural(fallos, L"pregunta guardada", L"preguntas guardadas") : L"Las preguntas que fallas", L"Ir a", I_FALLOS,
            tm.medio, L"", [](Estado* e) { irA(e, V_FALLOS); });

        // --- Estudiar
        add(L"Simulacro de examen", L"20 preguntas  ·  1 min cada una  ·  los fallos restan", L"Estudiar", I_RELOJ, tm.mal, L"",
            [](Estado* e) { ejecutar(e, A_SIMULACRO, 0); });
        add(L"Repaso de lo que toca hoy", L"Las preguntas que toca repasar para que no se olviden", L"Estudiar", I_REPETIR, tm.medio, L"",
            [](Estado* e) { ejecutar(e, A_REPASO, 0); });
        add(L"Hacer un test", L"Empieza, sigue o configura un test sin salir de la pantalla", L"Estudiar", I_TEST, tm.acento,
            L"Ctrl+T", [](Estado* e) { e->panelTest = true; });
        add(L"Test rápido de 10 preguntas", L"De todos los temas, en modo rápido", L"Estudiar", I_JUGAR, tm.acento, L"", [](Estado* e) {
            js::Valor c = configParaEnviar(e);
            c["temas"] = js::Valor::array();  // de todos los temas, como dice
            c["n"] = 10;
            c["modo"] = "rapido";
            empezarCon(e, c, L"Preparando un test rápido…");
        });
        if (fallos)
            add(L"Test de mis fallos", plural(fallos, L"pregunta que fallaste", L"preguntas que fallaste"), L"Estudiar", I_FALLOS, tm.medio, L"",
                [](Estado* e) { ejecutar(e, A_TEST_FALLOS, 0); });
        add(L"Repasar tarjetas", std::to_wstring(res["tarjetas_pendientes"].comoEntero()) + L" pendientes  ·  " +
                                     std::to_wstring(res["tarjetas_nuevas"].comoEntero()) + L" nuevas",
            L"Estudiar", I_TARJETAS, ORO, L"", [](Estado* e) {
                irA(e, V_TARJETAS);
                ejecutar(e, A_T_EMPEZAR, 0);
            });
    }

    // --- Concentracion
    if (s->pomodoro == 0) {
        add(L"Concentrarme " + std::to_wstring(gui::estilo().pomodoro) + L" minutos", L"Temporizador de estudio (pomodoro)",
            L"Concentración", I_FUEGO, NARANJA, L"", [](Estado* e) { empezarConcentracion(e); });
        add(L"Descanso de " + std::to_wstring(gui::estilo().descanso) + L" minutos", L"Con respiración guiada", L"Concentración",
            I_BOMBILLA, tm.ok, L"", [](Estado* e) { ejecutar(e, A_DESCANSO, 0); });
    } else {
        add(s->pomodoroPausa ? L"Reanudar el temporizador" : L"Pausar el temporizador", L"", L"Concentración", I_PAUSA, NARANJA, L"",
            [](Estado* e) { ejecutar(e, A_POMODORO, 0); });
        add(L"Parar el temporizador", L"Se guardan los minutos ya estudiados (desde 5)", L"Concentración", I_RELOJ, NARANJA, L"",
            [](Estado* e) { ejecutar(e, A_POMODORO_PARAR, 0); });
        if (s->pomodoro == 2)
            add(L"Ver la pantalla de descanso", L"Respiración guiada", L"Concentración", I_BOMBILLA, tm.ok, L"",
                [](Estado* e) { abrirDescanso(e); });
    }
    add(s->enfoque ? L"Salir del modo enfoque" : L"Modo enfoque", L"Pantalla completa sin distracciones", L"Concentración", I_DIANA,
        tm.acento, L"F9", [](Estado* e) { alternarEnfoque(e); });

    // --- Musica
    for (int m = musica::LLUVIA; m < musica::N_SONIDOS; ++m) {
        int suena = musica::sonando();
        add(L"Música: " + std::wstring(musica::nombre(m)), m == suena ? L"Sonando ahora" : musica::descripcion(m), L"Música", I_BOMBILLA,
            musica::color(m), L"", [m](Estado* e) { enfoqueAccion(e, A_MUSICA_SONIDO, m); });
        v.back().icono = (Icono)-1;  // se dibuja una nota
    }
    if (musica::sonando())
        add(L"Parar la música", musica::nombre(musica::sonando()), L"Música", I_PAUSA, tm.textoSuave, L"Ctrl+M",
            [](Estado* e) { enfoqueAccion(e, A_MUSICA_SONIDO, musica::NADA); });
    add(L"Panel de música y volumen", L"", L"Música", I_BOMBILLA, tm.acento, L"", [](Estado* e) { e->panelMusica = true; });
    v.back().icono = (Icono)-1;

    // --- Temas de los apuntes (solo buscando)
    if (listo) {
        const js::Valor& archivos = s->hola["archivos"];
        for (std::size_t i = 0; i < archivos.tam(); ++i) {
            const js::Valor& a = archivos[i];
            if (!a["error"].comoCadena().empty() || a["fragmentos"].comoEntero() == 0) continue;
            std::string ruta = a["ruta"].comoCadena();
            std::wstring tema = W(a["tema"]), grupo = W(a["grupo"]);
            add(L"Test de «" + tema + L"»", grupo, L"Tema", I_DIANA, tm.acento, L"",
                [ruta, tema](Estado* e) { testDeTema(e, ruta, tema); });
            v.back().soloBuscando = true;
            add(L"Resumen de «" + tema + L"»", grupo, L"Tema", I_RESUMEN, TURQUESA, L"", [ruta](Estado* e) { resumenDeTema(e, ruta); });
            v.back().soloBuscando = true;
            add(L"Leer «" + tema + L"»", grupo, L"Tema", I_LIBRO, RGB(96, 165, 250), L"", [ruta](Estado* e) { abrirTemario(e, ruta); });
            v.back().soloBuscando = true;
        }
        if (iaDisponible(s))
            add(L"Ordenar los temas con la IA", L"Vuelve a poner nombre a tus temas (tarda un minuto)", L"Tema", I_CHISPA, VIOLETA, L"",
                [](Estado* e) { comandosAccion(e, A_TEMAS_IA, 0); });
    }

    // --- Asignaturas
    for (std::size_t i = 0; i < s->todas.size(); ++i) {
        const Asignatura& a = s->todas[i];
        if (a.codigo == s->asignatura.codigo) continue;
        int k = (int)i;
        add(L"Cambiar a " + W(a.codigo), W(a.nombre), L"Asignatura", I_LIBRO, gui::colorDeAsignatura(a.color, i), L"",
            [k](Estado* e) { ejecutar(e, A_ASIGNATURA, k); });
    }

    // --- Ventana y datos
    add(s->pantallaCompleta ? L"Salir de pantalla completa" : L"Pantalla completa", L"", L"Ventana", I_EXPORTAR, tm.textoSuave, L"F11",
        [](Estado* e) { ejecutar(e, A_PANTALLA_COMPLETA, 0); });
    add(L"Atajos de teclado", L"Todas las teclas rápidas del profesor", L"Ventana", I_BOMBILLA, tm.textoSuave, L"F1",
        [](Estado* e) { e->atajos = true; });
    if (listo) {
        add(L"Exportar un dossier de estudio", L"Resúmenes, glosario y fallos para imprimir", L"Ventana", I_EXPORTAR, tm.textoSuave, L"",
            [](Estado* e) { ejecutar(e, A_EXPORTAR, 0); });
        add(L"Exportar las tarjetas a Anki", L"Para repasar en el móvil", L"Ventana", I_EXPORTAR, tm.textoSuave, L"",
            [](Estado* e) { ejecutar(e, A_EXPORTAR, 1); });
        add(L"Abrir la carpeta del temario", L"", L"Ventana", I_CARPETA, tm.textoSuave, L"", [](Estado* e) { ejecutar(e, A_ABRIR_TEMARIO, 0); });
    }
    return v;
}

std::vector<Comando> filtrados(Estado* s) {
    std::vector<Comando> v = todos(s);
    std::vector<Comando> r;
    for (Comando& c : v) {
        c.puntos = puntuar(c, s->comandosTexto);
        if (c.puntos >= 0) r.push_back(c);
    }
    if (!palabras(s->comandosTexto).empty())
        std::stable_sort(r.begin(), r.end(), [](const Comando& a, const Comando& b) { return a.puntos > b.puntos; });
    return r;
}

void cerrarComandos(Estado* s) {
    s->comandos = false;
    s->comandosTexto.clear();
    InvalidateRect(s->ventana, NULL, FALSE);
}

void lanzar(Estado* s, int indice) {
    std::vector<Comando> v = filtrados(s);
    cerrarComandos(s);
    if (indice < 0 || indice >= (int)v.size()) return;
    v[(std::size_t)indice].hacer(s);
    InvalidateRect(s->ventana, NULL, FALSE);
}

void mover(Estado* s, int delta, int total) {
    if (total <= 0) return;
    s->comandosSel = std::max(0, std::min(total - 1, s->comandosSel + delta));
    if (s->comandosSel < s->comandosPrimero) s->comandosPrimero = s->comandosSel;
    if (s->comandosSel >= s->comandosPrimero + FILAS_VISIBLES) s->comandosPrimero = s->comandosSel - FILAS_VISIBLES + 1;
    InvalidateRect(s->ventana, NULL, FALSE);
}

// Corchea pequena (los iconos de la fuente no tienen una).
void notaMusical(HDC dc, int cx, int cy, int tam, COLORREF c) {
    float k = tam / 16.0f;
    circulo(dc, (int)(cx - 4 * k), (int)(cy + 4 * k), (int)std::max(2.0f, 3.0f * k), c);
    circulo(dc, (int)(cx + 4 * k), (int)(cy + 2.5f * k), (int)std::max(2.0f, 3.0f * k), c);
    float g = std::max(1.3f, 1.5f * k);
    linea(dc, cx - 1.4f * k, cy + 4 * k, cx - 1.4f * k, cy - 5.5f * k, c, g);
    linea(dc, cx + 6.6f * k, cy + 2.5f * k, cx + 6.6f * k, cy - 7 * k, c, g);
    linea(dc, cx - 1.4f * k, cy - 5.5f * k, cx + 6.6f * k, cy - 7 * k, c, g * 1.4f);
}

// Tecla dibujada como una pequena pastilla con borde («Ctrl», «K»).
int tecla(Estado* s, HDC dc, int x, int cy, const std::wstring& t, COLORREF fondo, COLORREF borde, COLORREF tinta) {
    int w = std::max(E(s, 22), anchoTexto(dc, s->f.mini, t) + E(s, 12));
    RECT r = rect(x, cy - E(s, 11), w, E(s, 22));
    redondeado(dc, r, E(s, 6), fondo, borde);
    texto(dc, s->f.mini, tinta, t, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return w;
}

// «Ctrl+Shift+K» -> varias teclas seguidas. Devuelve el ancho total.
int combinacion(Estado* s, HDC dc, int x, int cy, const std::wstring& atajo, COLORREF fondo, COLORREF borde, COLORREF tinta,
                bool medir = false) {
    int x0 = x;
    std::wstring parte;
    auto soltar = [&]() {
        if (parte.empty()) return;
        if (medir) x += std::max(E(s, 22), anchoTexto(dc, s->f.mini, parte) + E(s, 12)) + E(s, 4);
        else x += tecla(s, dc, x, cy, parte, fondo, borde, tinta) + E(s, 4);
        parte.clear();
    };
    for (std::size_t i = 0; i < atajo.size(); ++i) {
        if (atajo[i] == L'+' && !parte.empty()) soltar();
        else parte += atajo[i];
    }
    soltar();
    return x - x0 - E(s, 4);
}

}  // namespace

void abrirResumenDe(Estado* s, const std::string& ruta) { resumenDeTema(s, ruta); }

bool hayCapa(Estado* s) { return s->comandos || s->atajos || s->pantallaDescanso; }

void abrirComandos(Estado* s) {
    s->comandos = true;
    s->atajos = false;
    s->panelMusica = false;
    s->selector = false;
    s->comandosTexto.clear();
    s->comandosSel = 0;
    s->comandosPrimero = 0;
    s->comandosAbierta = GetTickCount64();
    SetFocus(s->ventana);  // las letras llegan a la ventana, no al campo de texto que tuviera el foco
    InvalidateRect(s->ventana, NULL, FALSE);
}

// ------------------------------------------------------------------ dibujo

int pintarCajaComandos(Estado* s, HDC dc, int izquierda, int derecha, int alto) {
    int disponible = derecha - izquierda;
    if (disponible < E(s, 150)) return 0;  // estrecha, el texto se corta con «…»
    int w = std::min(E(s, 320), disponible);
    const COLORREF fondo = gui::colorBarraSuperior();
    const COLORREF caja = mezcla(fondo, RGB(255, 255, 255), encima(s, A_COMANDOS) ? 0.13 : 0.07);
    const COLORREF suave = mezcla(gui::textoBarraSuperior(), fondo, 0.4);
    RECT r = rect(izquierda, (alto - E(s, 32)) / 2, w, E(s, 32));  // siempre en el mismo sitio
    redondeado(dc, r, E(s, 10), caja, mezcla(caja, RGB(255, 255, 255), 0.08));
    icono(s, dc, I_BUSCAR, r.left + E(s, 18), (r.top + r.bottom) / 2, E(s, 14), suave);
    int kw = combinacion(s, dc, 0, 0, L"Ctrl+K", 0, 0, 0, true);
    texto(dc, s->f.peque, suave, L"Buscar o ir a…", rect(r.left + E(s, 34), r.top, w - E(s, 44) - kw, r.bottom - r.top),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    combinacion(s, dc, r.right - E(s, 8) - kw, (r.top + r.bottom) / 2, L"Ctrl+K", mezcla(caja, RGB(255, 255, 255), 0.06),
                mezcla(caja, RGB(255, 255, 255), 0.16), suave);
    zona(s, r, A_COMANDOS);
    return w;
}

void pintarComandos(Estado* s, HDC dc, RECT cliente) {
    if (!s->comandos) return;
    const gui::Tema& tm = gui::tema();
    s->zonas.clear();  // lo de debajo no se puede pulsar
    velo(dc, cliente, RGB(0, 0, 0), tm.oscuro ? 120 : 70);
    zona(s, cliente, A_CERRAR_COMANDOS);
    std::vector<Comando> v = filtrados(s);
    int total = (int)v.size();
    if (s->comandosSel >= total) s->comandosSel = std::max(0, total - 1);
    s->comandosPrimero = std::max(0, std::min(s->comandosPrimero, std::max(0, total - FILAS_VISIBLES)));

    int w = std::min(E(s, 700), (int)cliente.right - E(s, 40));
    int fila = E(s, 54), cabeza = E(s, 64), pie = E(s, 40);
    int filas = std::max(1, std::min(FILAS_VISIBLES, total));
    int alto = cabeza + E(s, 8) + filas * fila + E(s, 8) + pie;
    int x = (cliente.right - w) / 2, y = std::max((int)E(s, 40), (int)(cliente.bottom * 0.13));
    RECT caja = rect(x, y, w, alto);

    redondeado(dc, caja, E(s, 18), tm.panel, mezcla(tm.borde, tm.acento, 0.35), 1);
    zona(s, caja, A_BLOQUEO);

    // Caja de texto
    int cy = y + cabeza / 2;
    icono(s, dc, I_BUSCAR, x + E(s, 30), cy, E(s, 20), tm.acento);
    RECT rt = rect(x + E(s, 56), y, w - E(s, 140), cabeza);
    if (s->comandosTexto.empty()) {
        texto(dc, s->f.subtitulo, mezcla(tm.textoSuave, tm.panel, 0.2), L"Escribe una pantalla, un tema, una acción…", rt,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        linea(dc, (float)rt.left, (float)(cy - E(s, 12)), (float)rt.left, (float)(cy + E(s, 12)), tm.acento, (float)E(s, 2));
    } else {
        int tw = anchoTexto(dc, s->f.subtitulo, s->comandosTexto);
        texto(dc, s->f.subtitulo, tm.texto, s->comandosTexto, rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        int cx = std::min((int)rt.right, (int)rt.left + tw + E(s, 2));
        if ((GetTickCount64() - s->comandosAbierta) / 530 % 2 == 0)
            linea(dc, (float)cx, (float)(cy - E(s, 12)), (float)cx, (float)(cy + E(s, 12)), tm.acento, (float)E(s, 2));
    }
    combinacion(s, dc, x + w - E(s, 24) - combinacion(s, dc, 0, 0, L"Esc", 0, 0, 0, true), cy, L"Esc", tm.panelAlt, tm.borde,
                tm.textoSuave);
    linea(dc, (float)x, (float)(y + cabeza) + 0.5f, (float)(x + w), (float)(y + cabeza) + 0.5f, tm.borde, 1.0f);

    // Resultados
    int fy = y + cabeza + E(s, 8);
    if (total == 0) {
        texto(dc, s->f.texto, tm.textoSuave, L"No encuentro nada con «" + s->comandosTexto + L"»", rect(x, fy, w, fila),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    std::wstring grupoAnterior;
    for (int i = s->comandosPrimero; i < total && i < s->comandosPrimero + FILAS_VISIBLES; ++i) {
        const Comando& c = v[(std::size_t)i];
        RECT r = rect(x + E(s, 8), fy, w - E(s, 16), fila - E(s, 4));
        bool el = i == s->comandosSel, h = encima(s, A_COMANDO, i);
        if (el) {
            redondeado(dc, r, E(s, 12), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.20 : 0.12));
            redondeado(dc, rect(r.left, r.top + E(s, 14), E(s, 3), r.bottom - r.top - E(s, 28)), E(s, 2), tm.acento);
        } else if (h) {
            redondeado(dc, r, E(s, 12), tm.panelAlt);
        }
        int ix = r.left + E(s, 30), iy = (r.top + r.bottom) / 2;
        redondeado(dc, rect(ix - E(s, 17), iy - E(s, 17), E(s, 34), E(s, 34)), E(s, 10), mezcla(tm.panel, c.color, tm.oscuro ? 0.22 : 0.16));
        if ((int)c.icono < 0) notaMusical(dc, ix, iy, E(s, 15), c.color);
        else icono(s, dc, c.icono, ix, iy, E(s, 17), c.color);
        int tx = ix + E(s, 30);
        int derechaTexto = r.right - E(s, 14);
        // A la derecha: el atajo, o el grupo
        if (!c.atajo.empty()) {
            int kw = combinacion(s, dc, 0, 0, c.atajo, 0, 0, 0, true);
            combinacion(s, dc, derechaTexto - kw, iy, c.atajo, tm.panelAlt, tm.borde, tm.textoSuave);
            derechaTexto -= kw + E(s, 12);
        } else {
            int gw = anchoTexto(dc, s->f.mini, c.grupo);
            texto(dc, s->f.mini, mezcla(tm.textoSuave, tm.panel, 0.25), c.grupo, rect(derechaTexto - gw, r.top, gw, r.bottom - r.top),
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            derechaTexto -= gw + E(s, 12);
        }
        if (c.detalle.empty()) {
            texto(dc, el ? s->f.negrita : s->f.texto, tm.texto, c.titulo, rect(tx, r.top, derechaTexto - tx, r.bottom - r.top),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            texto(dc, el ? s->f.negrita : s->f.texto, tm.texto, c.titulo, rect(tx, r.top + E(s, 6), derechaTexto - tx, E(s, 22)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            texto(dc, s->f.mini, tm.textoSuave, c.detalle, rect(tx, r.top + E(s, 27), derechaTexto - tx, E(s, 18)),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        zona(s, r, A_COMANDO, i);
        fy += fila;
    }
    // Barrita de desplazamiento si hay mas de las que caben
    if (total > FILAS_VISIBLES) {
        int pista = filas * fila, largo = std::max(E(s, 24), pista * FILAS_VISIBLES / total);
        int pos = (pista - largo) * s->comandosPrimero / std::max(1, total - FILAS_VISIBLES);
        redondeado(dc, rect(x + w - E(s, 6), y + cabeza + E(s, 8) + pos, E(s, 3), largo), E(s, 2), mezcla(tm.panel, tm.textoSuave, 0.5));
    }

    // Pie con las teclas
    int py = y + alto - pie;
    linea(dc, (float)x, (float)py + 0.5f, (float)(x + w), (float)py + 0.5f, tm.borde, 1.0f);
    int px = x + E(s, 20), pcy = py + pie / 2;
    auto ayuda = [&](const std::wstring& teclas, const std::wstring& que) {
        px += combinacion(s, dc, px, pcy, teclas, tm.panelAlt, tm.borde, tm.textoSuave) + E(s, 8);
        int qw = anchoTexto(dc, s->f.mini, que);
        texto(dc, s->f.mini, tm.textoSuave, que, rect(px, py, qw + E(s, 4), pie), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        px += qw + E(s, 22);
    };
    ayuda(L"↑+↓", L"moverse");
    ayuda(L"Intro", L"abrir");
    ayuda(L"Esc", L"cerrar");
    std::wstring cuenta = s->comandosTexto.empty() ? L"Escribe para ver también tus temas"
                                                   : std::to_wstring(total) + (total == 1 ? L" resultado" : L" resultados");
    texto(dc, s->f.mini, mezcla(tm.textoSuave, tm.panel, 0.2), cuenta, rect(px, py, x + w - E(s, 20) - px, pie),
          DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void pintarAtajos(Estado* s, HDC dc, RECT cliente) {
    if (!s->atajos) return;
    const gui::Tema& tm = gui::tema();
    s->zonas.clear();
    velo(dc, cliente, RGB(0, 0, 0), tm.oscuro ? 120 : 70);
    zona(s, cliente, A_CERRAR_ATAJOS);
    struct Atajo {
        const wchar_t* teclas;
        const wchar_t* que;
    };
    struct Grupo {
        const wchar_t* titulo;
        std::vector<Atajo> atajos;
    };
    std::vector<Grupo> grupos = {
        {L"En cualquier pantalla",
         {{L"Ctrl+K", L"Paleta de comandos: ir a cualquier sitio"},
          {L"Ctrl+F", L"Buscar en tus apuntes"},
          {L"F1", L"Esta ayuda"},
          {L"F9", L"Modo enfoque (sin distracciones)"},
          {L"Ctrl+T", L"Hacer un test (también en pantalla completa)"},
          {L"F11", L"Pantalla completa"},
          {L"Ctrl+M", L"Música de fondo sí / no"},
          {L"Alt+←", L"Volver a la pantalla anterior"},
          {L"Alt+1…9", L"Ir a cada sección del menú (Alt+0: Mis fallos)"},
          {L"Esc", L"Cerrar lo que esté abierto (no la pantalla completa ni el enfoque)"}}},
        {L"Durante un test",
         {{L"1+2+3+4", L"Elegir una respuesta (o A, B, C, D)"},
          {L"V+F", L"Verdadero o falso"},
          {L"Intro", L"Comprobar y pasar a la siguiente"},
          {L"S", L"No estoy seguro (si aciertas, vuelve pronto a repaso)"},
          {L"←+→", L"Pregunta anterior o siguiente"}}},
        {L"Tarjetas",
         {{L"Espacio", L"Girar la tarjeta"},
          {L"1", L"Otra vez (sale hoy de nuevo)"},
          {L"2", L"Bien"},
          {L"3", L"Fácil"}}},
        {L"Leyendo el temario",
         {{L"Espacio", L"Avanzar una pantalla (Mayús: retroceder)"},
          {L"←+→", L"Documento anterior o siguiente"},
          {L"−", L"Letra más pequeña (la tecla de sumar, más grande)"},
          {L"Inicio+Fin", L"Ir al principio o al final"},
          {L"AvPág+RePág", L"Página siguiente o anterior"},
          {L"0", L"Volver al tamaño de inicio (o pulsa el tamaño en la barra)"},
          {L"L", L"Mostrar u ocultar la lista de documentos"},
          {L"T", L"Ver el PDF tal cual o solo el texto"},
          {L"Ctrl+C", L"Copiar lo seleccionado"},
          {L"Ctrl+Rueda", L"Ampliar o reducir las páginas (Mayús+rueda: de lado)"},
          {L"Arrastrar", L"Seleccionar un trozo y preguntar al profesor por él"}}},
        {L"Escribir",
         {{L"Intro", L"Enviar una duda (Mayús+Intro: salto de línea)"},
          {L"Ctrl+Intro", L"Entregar una respuesta de desarrollo"},
          {L"Ctrl+A", L"Seleccionar todo el texto"}}},
    };
    // Sin IA no se ofrece preguntar al profesor: la ayuda tampoco lo menciona
    if (iaDisponible(s)) {
        grupos[3].atajos.insert(grupos[3].atajos.end() - 1, {L"P", L"Preguntar al profesor"});
    } else {
        grupos[3].atajos.back().que = L"Seleccionar un trozo: test, buscarlo o copiarlo";
        grupos[4].atajos.erase(grupos[4].atajos.begin());  // «Enviar una duda»
    }
    int w = std::min(E(s, 900), (int)cliente.right - E(s, 40));
    int columnas = w >= E(s, 760) ? 2 : 1;
    int colW = (w - E(s, 48) - (columnas - 1) * E(s, 28)) / columnas;
    int filaAlto = E(s, 34);
    // Alto de cada columna (los grupos se reparten en orden)
    std::vector<int> altoGrupo;
    for (const Grupo& g : grupos) altoGrupo.push_back(E(s, 34) + (int)g.atajos.size() * filaAlto + E(s, 14));
    std::vector<int> columnaDe(grupos.size(), 0);
    int altoCol[2] = {0, 0};
    for (std::size_t i = 0; i < grupos.size(); ++i) {
        int c = columnas == 2 && altoCol[1] < altoCol[0] ? 1 : 0;
        columnaDe[i] = c;
        altoCol[c] += altoGrupo[i];
    }
    int alto = E(s, 80) + std::max(altoCol[0], altoCol[1]) + E(s, 20);
    int x = (cliente.right - w) / 2, y = std::max((int)E(s, 30), (int)(cliente.bottom - alto) / 2);
    RECT caja = rect(x, y, w, alto);
    redondeado(dc, caja, E(s, 18), tm.panel, mezcla(tm.borde, tm.acento, 0.35));
    zona(s, caja, A_BLOQUEO);
    texto(dc, s->f.titulo, tm.texto, L"Atajos de teclado", rect(x + E(s, 24), y + E(s, 18), w - E(s, 120), E(s, 40)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    combinacion(s, dc, x + w - E(s, 24) - combinacion(s, dc, 0, 0, L"Esc", 0, 0, 0, true), y + E(s, 38), L"Esc", tm.panelAlt, tm.borde,
                tm.textoSuave);
    int yCol[2] = {y + E(s, 74), y + E(s, 74)};
    for (std::size_t i = 0; i < grupos.size(); ++i) {
        int c = columnaDe[i];
        int gx = x + E(s, 24) + c * (colW + E(s, 28)), gy = yCol[c];
        texto(dc, s->f.mini, tm.acento, grupos[i].titulo, rect(gx, gy, colW, E(s, 24)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        gy += E(s, 30);
        for (const Atajo& a : grupos[i].atajos) {
            int kw = combinacion(s, dc, gx, gy + filaAlto / 2, a.teclas, tm.panelAlt, tm.borde, tm.texto);
            int tx = gx + std::max(kw, E(s, 96)) + E(s, 14);
            texto(dc, s->f.peque, tm.textoSuave, a.que, rect(tx, gy, colW - (tx - gx), filaAlto), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            gy += filaAlto;
        }
        yCol[c] += altoGrupo[i];
    }
}

// ------------------------------------------------------------------ teclado y acciones

bool comandosTecla(Estado* s, WPARAM t) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (s->atajos) {
        if (t == VK_ESCAPE || t == VK_F1 || t == VK_RETURN) {
            s->atajos = false;
            InvalidateRect(s->ventana, NULL, FALSE);
        }
        return true;
    }
    if (s->comandos) {
        int total = (int)filtrados(s).size();
        if (t == VK_ESCAPE || (ctrl && (t == 'K' || t == 'P'))) cerrarComandos(s);
        else if (t == VK_DOWN) mover(s, 1, total);
        else if (t == VK_UP) mover(s, -1, total);
        else if (t == VK_NEXT) mover(s, FILAS_VISIBLES - 1, total);
        else if (t == VK_PRIOR) mover(s, -(FILAS_VISIBLES - 1), total);
        else if (t == VK_HOME && ctrl) mover(s, -total, total);
        else if (t == VK_END && ctrl) mover(s, total, total);
        else if (t == VK_RETURN) lanzar(s, s->comandosSel);
        else if (t == VK_BACK && !s->comandosTexto.empty()) {
            if (ctrl) {  // Ctrl+Retroceso: borra la ultima palabra
                while (!s->comandosTexto.empty() && iswspace(s->comandosTexto.back())) s->comandosTexto.pop_back();
                while (!s->comandosTexto.empty() && !iswspace(s->comandosTexto.back())) s->comandosTexto.pop_back();
            } else {
                s->comandosTexto.pop_back();
            }
            s->comandosSel = s->comandosPrimero = 0;
            s->comandosAbierta = GetTickCount64();
            InvalidateRect(s->ventana, NULL, FALSE);
        }
        return true;  // las letras llegan por WM_CHAR; ninguna tecla pasa a la pantalla de debajo
    }
    if (ctrl && (t == 'K' || t == 'P')) {
        abrirComandos(s);
        return true;
    }
    if (t == VK_F1) {
        s->atajos = true;
        s->panelMusica = false;
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    return false;
}

bool comandosCaracter(Estado* s, wchar_t c) {
    if (!s->comandos) return false;
    if (c >= 32 && c != 127 && s->comandosTexto.size() < 60) {
        s->comandosTexto += c;
        s->comandosSel = s->comandosPrimero = 0;
        s->comandosAbierta = GetTickCount64();
        InvalidateRect(s->ventana, NULL, FALSE);
    }
    return true;
}

bool comandosAccion(Estado* s, int accion, int dato) {
    switch (accion) {
        case A_COMANDOS: abrirComandos(s); return true;
        case A_CERRAR_COMANDOS: cerrarComandos(s); return true;
        case A_COMANDO: lanzar(s, dato); return true;
        case A_ATAJOS: s->atajos = true; return true;
        case A_CERRAR_ATAJOS: s->atajos = false; return true;
        case A_BLOQUEO: return true;  // clic dentro de una caja flotante: no hace nada
        case A_TEMAS_IA: {
            js::Valor p = js::Valor::objeto();
            p["ia"] = true;
            enviar(s, "temas", p);
            s->temasIa = "trabajando";
            s->mensaje = L"Le pido a la IA que vuelva a ordenar tus apuntes por temas. Tarda un minuto; puedes seguir estudiando.";
            return true;
        }
        default: return false;
    }
}

}  // namespace ui
}  // namespace profesor
