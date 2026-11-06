// GuiProfesorTemario.cpp - Leer el temario dentro del profesor.
//
// A la izquierda, los documentos por temas (con una marca en los que ya has leído y por dónde vas
// en los empezados): la lista va fija y se desplaza por su cuenta (rueda encima o su barra), así que
// se puede bajar por el documento o por la lista sin perder la otra de vista. A la derecha, el
// documento elegido página a página, con letra cómoda para
// leer. Los PDF se ven tal cual, como en un visor: cada página en blanco con sus gráficas y figuras
// (el motor pinta cada página como imagen bajo demanda y aquí se escala una vez a su ancho). Con
// «Solo el texto» se lee el texto extraído, con letra cómoda de tamaño ajustable (es la única vista
// de Word, PowerPoint y texto plano). Al llegar al final queda marcado como leído (o a mano).
// Recuerda por dónde ibas en cada documento y vuelve ahí al abrirlo. «Pantalla completa» lo abre en
// el modo enfoque: sin barra ni lateral y con la columna de lectura centrada.
//
// Mientras se lee:
//  - Abajo flota una barra con la página (‹ › saltan a la anterior o la siguiente), el tamaño (las
//    páginas se pueden ampliar hasta el 300 %, más anchas que la columna: Mayús + rueda las mueve de
//    lado; pulsar el porcentaje lo deja como al principio), el menú «Test» (de lo que estás
//    leyendo, rápido del tema, simulacro, repaso de fallos, desarrollo), el resumen del tema y
//    «Explícame esta página».
//  - Arrastrando el ratón se selecciona texto (palabra a palabra en la vista de texto; un recuadro en
//    la del PDF, cuyo texto saca el motor). La selección puede abarcar varias páginas: cerca del borde
//    de arriba o de abajo el texto se desplaza solo (y la rueda también vale mientras se arrastra).
//    Sobre la selección sale una barra para preguntar al profesor, hacer un test solo de ese trozo,
//    buscarlo en el temario (si es corto) o copiarlo.
//  - «Preguntar al profesor» abre un panel a la derecha, sin dejar de ver el apunte, con preguntas ya
//    preparadas sobre el trozo (explícamelo, un ejemplo, cómo cae en el examen…) y su propio campo
//    para escribir otra.
//  - Todo lo que es preguntar al profesor lo responde la IA: sin ella no se ofrece (una respuesta sin
//    IA solo podía devolver trozos del temario sin explicar nada).
#include "ProfesorUi.hpp"

#include <algorithm>
#include <cmath>
#include <cwctype>

namespace profesor {
namespace ui {

namespace {

const COLORREF AZUL = RGB(96, 165, 250);
const COLORREF VERDE = RGB(52, 211, 153);
const COLORREF VIOLETA = RGB(167, 139, 250);
const COLORREF TURQUESA = RGB(45, 212, 191);
const COLORREF ROSA = RGB(244, 114, 182);
const int TAM_MIN = 14, TAM_MAX = 34, TAM_INICIAL = 17;  // letra de lectura (px a 96 ppp)
// Tamaños de las páginas del PDF (% del ancho de la columna). Por encima de 100 se salen de ella.
const int ZOOMS[] = {50, 60, 70, 80, 90, 100, 115, 130, 150, 175, 200, 250, 300};
const int N_ZOOMS = (int)(sizeof(ZOOMS) / sizeof(ZOOMS[0]));
const int ZOOM_MIN = 50, ZOOM_MAX = 300;

// Se puede leer aqui: tiene texto o es un PDF que se ve tal cual.
bool legible(const js::Valor& d) { return d["legibles"].comoEntero() > 0 || d["visual"].comoBool(); }

// El documento abierto se esta viendo como el PDF (paginas pintadas) y no como texto.
bool vistaPdf(Estado* s) {
    const js::Valor& l = s->lectura;
    if (!l["visual"].comoBool() || l["hojas"].tam() == 0) return false;
    return s->lecturaPdf || l["paginas"].tam() == 0;  // sin texto extraido no hay otra vista
}

int indiceDoc(Estado* s, const std::string& ruta) {
    const js::Valor& t = s->temario["temas"];
    for (std::size_t i = 0; i < t.tam(); ++i)
        if (t[i]["ruta"].comoCadena() == ruta) return (int)i;
    return -1;
}

// Documento legible siguiente (+1) o anterior (-1) en el orden de la lista (por temas).
int vecino(Estado* s, int paso) {
    const js::Valor& t = s->temario["temas"];
    std::vector<FilaTema> filas = filasPorTema(t);
    std::vector<int> orden;
    for (const FilaTema& f : filas)
        if (f.indice >= 0 && legible(t[(std::size_t)f.indice])) orden.push_back(f.indice);
    int actual = indiceDoc(s, s->lecturaRuta);
    for (std::size_t k = 0; k < orden.size(); ++k)
        if (orden[k] == actual) {
            int j = (int)k + paso;
            return j >= 0 && j < (int)orden.size() ? orden[(std::size_t)j] : -1;
        }
    return -1;
}

void quitarSeleccion(Estado* s) {
    if (s->lecturaArrastre) {
        KillTimer(s->ventana, TIMER_ARRASTRE);
        ReleaseCapture();
    }
    s->lecturaDesplazado = 0;
    s->selDesdeP = s->selHastaP = -1;
    s->selHoja = s->selHojaFin = -1;
    s->lecturaSel.clear();
    s->lecturaSelDonde.clear();
    s->lecturaSelPidiendo = false;
    s->lecturaArrastre = 0;
}

void pedirDoc(Estado* s, const std::string& ruta) {
    if (ruta.empty()) return;
    guardarLectura(s);  // por donde iba el que se deja
    s->lecturaRuta = ruta;
    quitarSeleccion(s);
    js::Valor p = js::Valor::objeto();
    p["ruta"] = ruta;
    enviar(s, "leer", p, s->lectura.esNulo(), L"Abriendo el documento…");
}

// Parrafos del texto: una marca de pagina («\x01página 3») delante del texto de cada una. Los
// espacios seguidos se juntan en uno (se maqueta y se selecciona palabra a palabra).
std::vector<std::wstring> parrafos(const js::Valor& lectura) {
    std::vector<std::wstring> v;
    const js::Valor& pags = lectura["paginas"];
    for (std::size_t i = 0; i < pags.tam(); ++i) {
        v.push_back(L"\x01" + W(pags[i]["ubicacion"]));  // marca de pagina, antes de su texto
        std::wstring t = W(pags[i]["texto"]);
        std::size_t inicio = 0;
        while (inicio <= t.size()) {
            std::size_t fin = t.find(L'\n', inicio);
            if (fin == std::wstring::npos) fin = t.size();
            std::wstring p;
            for (std::size_t k = inicio; k < fin; ++k) {
                wchar_t c = t[k] == L'\t' || t[k] == L'\r' ? L' ' : t[k];
                if (c == L' ' && (p.empty() || p.back() == L' ')) continue;
                p += c;
            }
            while (!p.empty() && p.back() == L' ') p.pop_back();
            if (!p.empty()) v.push_back(p);
            inicio = fin + 1;
        }
    }
    return v;
}

// Viñetas y titulos cortos un poco destacados.
bool esTitulo(const std::wstring& p) {
    return p.size() < 70 && p.back() != L'.' && p.back() != L':' && p.find(L". ") == std::wstring::npos;
}

// ---------------------------------------------------------------- maquetacion
// El texto se coloca palabra a palabra (y no con DrawText) para saber donde queda cada una y
// poder seleccionarlas con el raton.

struct Trozo {
    std::size_t ini, len;
};

std::vector<Trozo> trocear(const std::wstring& t) {
    std::vector<Trozo> v;
    std::size_t i = 0;
    while (i < t.size()) {
        std::size_t fin = t.find(L' ', i);
        if (fin == std::wstring::npos) fin = t.size();
        if (fin > i) v.push_back({i, fin - i});
        i = fin + 1;
    }
    return v;
}

struct Colocada {
    int x, y, w;
    std::size_t ini, len;
};

int interlineado(HDC dc, HFONT f) {
    SelectObject(dc, f);
    TEXTMETRICW m;
    GetTextMetricsW(dc, &m);
    return m.tmHeight * 128 / 100;  // algo de aire entre lineas: se lee mejor
}

// Coloca las palabras en lineas de `ancho` px. Devuelve el alto del parrafo.
int maquetar(HDC dc, HFONT f, const std::wstring& t, int ancho, std::vector<Colocada>* salida) {
    int lh = interlineado(dc, f);
    SIZE espacio = {0, 0};
    GetTextExtentPoint32W(dc, L" ", 1, &espacio);
    int x = 0, y = 0;
    for (const Trozo& tr : trocear(t)) {
        SIZE sz = {0, 0};
        GetTextExtentPoint32W(dc, t.c_str() + tr.ini, (int)tr.len, &sz);
        if (x > 0 && x + sz.cx > ancho) {
            x = 0;
            y += lh;
        }
        if (salida) salida->push_back({x, y, (int)sz.cx, tr.ini, tr.len});
        x += sz.cx + espacio.cx;
    }
    return y + lh;
}

// ------------------------------------------------------------------ seleccion

bool haySeleccionTexto(Estado* s) { return s->selDesdeP >= 0 && s->selHastaP >= 0; }

// Extremos de la seleccion de texto ordenados (de arriba abajo).
void extremos(Estado* s, int& p1, int& w1, int& p2, int& w2) {
    p1 = s->selDesdeP, w1 = s->selDesdeW, p2 = s->selHastaP, w2 = s->selHastaW;
    if (p2 < p1 || (p2 == p1 && w2 < w1)) {
        std::swap(p1, p2);
        std::swap(w1, w2);
    }
}

bool seleccionada(Estado* s, int p, int w) {
    if (!haySeleccionTexto(s)) return false;
    int p1, w1, p2, w2;
    extremos(s, p1, w1, p2, w2);
    if (p < p1 || p > p2) return false;
    if (p == p1 && w < w1) return false;
    if (p == p2 && w > w2) return false;
    return true;
}

std::wstring numeroDePagina(const std::wstring& ubicacion);

// Pagina («página 3») en la que esta el parrafo p ("" si no se sabe).
std::wstring paginaDelParrafo(const std::vector<std::wstring>& ps, int p) {
    for (int k = std::min(p, (int)ps.size() - 1); k >= 0; --k)
        if (ps[(std::size_t)k][0] == L'\x01') return ps[(std::size_t)k].substr(1);
    return L"";
}

// Texto de la seleccion (y en que pagina o paginas esta: «página 3», «páginas 3–5»).
std::wstring textoSeleccionado(Estado* s, std::wstring& donde) {
    std::vector<std::wstring> ps = parrafos(s->lectura);
    int p1, w1, p2, w2;
    extremos(s, p1, w1, p2, w2);
    donde = paginaDelParrafo(ps, p1);
    std::wstring hasta = paginaDelParrafo(ps, p2);
    if (!donde.empty() && !hasta.empty() && hasta != donde)
        donde = L"páginas " + numeroDePagina(donde) + L"–" + numeroDePagina(hasta);
    std::wstring t;
    for (int p = p1; p <= p2 && p < (int)ps.size(); ++p) {
        const std::wstring& par = ps[(std::size_t)p];
        if (par[0] == L'\x01') continue;
        std::vector<Trozo> tr = trocear(par);
        int desde = p == p1 ? w1 : 0, hasta = p == p2 ? w2 : (int)tr.size() - 1;
        std::wstring linea;
        for (int i = std::max(0, desde); i <= hasta && i < (int)tr.size(); ++i) {
            if (!linea.empty()) linea += L' ';
            linea += par.substr(tr[(std::size_t)i].ini, tr[(std::size_t)i].len);
        }
        if (linea.empty()) continue;
        if (!t.empty()) t += L'\n';
        t += linea;
    }
    return t;
}

// Palabra visible mas cercana a un punto (por lineas: primero la distancia vertical).
const Estado::PalabraVista* palabraCercana(Estado* s, POINT pt) {
    const Estado::PalabraVista* mejor = NULL;
    long mejorD = 0;
    for (const Estado::PalabraVista& p : s->lecturaPalabras) {
        long dy = pt.y < p.r.top ? p.r.top - pt.y : pt.y >= p.r.bottom ? pt.y - p.r.bottom + 1 : 0;
        long dx = pt.x < p.r.left ? p.r.left - pt.x : pt.x >= p.r.right ? pt.x - p.r.right + 1 : 0;
        long d = dy * 10000 + dx;
        if (!mejor || d < mejorD) {
            mejor = &p;
            mejorD = d;
        }
    }
    return mejor;
}

// Parte del recuadro del PDF que cae en la pagina i (izquierda, arriba, derecha, abajo; fracciones).
// El recuadro puede abarcar varias paginas: en la primera va desde donde se empezo hasta abajo, las
// de en medio entran enteras y en la ultima va desde arriba hasta donde se solto, siempre entre las
// mismas dos x.
bool cajaEnHoja(Estado* s, int i, double c[4]) {
    if (s->selHoja < 0) return false;
    int fin = s->selHojaFin < 0 ? s->selHoja : s->selHojaFin;
    bool alReves = fin < s->selHoja;  // se arrastro hacia arriba
    int a = alReves ? fin : s->selHoja, b = alReves ? s->selHoja : fin;
    if (i < a || i > b) return false;
    double arriba = alReves ? s->selCaja[3] : s->selCaja[1], abajo = alReves ? s->selCaja[1] : s->selCaja[3];
    c[0] = std::min(s->selCaja[0], s->selCaja[2]);
    c[2] = std::max(s->selCaja[0], s->selCaja[2]);
    if (a == b) {
        c[1] = std::min(arriba, abajo);
        c[3] = std::max(arriba, abajo);
    } else {
        c[1] = i == a ? arriba : 0.0;
        c[3] = i == b ? abajo : 1.0;
    }
    return true;
}

// Primera y ultima pagina del recuadro del PDF.
void hojasDeLaCaja(Estado* s, int& a, int& b) {
    int fin = s->selHojaFin < 0 ? s->selHoja : s->selHojaFin;
    a = std::min(s->selHoja, fin);
    b = std::max(s->selHoja, fin);
}

// Lleva el final de la seleccion al punto pt, con lo que se ve ahora en pantalla. true si ha cambiado.
bool extenderSeleccion(Estado* s, POINT pt) {
    if (s->lecturaArrastre == 1) {
        const Estado::PalabraVista* p = palabraCercana(s, pt);
        if (!p || (p->parrafo == s->selHastaP && p->palabra == s->selHastaW)) return false;
        s->selHastaP = p->parrafo;
        s->selHastaW = p->palabra;
        return true;
    }
    if (s->lecturaArrastre != 2) return false;
    // La pagina bajo el raton o, entre dos paginas o fuera de la ventana, la visible mas cercana
    const std::pair<int, RECT>* hoja = NULL;
    long mejor = 0;
    for (const auto& v : s->lecturaVisibles) {
        long d = pt.y < v.second.top ? v.second.top - pt.y : pt.y > v.second.bottom ? pt.y - v.second.bottom : 0;
        if (!hoja || d < mejor) {
            hoja = &v;
            mejor = d;
        }
    }
    if (!hoja) return false;
    const RECT& r = hoja->second;
    double fx = std::max(0.0, std::min(1.0, (double)(pt.x - r.left) / std::max(1L, r.right - r.left)));
    double fy = std::max(0.0, std::min(1.0, (double)(pt.y - r.top) / std::max(1L, r.bottom - r.top)));
    if (hoja->first == s->selHojaFin && fx == s->selCaja[2] && fy == s->selCaja[3]) return false;
    s->selHojaFin = hoja->first;
    s->selCaja[2] = fx;
    s->selCaja[3] = fy;
    return true;
}

// Seleccionando cerca del borde de arriba o de abajo (o fuera de la ventana): cuanto desplazar el
// texto en cada tic, mas cuanto mas lejos se lleva el raton. 0 = nada.
int velocidadArrastre(Estado* s) {
    int borde = E(s, 48), y = s->lecturaRaton.y;
    int fuera = y < s->cuerpo.top + borde ? y - (s->cuerpo.top + borde) : y > s->cuerpo.bottom - borde ? y - (s->cuerpo.bottom - borde) : 0;
    if (!fuera) return 0;
    int paso = std::min(E(s, 70), E(s, 4) + std::abs(fuera) / 2);
    return fuera < 0 ? -paso : paso;
}

// ---------------------------------------------------------------- utilidades

void marcar(Estado* s, bool leido) {
    js::Valor p = js::Valor::objeto();
    p["ruta"] = s->lecturaRuta;
    p["leido"] = leido;
    enviar(s, "marcar_leido", p);
}

// Cambia la letra de lectura (sin avisar al motor) y tira la cache de altos.
void ponerTam(Estado* s, int tam) {
    s->lecturaTam = std::max(TAM_MIN, std::min(TAM_MAX, tam));
    if (s->f.lectura) {
        LOGFONTW lf;
        if (GetObjectW(s->f.lectura, sizeof(lf), &lf)) {
            lf.lfHeight = -E(s, (int)std::lround(s->lecturaTam * gui::estilo().texto / 100.0));
            HFONT nueva = CreateFontIndirectW(&lf);
            if (nueva) {
                DeleteObject(s->f.lectura);
                s->f.lectura = nueva;
            }
        }
    }
    s->lecturaAltos.clear();
}

void ponerZoom(Estado* s, int z) {
    z = std::max(ZOOM_MIN, std::min(ZOOM_MAX, z));
    if (z == s->lecturaZoom) return;
    s->lecturaIrA = s->lecturaPos;
    s->lecturaZoom = z;
    if (z <= 100) s->lecturaDx = 0;
    aviso(s, z == 100 ? std::wstring(L"Páginas a su tamaño normal (100 %)")
                      : L"Páginas al " + std::to_wstring(z) + L" %" + (z > 100 ? L"  ·  Mayús + rueda para moverte de lado" : L"") +
                            L"  ·  pulsa el porcentaje o 0 para volver al 100 %");
}

// Hueco que se deja encima de una pagina al saltar a ella (debajo de la franja fija del titulo).
int margenPagina(Estado* s) { return E(s, 52); }

// Donde empieza la pagina anterior (paso -1) o la siguiente (+1) a la que se lee, en «y» del
// contenido; -1 si no hay. Sirve igual para las hojas del PDF y para las paginas del texto.
int destinoPagina(Estado* s, int paso) {
    int linea = s->cuerpo.top + margenPagina(s);  // la pagina que empieza aqui es la que se lee
    const std::vector<int>& v = s->lecturaInicios;
    if (paso > 0) {
        if (s->scroll >= s->altoContenido - s->altoVisible) return -1;  // ya abajo del todo: no se puede bajar mas
        for (int y : v)
            if (y - s->scroll > linea + E(s, 8)) return y;
        return -1;
    }
    for (std::size_t k = v.size(); k-- > 0;)
        if (v[k] - s->scroll < linea - E(s, 8)) return v[k];
    return -1;
}

void irAPagina(Estado* s, int paso) {
    int y = destinoPagina(s, paso);
    if (y < 0) {
        aviso(s, paso > 0 ? L"Ya estás en la última página" : L"Ya estás en la primera página");
        return;
    }
    desplazar(s, y - s->scroll - (s->cuerpo.top + margenPagina(s)));
}

void ponerLetra(Estado* s, int nuevo) {
    nuevo = std::max(TAM_MIN, std::min(TAM_MAX, nuevo));
    if (nuevo == s->lecturaTam) return;
    s->lecturaIrA = s->lecturaPos;  // seguir en el mismo sitio del texto
    ponerTam(s, nuevo);
    js::Valor p = js::Valor::objeto();
    p["tam"] = nuevo;
    enviar(s, "tam_lectura", p);
    aviso(s, L"Letra de lectura: " + std::to_wstring(nuevo) + L" px");
}

void cambiarLetra(Estado* s, int paso) {
    if (vistaPdf(s)) {
        // En la vista del PDF, «A−»/«A+» cambian el tamaño de las páginas (por pasos de la lista)
        int i = 0;
        while (i < N_ZOOMS - 1 && ZOOMS[i] < s->lecturaZoom) ++i;
        if (paso > 0 && ZOOMS[i] <= s->lecturaZoom) ++i;
        if (paso < 0) --i;
        ponerZoom(s, ZOOMS[std::max(0, std::min(N_ZOOMS - 1, i))]);
        return;
    }
    ponerLetra(s, s->lecturaTam + (s->lecturaTam >= 24 ? 2 : 1) * paso);
}

// Rehace la lista de documentos cambiando un campo de uno (los arrays no se modifican en su sitio).
void cambiarDoc(Estado* s, const std::string& ruta, const char* campo, const js::Valor& valor) {
    const js::Valor& viejos = s->temario["temas"];
    js::Valor nuevos = js::Valor::array();
    for (std::size_t i = 0; i < viejos.tam(); ++i) {
        js::Valor d = viejos[i];
        if (d["ruta"].comoCadena() == ruta) d[campo] = valor;
        nuevos.agregar(d);
    }
    s->temario["temas"] = nuevos;
}

int porcentaje(double f) { return (int)std::lround(std::max(0.0, std::min(1.0, f)) * 100); }

// Posicion de la pagina que se lee entre las que tienen texto (-1 si no tiene).
int paginaConTexto(Estado* s) {
    const js::Valor& pags = s->lectura["paginas"];
    for (std::size_t i = 0; i < pags.tam(); ++i)
        if (W(pags[i]["ubicacion"]) == s->lecturaPagina) return (int)i;
    return -1;
}

std::wstring numeroDePagina(const std::wstring& ubicacion) {
    std::size_t i = ubicacion.find_last_of(L' ');
    return i == std::wstring::npos ? ubicacion : ubicacion.substr(i + 1);
}

// ------------------------------------------------- preguntas y tests desde el lector

void abrirPanel(Estado* s, const std::wstring& cita, const std::string& contexto, const std::wstring& etiqueta) {
    s->contextoIndice = -1;
    s->contextoPregunta = contexto;
    s->contextoEtiqueta = etiqueta;
    s->lecturaPanelCita = cita;
    if (!s->lecturaPanel) s->lecturaChatDesde = s->chat.size();
    s->lecturaPanel = true;
    s->lecturaMenuTest = false;
    s->lecturaIrA = s->lecturaPos;  // la columna se estrecha: seguir en el mismo sitio
    colocarEdit(s);
}

void preguntar(Estado* s, const std::wstring& duda) {
    if (s->chatEnCurso) {
        aviso(s, L"Espera a que termine la respuesta anterior");
        return;
    }
    enviarDuda(s, duda);
}

std::string contextoDeSeleccion(Estado* s) {
    return gui::aUtf8(L"Fragmento de los apuntes («" + W(s->lectura["tema"]) + L"»" +
                      (s->lecturaSelDonde.empty() ? L"" : L", " + s->lecturaSelDonde) + L"):\n" + s->lecturaSel);
}

void empezarTestDeLectura(Estado* s, js::Valor c, const std::wstring& espera) {
    js::Valor t = js::Valor::array();
    t.agregar(s->lecturaRuta);
    c["temas"] = t;
    s->lecturaMenuTest = false;
    empezarCon(s, c, espera);
}

// ---------------------------------------------------------- paginas del PDF

const int ETIQUETA_HOJA = 40;  // hueco encima de cada pagina para «Página 3 de 20»

// Rectangulo de cada pagina (en coordenadas de pantalla), con el borde izquierdo en `x`.
std::vector<RECT> colocarHojas(Estado* s, int x, int tw, int arriba) {
    std::vector<RECT> v;
    const js::Valor& hojas = s->lectura["hojas"];
    int ancho = std::max(E(s, 200), tw * s->lecturaZoom / 100);
    int y = arriba;
    for (std::size_t i = 0; i < hojas.tam(); ++i) {
        double pw = hojas[i][0].comoNumero(), ph = hojas[i][1].comoNumero();
        int alto = pw > 0 ? (int)std::lround(ancho * ph / pw) : ancho * 4 / 3;
        y += E(s, ETIQUETA_HOJA);
        v.push_back(rect(x, y, ancho, alto));
        y += alto + E(s, 8);
    }
    return v;
}

// Pide al motor la pagina i pintada a (al menos) `ancho` px, si no la tiene ya o la tiene pequeña.
void pedirHoja(Estado* s, int i, int ancho) {
    int paso = 200, quiero = std::min(2400, (ancho + paso - 1) / paso * paso);
    if (s->lecturaFallida.count(i)) return;
    auto tiene = s->lecturaJpgAncho.find(i);
    if (tiene != s->lecturaJpgAncho.end() && tiene->second >= quiero) return;
    auto pedida = s->lecturaPedida.find(i);
    if (pedida != s->lecturaPedida.end() && pedida->second >= quiero) return;
    js::Valor p = js::Valor::objeto();
    p["ruta"] = s->lectura["ruta"].comoCadena();
    p["indice"] = i;
    p["ancho"] = quiero;
    int id = enviar(s, "pagina", p);
    if (id) {
        s->lecturaPedida[i] = quiero;
        s->lecturaPeticiones[id] = i;
    }
}

void soltarHoja(Estado* s, int i) {
    auto it = s->lecturaHojas.find(i);
    if (it == s->lecturaHojas.end()) return;
    delete it->second;
    s->lecturaHojas.erase(it);
}

// La pagina i ya escalada a r (se escala una sola vez: al desplazarse solo se copia).
Gdiplus::Bitmap* hojaEscalada(Estado* s, int i, const RECT& r) {
    int w = r.right - r.left, h = r.bottom - r.top;
    auto it = s->lecturaHojas.find(i);
    if (it != s->lecturaHojas.end()) {
        if (it->second && (int)it->second->GetWidth() == w && (int)it->second->GetHeight() == h) return it->second;
        soltarHoja(s, i);
    }
    auto jpg = s->lecturaJpg.find(i);
    if (jpg == s->lecturaJpg.end() || w <= 0 || h <= 0) return NULL;
    Gdiplus::Bitmap origen(jpg->second.c_str());
    if (origen.GetLastStatus() != Gdiplus::Ok || origen.GetWidth() == 0) return NULL;
    Gdiplus::Bitmap* escalada = new Gdiplus::Bitmap(w, h, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics g(escalada);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(&origen, 0, 0, w, h);
    }
    s->lecturaHojas[i] = escalada;
    return escalada;
}

// Pinta las paginas visibles como hojas blancas, pide las que faltan y suelta las lejanas.
void pintarHojas(Estado* s, HDC dc, const std::vector<RECT>& hojas, std::wstring& paginaActual) {
    const gui::Tema& tm = gui::tema();
    int arriba = s->cuerpo.top, abajo = s->cuerpo.bottom, pantalla = abajo - arriba;
    int n = (int)hojas.size();
    s->lecturaVisibles.clear();
    for (int i = 0; i < n; ++i) {
        const RECT& r = hojas[(std::size_t)i];
        int w = r.right - r.left;
        if (r.top - E(s, ETIQUETA_HOJA) <= arriba + pantalla / 3) paginaActual = L"página " + std::to_wstring(i + 1);
        bool visible = r.bottom >= arriba && r.top - E(s, ETIQUETA_HOJA) <= abajo;
        bool cerca = r.bottom >= arriba - pantalla && r.top <= abajo + pantalla;  // tambien la de antes y la de despues
        if (!cerca) {
            if (r.bottom < arriba - 3 * pantalla || r.top > abajo + 3 * pantalla) soltarHoja(s, i);
            continue;
        }
        pedirHoja(s, i, w);
        if (!visible) continue;
        s->lecturaVisibles.push_back({i, r});
        // «Página 3 de 20»
        wchar_t et[48];
        swprintf(et, 48, L"Página %d de %d", i + 1, n);
        int ex = std::max((int)r.left, (int)s->cuerpo.left), ew = std::min((int)r.right, (int)s->cuerpo.right) - ex;
        texto(dc, s->f.mini, tm.textoSuave, et, rect(ex, r.top - E(s, ETIQUETA_HOJA), ew, E(s, ETIQUETA_HOJA) - E(s, 6)),
              DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
        // Hoja blanca con una sombra suave, como en un visor de PDF
        RECT sombra = r;
        OffsetRect(&sombra, 0, E(s, 4));
        InflateRect(&sombra, E(s, 3), E(s, 3));
        redondeado(dc, sombra, E(s, 8), mezcla(tm.panel, RGB(0, 0, 0), tm.oscuro ? 0.45 : 0.08));
        redondeado(dc, r, E(s, 3), RGB(255, 255, 255), mezcla(RGB(255, 255, 255), RGB(0, 0, 0), 0.12));
        Gdiplus::Bitmap* bm = hojaEscalada(s, i, r);
        if (bm) {
            Gdiplus::Graphics g(dc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            g.DrawImage(bm, (int)r.left, (int)r.top, w, (int)(r.bottom - r.top));
        } else {
            bool fallo = s->lecturaFallida.count(i) > 0;
            int cy = std::max((int)r.top, arriba) + std::min((int)(r.bottom - r.top), pantalla) / 2;
            cy = std::min(cy, (int)r.bottom - E(s, 40));
            int cx = (std::max((int)r.left, (int)s->cuerpo.left) + std::min((int)r.right, (int)s->cuerpo.right)) / 2;
            if (!fallo) spinner(s, dc, cx, cy - E(s, 16), E(s, 14));
            texto(dc, s->f.peque, RGB(120, 120, 130),
                  fallo ? L"No he podido mostrar esta página: ábrela con «Abrir el original»" : L"Cargando la página…",
                  rect(cx - E(s, 260), cy + E(s, 8), E(s, 520), E(s, 22)), DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        // Lo que cae del recuadro seleccionado en esta pagina (la barra va junto al ultimo trozo visible)
        double caja[4];
        if (cajaEnHoja(s, i, caja)) {
            int h = r.bottom - r.top;
            RECT c = {r.left + (int)std::lround(caja[0] * w), r.top + (int)std::lround(caja[1] * h),
                      r.left + (int)std::lround(caja[2] * w), r.top + (int)std::lround(caja[3] * h)};
            velo(dc, c, tm.acento, 46);
            contorno(dc, c, E(s, 3), tm.acento, (float)E(s, 2));
            s->lecturaSelRect = c;
        }
    }
}

// --------------------------------------------------------------- piezas flotantes

// Boton en forma de pildora con icono opcional. Devuelve su ancho.
int pildora(Estado* s, HDC dc, int x, int y, int alto, int ic, const std::wstring& t, int accion, int dato, bool principal,
            COLORREF c, bool medir = false) {
    const gui::Tema& tm = gui::tema();
    int tw = t.empty() ? 0 : anchoTexto(dc, principal ? s->f.negrita : s->f.peque, t);
    int w = (ic >= 0 ? E(s, 30) : E(s, 14)) + tw + (t.empty() ? E(s, 4) : E(s, 14));
    if (medir) return w;
    RECT r = rect(x, y, w, alto);
    bool h = encima(s, accion, dato);
    COLORREF fondo = principal ? (h ? mezcla(c, RGB(255, 255, 255), 0.14) : c) : (h ? mezcla(tm.panelAlt, c, 0.18) : tm.panel);
    COLORREF borde = principal ? fondo : (h ? mezcla(tm.borde, c, 0.6) : tm.panel);
    redondeado(dc, r, alto / 2, fondo, borde);
    COLORREF tinta = principal ? RGB(255, 255, 255) : (h ? tm.texto : mezcla(tm.texto, tm.textoSuave, 0.25));
    if (ic >= 0) glifo(s, dc, (Icono)ic, x + E(s, 18), y + alto / 2, E(s, 15), principal ? tinta : c);
    if (!t.empty())
        texto(dc, principal ? s->f.negrita : s->f.peque, tinta, t, rect(x + (ic >= 0 ? E(s, 30) : E(s, 14)), y, tw + E(s, 4), alto),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    zona(s, r, accion, dato);
    return w;
}

// Separador fino entre grupos de la barra.
void separador(HDC dc, int x, int y, int alto) {
    redondeado(dc, rect(x, y + alto / 4, 1, alto / 2), 0, mezcla(gui::tema().panel, gui::tema().borde, 0.9));
}

// Lo que se puede hacer con un trozo seleccionado (dato de A_L_SEL).
enum { SEL_PREGUNTAR = 0, SEL_TEST = 3, SEL_COPIAR = 4, SEL_CERRAR = 5, SEL_BUSCAR = 6 };
const std::size_t SEL_MIN_TEST = 160;   // un par de frases: menos no da para sacar preguntas
const std::size_t SEL_MAX_BUSCAR = 80;  // un termino o una frase corta: buscar un parrafo entero no sirve

// Barra flotante sobre la seleccion. Con IA lo principal es preguntar al profesor (las preguntas
// concretas se eligen luego en el panel); sin IA, el test del trozo o buscarlo en el temario.
void pintarBarraSeleccion(Estado* s, HDC dc, int derecha, int suelo) {
    const gui::Tema& tm = gui::tema();
    RECT sel = s->lecturaSelRect;
    if (sel.right <= sel.left || sel.bottom < s->cuerpo.top || sel.top > suelo) return;
    int alto = E(s, 40), pad = E(s, 5);
    struct Pieza {
        int icono;
        std::wstring t;
        int dato;
        bool principal;
        COLORREF color;
    };
    std::vector<Pieza> piezas;
    bool ia = iaDisponible(s);
    bool test = s->lecturaSel.size() >= SEL_MIN_TEST, buscar = s->lecturaSel.size() <= SEL_MAX_BUSCAR;
    if (ia) piezas.push_back({I_DUDAS, L"Preguntar al profesor", SEL_PREGUNTAR, true, tm.acento});
    if (test) piezas.push_back({I_TEST, L"Test de esto", SEL_TEST, !ia, TURQUESA});
    if (buscar) piezas.push_back({I_BUSCAR, L"Buscar en el temario", SEL_BUSCAR, !ia && !test, AZUL});
    piezas.push_back({I_COPIAR, piezas.empty() ? L"Copiar" : L"", SEL_COPIAR, false, tm.textoSuave});
    piezas.push_back({-1, L"✕", SEL_CERRAR, false, tm.textoSuave});
    int w = 0;
    if (s->lecturaSelPidiendo) {
        w = anchoTexto(dc, s->f.peque, L"Leyendo el texto de esa zona…") + E(s, 60);
    } else {
        w = 2 * pad;
        for (const Pieza& p : piezas)
            w += pildora(s, dc, 0, 0, alto - 2 * pad, p.icono, p.t, A_L_SEL, p.dato, p.principal, p.color, true) + E(s, 2);
    }
    int izquierda = s->cuerpo.left + E(s, 10);
    int x = std::max(izquierda, std::min(derecha - E(s, 10) - w, (int)(sel.left + sel.right) / 2 - w / 2));
    int y = sel.bottom + E(s, 10);
    if (y + alto > suelo) y = sel.top - alto - E(s, 10);           // no cabe debajo: encima
    if (y < s->cuerpo.top + E(s, 4)) y = std::min(suelo - alto, (int)s->cuerpo.top + E(s, 48));  // ni encima: arriba del todo
    RECT caja = rect(x, y, w, alto);
    RECT sombra = caja;
    OffsetRect(&sombra, 0, E(s, 3));
    redondeado(dc, sombra, alto / 2, mezcla(tm.fondo, RGB(0, 0, 0), 0.35));
    redondeado(dc, caja, alto / 2, tm.panel, mezcla(tm.borde, tm.acento, 0.45));
    zona(s, caja, A_NADA);
    if (s->lecturaSelPidiendo) {
        spinner(s, dc, x + E(s, 24), y + alto / 2, E(s, 9));
        texto(dc, s->f.peque, tm.textoSuave, L"Leyendo el texto de esa zona…", rect(x + E(s, 44), y, w - E(s, 50), alto),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    int bx = x + pad, by = y + pad, bh = alto - 2 * pad;
    for (const Pieza& p : piezas) bx += pildora(s, dc, bx, by, bh, p.icono, p.t, A_L_SEL, p.dato, p.principal, p.color) + E(s, 2);
}

// Menu «Test» de la barra de abajo: todos los tests que se pueden hacer de lo que se lee.
void pintarMenuTest(Estado* s, HDC dc, int centro, int suelo, int derecha) {
    const gui::Tema& tm = gui::tema();
    struct Fila {
        Icono icono;
        std::wstring titulo, detalle;
        int accion;
        COLORREF color;
    };
    std::vector<Fila> filas;
    int j = paginaConTexto(s);
    const js::Valor& pags = s->lectura["paginas"];
    if (j >= 0) {
        std::wstring desde = numeroDePagina(s->lecturaPagina);
        std::wstring donde = (std::size_t)j + 1 < pags.tam()
                                 ? L"Páginas " + desde + L" y " + numeroDePagina(W(pags[(std::size_t)j + 1]["ubicacion"]))
                                 : L"Página " + desde;
        filas.push_back({I_DIANA, L"Test de lo que estás leyendo", donde + L"  ·  al momento, sin esperar a la IA", A_L_TEST_PAGINA,
                         TURQUESA});
    }
    filas.push_back({I_JUGAR, L"Test rápido del tema", L"5 preguntas al momento", A_L_TEST_RAPIDO, tm.acento});
    std::string modo = cfg(s, "modo", "mixto");
    std::wstring nombreModo = modo == "ia" ? L"solo IA" : modo == "mixto" ? L"mixto" : L"rápido";
    filas.push_back({I_TEST, L"Test del tema", std::to_wstring(s->config["n"].comoEntero(10)) + L" preguntas  ·  modo " + nombreModo + L"  ·  " +
                                                   W(cfg(s, "dificultad", "Intermedia")),
                     A_L_TEST, AZUL});
    filas.push_back({I_RELOJ, L"Simulacro del tema", L"20 preguntas  ·  1 min cada una  ·  los fallos restan", A_L_SIMULACRO, ROSA});
    filas.push_back({I_REPETIR, L"Repasar mis fallos del tema", L"Las preguntas de este tema que te toca repasar", A_L_REPASO,
                     tm.medio});
    filas.push_back({I_DESARROLLO, L"Pregunta de desarrollo", L"Responde con tus palabras y el profesor te corrige", A_L_DESARROLLO,
                     VIOLETA});

    int filaAlto = E(s, 58), w = std::min(E(s, 460), derecha - (int)s->cuerpo.left - E(s, 20));
    int alto = E(s, 50) + (int)filas.size() * filaAlto + E(s, 10);
    int x = std::max((int)s->cuerpo.left + E(s, 10), std::min(derecha - E(s, 10) - w, centro - w / 2));
    int y = suelo - alto;
    zona(s, s->cuerpo, A_L_CERRAR_MENU);  // un clic fuera lo cierra
    RECT panel = rect(x, y, w, alto);
    RECT sombra = panel;
    OffsetRect(&sombra, 0, E(s, 4));
    redondeado(dc, sombra, E(s, 18), mezcla(tm.fondo, RGB(0, 0, 0), 0.4));
    redondeado(dc, panel, E(s, 16), tm.panel, mezcla(tm.borde, tm.acento, 0.35));
    zona(s, panel, A_NADA);
    texto(dc, s->f.negrita, tm.texto, L"Ponte a prueba con «" + W(s->lectura["tema"]) + L"»",
          rect(x + E(s, 18), y + E(s, 12), w - E(s, 36), E(s, 28)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    int fy = y + E(s, 46);
    for (const Fila& f : filas) {
        RECT r = rect(x + E(s, 8), fy, w - E(s, 16), filaAlto - E(s, 4));
        bool h = encima(s, f.accion);
        if (h) redondeado(dc, r, E(s, 12), mezcla(tm.panel, f.color, 0.12));
        int cx = r.left + E(s, 24), cy = (r.top + r.bottom) / 2;
        circulo(dc, cx, cy, E(s, 16), mezcla(tm.panel, f.color, 0.22));
        glifo(s, dc, f.icono, cx, cy, E(s, 15), f.color);
        texto(dc, s->f.texto, tm.texto, f.titulo, rect(cx + E(s, 26), r.top + E(s, 6), r.right - cx - E(s, 60), E(s, 24)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.mini, tm.textoSuave, f.detalle, rect(cx + E(s, 26), r.top + E(s, 29), r.right - cx - E(s, 60), E(s, 18)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        texto(dc, s->f.texto, h ? f.color : tm.textoSuave, L"→", rect(r.right - E(s, 36), r.top, E(s, 28), r.bottom - r.top),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        zona(s, r, f.accion);
        fy += filaAlto;
    }
}

// Barra flotante de abajo: pagina, tamaño, tests, resumen y preguntar. Devuelve su alto.
int pintarBarraLectura(Estado* s, HDC dc, int centro, int derecha, int nHojas) {
    const gui::Tema& tm = gui::tema();
    bool pdf = vistaPdf(s);
    int alto = E(s, 46), pad = E(s, 5), bh = alto - 2 * pad;
    std::wstring pagina = s->lecturaPagina.empty() ? L"" : s->lecturaPagina;
    if (pdf && nHojas > 0 && !pagina.empty()) pagina = L"Página " + numeroDePagina(pagina) + L" / " + std::to_wstring(nHojas);
    else if (!pagina.empty()) pagina[0] = (wchar_t)towupper(pagina[0]);
    std::wstring valor = pdf ? std::to_wstring(s->lecturaZoom) + L" %" : std::to_wstring(s->lecturaTam) + L" px";
    // El tamaño se vuelve al de inicio pulsando el propio valor (no hay un boton aparte que aparezca y desaparezca)
    bool ajustado = pdf ? s->lecturaZoom == 100 : s->lecturaTam == TAM_INICIAL;
    bool hayAnterior = destinoPagina(s, -1) >= 0, haySiguiente = destinoPagina(s, 1) >= 0;
    // Ancho fijo para la pagina: la barra no baila al pasar de la 9 a la 10
    std::wstring muestra = pdf && nHojas > 0 ? L"Página " + std::wstring(std::to_wstring(nHojas).size(), L'8') + L" / " + std::to_wstring(nHojas)
                                             : L"página 888";
    bool ia = iaDisponible(s);  // explicar y preguntar los responde la IA
    bool explicar = ia && paginaConTexto(s) >= 0;
    int disponible = derecha - (int)s->cuerpo.left - E(s, 24);
    // Con poco sitio se quitan los textos de los botones secundarios
    for (int compacto = 0; compacto < 2; ++compacto) {
        std::wstring tRes = compacto ? L"" : L"Resumen", tExp = compacto ? L"" : L"Explícame esta página";
        int w = pad;
        int anchoPag = std::max(anchoTexto(dc, s->f.peque, pagina), anchoTexto(dc, s->f.peque, muestra));
        if (!pagina.empty()) w += 2 * E(s, 30) + anchoPag + E(s, 16);
        w += E(s, 34) + anchoTexto(dc, s->f.peque, valor) + E(s, 14) + E(s, 34) + E(s, 8);
        w += E(s, 10) + pildora(s, dc, 0, 0, bh, I_TEST, L"Test  ▴", A_L_MENU_TEST, 0, true, tm.acento, true) + E(s, 4);
        w += pildora(s, dc, 0, 0, bh, I_RESUMEN, tRes, A_L_RESUMEN, 0, false, VIOLETA, true) + E(s, 4);
        if (explicar) w += pildora(s, dc, 0, 0, bh, I_BOMBILLA, tExp, A_L_EXPLICAR, 0, false, RGB(251, 191, 36), true) + E(s, 4);
        if (ia) w += pildora(s, dc, 0, 0, bh, I_DUDAS, compacto ? L"" : L"Preguntar", A_L_PREGUNTAR, 0, false, TURQUESA, true);
        w += pad;
        if (w > disponible && !compacto) continue;
        int x = std::max((int)s->cuerpo.left + E(s, 12), std::min(derecha - E(s, 12) - w, centro - w / 2));
        int y = s->cuerpo.bottom - alto - E(s, 16);
        RECT caja = rect(x, y, w, alto);
        RECT sombra = caja;
        OffsetRect(&sombra, 0, E(s, 4));
        redondeado(dc, sombra, alto / 2, mezcla(tm.fondo, RGB(0, 0, 0), 0.35));
        redondeado(dc, caja, alto / 2, tm.panel, mezcla(tm.borde, tm.acento, 0.3));
        zona(s, caja, A_NADA);
        int bx = x + pad, by = y + pad;
        if (!pagina.empty()) {
            // ‹ Página 3 / 20 ›: las flechas llevan al principio de la pagina anterior o la siguiente
            for (int lado = -1; lado <= 1; lado += 2) {
                bool puede = lado < 0 ? hayAnterior : haySiguiente;
                RECT rf = rect(lado < 0 ? bx : bx + E(s, 30) + anchoPag, by, E(s, 30), bh);
                if (puede && encima(s, A_L_PAGINA, lado)) redondeado(dc, rf, bh / 2, tm.panelAlt);
                texto(dc, s->f.negrita, puede ? tm.texto : tm.borde, lado < 0 ? L"‹" : L"›", rf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (puede) zona(s, rf, A_L_PAGINA, lado);
            }
            texto(dc, s->f.peque, tm.textoSuave, pagina, rect(bx + E(s, 30), y, anchoPag, alto), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            bx += 2 * E(s, 30) + anchoPag + E(s, 8);
            separador(dc, bx, y, alto);
            bx += E(s, 8);
        }
        // − 120 % +
        bool menos = pdf ? s->lecturaZoom > ZOOM_MIN : s->lecturaTam > TAM_MIN;
        bool mas = pdf ? s->lecturaZoom < ZOOM_MAX : s->lecturaTam < TAM_MAX;
        RECT rm = rect(bx, by, E(s, 34), bh), rM;
        bool hm = menos && encima(s, A_L_LETRA, -1);
        if (hm) redondeado(dc, rm, bh / 2, tm.panelAlt);
        texto(dc, s->f.negrita, menos ? tm.texto : tm.borde, pdf ? L"−" : L"A−", rm, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (menos) zona(s, rm, A_L_LETRA, -1);
        bx += E(s, 34);
        int vw = anchoTexto(dc, s->f.peque, valor) + E(s, 14);
        RECT rv = rect(bx, by, vw, bh);
        if (!ajustado) {  // pulsar el valor lo deja como al principio
            bool hv = encima(s, A_L_AJUSTAR);
            redondeado(dc, rv, bh / 2, hv ? mezcla(tm.panel, tm.acento, 0.22) : mezcla(tm.panel, tm.acento, 0.10));
            zona(s, rv, A_L_AJUSTAR);
        }
        texto(dc, s->f.peque, ajustado ? tm.textoSuave : tm.acento, valor, rect(bx, y, vw, alto), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        bx += vw;
        rM = rect(bx, by, E(s, 34), bh);
        if (mas && encima(s, A_L_LETRA, 1)) redondeado(dc, rM, bh / 2, tm.panelAlt);
        texto(dc, s->f.negrita, mas ? tm.texto : tm.borde, pdf ? L"+" : L"A+", rM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (mas) zona(s, rM, A_L_LETRA, 1);
        bx += E(s, 34) + E(s, 4);
        separador(dc, bx, y, alto);
        bx += E(s, 10);
        bx += pildora(s, dc, bx, by, bh, I_TEST, s->lecturaMenuTest ? L"Test  ▾" : L"Test  ▴", A_L_MENU_TEST, 0, true, tm.acento) + E(s, 4);
        bx += pildora(s, dc, bx, by, bh, I_RESUMEN, tRes, A_L_RESUMEN, 0, false, VIOLETA) + E(s, 4);
        if (explicar) bx += pildora(s, dc, bx, by, bh, I_BOMBILLA, tExp, A_L_EXPLICAR, 0, false, RGB(251, 191, 36)) + E(s, 4);
        if (ia) pildora(s, dc, bx, by, bh, I_DUDAS, compacto ? L"" : L"Preguntar", A_L_PREGUNTAR, 0, false, TURQUESA);
        if (s->lecturaMenuTest) pintarMenuTest(s, dc, centro, y - E(s, 10), derecha);
        return alto + E(s, 16);
    }
    return 0;
}

// Preguntas ya preparadas del panel del profesor: lo que se ve en el boton y lo que se le pide de
// verdad (sin pedido: explicar la pagina que se lee, que manda el texto de la pagina).
struct Sugerida {
    std::wstring titulo, pedido;
};

std::vector<Sugerida> sugeridas(Estado* s) {
    std::vector<Sugerida> v;
    if (!s->lecturaPanelCita.empty()) {  // sobre un trozo seleccionado
        v.push_back({L"Explícamelo con palabras sencillas", L"Explícame este fragmento de mis apuntes con palabras sencillas, paso a paso."});
        v.push_back({L"Ponme un ejemplo", L"Ponme un ejemplo concreto que ayude a entender este fragmento."});
        v.push_back({L"¿Qué es lo más importante aquí?", L"¿Cuáles son las ideas clave de este fragmento que no debo olvidar?"});
        v.push_back({L"¿Cómo puede caer en el examen?",
                     L"¿Cómo podrían preguntarme esto en el examen? Dime qué tendría que saber responder."});
        v.push_back({L"Compruébame si lo he entendido",
                     L"Hazme una pregunta corta sobre este fragmento para comprobar si lo he entendido. No me digas la respuesta todavía."});
        v.push_back({L"¿Con qué otros conceptos se relaciona?",
                     L"¿Con qué otros conceptos del temario se relaciona este fragmento y en qué se diferencian?"});
        return v;
    }
    // Pregunta libre mientras se lee: sobre la pagina o el documento
    std::wstring tema = L"«" + W(s->lectura["tema"]) + L"»";
    if (paginaConTexto(s) >= 0) v.push_back({L"Explícame la página que estoy leyendo", L""});
    v.push_back({L"Resúmeme las ideas clave del tema", L"Resúmeme las ideas clave de " + tema + L": lo esencial para entenderlo."});
    v.push_back({L"¿Qué es lo más importante para el examen?", L"¿Qué es lo más importante de " + tema + L" para el examen?"});
    v.push_back({L"Compruébame si lo he entendido",
                 L"Hazme una pregunta corta sobre " + tema + L" para comprobar si lo he entendido. No me digas la respuesta todavía."});
    return v;
}

// Panel de la derecha: lo que se pregunta sobre el apunte y las respuestas del profesor.
void pintarPanel(Estado* s, HDC dc, RECT p) {
    const gui::Tema& tm = gui::tema();
    RECT sombra = p;
    OffsetRect(&sombra, -E(s, 2), E(s, 3));
    redondeado(dc, sombra, E(s, 18), mezcla(tm.fondo, RGB(0, 0, 0), 0.35));
    redondeado(dc, p, E(s, 18), tm.panel, mezcla(tm.borde, tm.acento, 0.3));
    zona(s, p, A_NADA);
    int x = p.left + E(s, 18), w = p.right - p.left - E(s, 36), y = p.top + E(s, 16);
    // Cabecera: el profesor y cerrar
    avatar(s, dc, x + E(s, 18), y + E(s, 18), E(s, 18));
    texto(dc, s->f.negrita, tm.texto, L"Preguntar al profesor", rect(x + E(s, 46), y, w - E(s, 90), E(s, 22)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    texto(dc, s->f.mini, tm.textoSuave, s->chatEnCurso ? L"Escribiendo…" : L"Responde con tus apuntes",
          rect(x + E(s, 46), y + E(s, 20), w - E(s, 90), E(s, 18)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT cerrar = rect(p.right - E(s, 48), y + E(s, 2), E(s, 32), E(s, 32));
    if (encima(s, A_L_CERRAR_PANEL)) redondeado(dc, cerrar, E(s, 16), tm.panelAlt);
    texto(dc, s->f.texto, tm.textoSuave, L"✕", cerrar, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    zona(s, cerrar, A_L_CERRAR_PANEL);
    y += E(s, 50);
    // De que se habla: cita con una barra de color
    if (!s->lecturaPanelCita.empty()) {
        std::wstring cita = s->lecturaPanelCita.size() > 420 ? s->lecturaPanelCita.substr(0, 420) + L"…" : s->lecturaPanelCita;
        int ch = std::min(E(s, 96), altoTexto(dc, s->f.peque, cita, w - E(s, 26)));
        RECT caja = rect(x, y, w, ch + E(s, 20));
        redondeado(dc, caja, E(s, 10), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.10 : 0.06));
        redondeado(dc, rect(x, y, E(s, 3), ch + E(s, 20)), E(s, 2), tm.acento);
        texto(dc, s->f.peque, mezcla(tm.texto, tm.textoSuave, 0.35), cita, rect(x + E(s, 14), y + E(s, 10), w - E(s, 24), ch),
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL);
        y = caja.bottom + E(s, 12);
    }
    // Abajo: atajos y el campo para escribir
    int campoAlto = E(s, 50);
    RECT campo = rect(x, p.bottom - E(s, 16) - campoAlto, w, campoAlto);
    int chipsY = campo.top - E(s, 44);
    bool vacio = s->chat.size() <= s->lecturaChatDesde;  // sin atajos debajo: las preguntas preparadas llegan al campo
    int fin = (vacio ? campo.top : chipsY) - E(s, 10);
    // Conversacion (las ultimas, pegadas abajo; lo que no cabe se corta por arriba)
    std::vector<int> altos;
    int total = 0;
    for (std::size_t i = s->lecturaChatDesde; i < s->chat.size(); ++i) {
        const Mensaje& m = s->chat[i];
        int h = m.alumno ? altoTexto(dc, s->f.texto, m.texto, w - E(s, 60)) + E(s, 22)
                         : (m.texto.empty() ? E(s, 36) : textoRico(s, dc, m.texto, x, 0, w, false)) + E(s, 12);
        altos.push_back(h);
        total += h + E(s, 10);
    }
    if (altos.empty()) {
        // Aun no se ha preguntado nada: preguntas preparadas para elegir (o escribir otra abajo)
        texto(dc, s->f.negrita, tm.texto, L"¿Qué quieres saber?", rect(x, y, w, E(s, 24)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.mini, tm.textoSuave, L"Elige una pregunta o escribe la tuya abajo", rect(x, y + E(s, 24), w, E(s, 18)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        std::vector<Sugerida> lista = sugeridas(s);
        int fy = y + E(s, 52), filaAlto = E(s, 44);
        for (std::size_t i = 0; i < lista.size() && fy + filaAlto - E(s, 6) <= fin; ++i, fy += filaAlto) {
            RECT r = rect(x, fy, w, filaAlto - E(s, 6));
            bool h = encima(s, A_L_SUGERIDA, (int)i);
            redondeado(dc, r, E(s, 12), h ? mezcla(tm.panel, tm.acento, 0.14) : mezcla(tm.panel, tm.panelAlt, 0.6),
                       h ? mezcla(tm.borde, tm.acento, 0.6) : tm.borde);
            texto(dc, s->f.peque, h ? tm.texto : mezcla(tm.texto, tm.textoSuave, 0.2), lista[i].titulo,
                  rect(r.left + E(s, 14), r.top, w - E(s, 48), r.bottom - r.top), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            texto(dc, s->f.texto, h ? tm.acento : tm.textoSuave, L"→", rect(r.right - E(s, 32), r.top, E(s, 24), r.bottom - r.top),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            zona(s, r, A_L_SUGERIDA, (int)i);
        }
    } else {
        HRGN region = CreateRectRgn(p.left, y, p.right, fin);
        SelectClipRgn(dc, region);
        int my = std::min(y, fin - total);
        for (std::size_t k = 0; k < altos.size(); ++k) {
            const Mensaje& m = s->chat[s->lecturaChatDesde + k];
            int h = altos[k];
            if (my + h >= y) {
                if (m.alumno) {
                    int bw = std::min(w - E(s, 30), anchoTexto(dc, s->f.texto, m.texto) + E(s, 28));
                    RECT b = rect(x + w - bw, my, bw, h);
                    redondeado(dc, b, E(s, 14), mezcla(tm.panel, tm.acento, tm.oscuro ? 0.28 : 0.16));
                    texto(dc, s->f.texto, tm.texto, m.texto, rect(b.left + E(s, 14), my + E(s, 11), bw - E(s, 28), h - E(s, 16)),
                          DT_LEFT | DT_TOP | DT_WORDBREAK);
                } else if (m.texto.empty()) {
                    spinner(s, dc, x + E(s, 14), my + E(s, 18), E(s, 9));
                    int seg = s->chatDesde ? (int)((GetTickCount64() - s->chatDesde) / 1000) : 0;
                    std::wstring fase = seg < 45 ? L"Leyendo tus apuntes…" : L"Preparando la explicación…";
                    if (seg >= 4) fase += L"  " + tiempo(seg);
                    texto(dc, s->f.peque, tm.textoSuave, fase, rect(x + E(s, 34), my, w - E(s, 34), E(s, 36)),
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                } else {
                    textoRico(s, dc, m.texto, x, my, w);
                }
            }
            my += h + E(s, 10);
        }
        SelectClipRgn(dc, NULL);
        DeleteObject(region);
        if (total > fin - y)  // hay mas arriba: un degradado lo indica
            for (int k = 0; k < E(s, 18); ++k)
                velo(dc, rect(p.left + E(s, 1), y + k, p.right - p.left - E(s, 2), 1), tm.panel, 230 - k * 230 / E(s, 18));
    }
    // Atajos (o «Detener» mientras escribe)
    if (s->chatEnCurso) {
        int cw = anchoTexto(dc, s->f.peque, L"Detener") + E(s, 40);
        RECT r = rect(x, chipsY, cw, E(s, 32));
        bool h = encima(s, A_CANCELAR_DUDA);
        redondeado(dc, r, E(s, 16), h ? mezcla(tm.panel, tm.mal, 0.18) : tm.panel, mezcla(tm.borde, tm.mal, 0.5));
        redondeado(dc, rect(r.left + E(s, 13), chipsY + E(s, 11), E(s, 10), E(s, 10)), E(s, 2), tm.mal);
        texto(dc, s->f.peque, tm.texto, L"Detener", rect(r.left + E(s, 30), chipsY, cw - E(s, 34), E(s, 32)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        zona(s, r, A_CANCELAR_DUDA);
    } else if (!altos.empty()) {  // para seguir preguntando sobre la respuesta
        const wchar_t* textos[] = {L"Más sencillo", L"Ponme un ejemplo", L"Ponme a prueba", L"En Dudas →"};
        int cx = x;
        for (int i = 0; i < 4; ++i) {
            int cw = anchoTexto(dc, s->f.peque, textos[i]) + E(s, 22);
            if (cx + cw > x + w) break;
            RECT r = rect(cx, chipsY, cw, E(s, 32));
            bool h = encima(s, A_L_RAPIDA, i);
            redondeado(dc, r, E(s, 16), h ? tm.panelAlt : tm.panel, h ? mezcla(tm.borde, tm.acento, 0.5) : tm.borde);
            texto(dc, s->f.peque, h ? tm.texto : tm.textoSuave, textos[i], r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            zona(s, r, A_L_RAPIDA, i);
            cx += cw + E(s, 6);
        }
    }
    // Campo (el EDIT del chat se coloca dentro)
    bool foco = GetFocus() == s->edit;
    redondeado(dc, campo, E(s, 16), tm.panel, foco ? tm.acento : mezcla(tm.borde, tm.textoSuave, 0.25), foco ? 2 : 1);
    RECT dentro = rect(campo.left + E(s, 14), campo.top + E(s, 13), campo.right - campo.left - E(s, 60), campo.bottom - campo.top - E(s, 22));
    RECT enviarR = rect(campo.right - E(s, 42), campo.top + E(s, 8), E(s, 34), E(s, 34));
    circulo(dc, (enviarR.left + enviarR.right) / 2, (enviarR.top + enviarR.bottom) / 2, E(s, 17),
            s->chatEnCurso ? tm.borde : (encima(s, A_ENVIAR) ? mezcla(tm.acento, RGB(255, 255, 255), 0.15) : tm.acento));
    icono(s, dc, I_FLECHA, (enviarR.left + enviarR.right) / 2, (enviarR.top + enviarR.bottom) / 2, E(s, 15), RGB(255, 255, 255));
    zona(s, enviarR, A_ENVIAR);
    s->lecturaCampo = dentro;
    RECT ahora;
    GetWindowRect(s->edit, &ahora);
    MapWindowPoints(NULL, s->ventana, (POINT*)&ahora, 2);
    if (!EqualRect(&dentro, &ahora))
        MoveWindow(s->edit, dentro.left, dentro.top, dentro.right - dentro.left, dentro.bottom - dentro.top, TRUE);
}

}  // namespace

void soltarHojas(Estado* s) {
    for (auto& par : s->lecturaHojas) delete par.second;
    s->lecturaHojas.clear();
    s->lecturaJpg.clear();
    s->lecturaJpgAncho.clear();
    s->lecturaPedida.clear();
    s->lecturaPeticiones.clear();
    s->lecturaFallida.clear();
}

void guardarLectura(Estado* s) {
    if (s->lectura.esNulo()) return;
    if (std::fabs(s->lecturaPos - s->lecturaGuardada) < 0.005) return;
    js::Valor p = js::Valor::objeto();
    p["ruta"] = s->lectura["ruta"].comoCadena();
    p["posicion"] = s->lecturaPos;
    enviar(s, "posicion_lectura", p);
    s->lecturaGuardada = s->lecturaPos;
}

void abrirTemario(Estado* s, const std::string& ruta) {
    if (!ruta.empty() && ruta != s->lecturaRuta) {
        guardarLectura(s);
        s->lectura = js::Valor();
        s->lecturaRuta = ruta;
        quitarSeleccion(s);
    }
    bool primera = s->temario.esNulo();
    if (primera) s->abrirAlLlegar = V_TEMARIO;
    enviar(s, "temario", js::Valor::objeto(), primera, L"Reuniendo tu temario…");
    if (!primera) {
        irAVista(s, V_TEMARIO);
        if (s->lectura.esNulo() || s->lectura["ruta"].comoCadena() != s->lecturaRuta) pedirDoc(s, s->lecturaRuta);
        else s->lecturaIrA = s->lecturaPos;  // volver a donde se estaba
    }
    s->mensaje = iaDisponible(s) ? L"Lee con calma: recuerdo por dónde vas. Si algo no lo entiendes, selecciónalo arrastrando el "
                                   L"ratón y pregúntame por ello."
                                 : L"Lee con calma: recuerdo por dónde vas. Selecciona un trozo con el ratón para hacer un test "
                                   L"de él, buscarlo en el temario o copiarlo.";
}

int anchoPanelLectura(Estado* s) {
    if (s->vista != V_TEMARIO || !s->lecturaPanel || s->lectura.esNulo()) return 0;
    RECT c;
    GetClientRect(s->ventana, &c);
    return std::max(E(s, 340), std::min(E(s, 460), (int)c.right * 30 / 100));
}

bool campoEnPanelLectura(Estado* s) { return anchoPanelLectura(s) > 0; }

int pintarTemario(Estado* s, HDC dc, Area a) {
    const gui::Tema& tm = gui::tema();
    int x = a.x, y = a.y, w = a.w, y0 = y;
    if (s->lecturaPanel && !iaDisponible(s) && !s->chatEnCurso) {  // la IA se ha apagado: no queda a quien preguntar
        s->lecturaPanel = false;
        s->lecturaCampo = RECT();
        colocarEdit(s);
    }
    const js::Valor& docs = s->temario["temas"];
    bool conLista = s->lecturaLista && !s->enfoque;
    s->lecturaPalabras.clear();
    s->lecturaVisibles.clear();
    s->lecturaSelRect = RECT();
    int anchoPanel = anchoPanelLectura(s);
    int derecha = s->cuerpo.right - anchoPanel;  // lo que queda a la izquierda del panel del profesor

    if (!s->enfoque) {
        int leidos = s->temario["leidos"].comoEntero(), total = s->temario["total"].comoEntero();
        y += cabecera(s, dc, x, y, w, I_LIBRO, AZUL, L"Temario",
                      std::to_wstring(leidos) + L" de " + std::to_wstring(total) +
                          (iaDisponible(s) ? L" documentos leídos. Selecciona cualquier trozo con el ratón para preguntar al profesor por él."
                                           : L" documentos leídos. Selecciona un trozo con el ratón para hacer un test de él o buscarlo."));
    }

    int wl = conLista ? std::max(E(s, 230), std::min(E(s, 320), w * 28 / 100)) : 0;
    int xr = conLista ? x + wl + E(s, 20) : x, wr = conLista ? w - wl - E(s, 20) : w;

    // --- Lista de documentos por temas: fija a la izquierda (no se va con el documento al bajar) y
    // con su propio desplazamiento: la rueda encima de ella o su barra la mueven solo a ella
    if (conLista) {
        int filaAlto = E(s, 52), subAlto = E(s, 60);
        std::vector<FilaTema> filas = filasPorTema(docs);
        int contenido = E(s, 10);
        for (const FilaTema& f : filas) contenido += f.indice < 0 ? subAlto : filaAlto;
        if (docs.tam() == 0) contenido = filaAlto + E(s, 10);
        int arriba = std::max(y, (int)s->cuerpo.top + E(s, 12)), abajo = s->cuerpo.bottom - E(s, 16);
        int hueco = std::max(filaAlto, abajo - arriba);
        RECT lista = rect(x, arriba, wl, std::min(contenido, hueco));
        s->lecturaListaMax = std::max(0, contenido - hueco);
        // Al abrir otro documento (o al volver), la lista lo pone a la vista
        if (s->lecturaListaRuta != s->lecturaRuta) {
            s->lecturaListaRuta = s->lecturaRuta;
            int fy = E(s, 5);
            for (const FilaTema& f : filas) {
                int h = f.indice < 0 ? subAlto : filaAlto;
                if (f.indice >= 0 && docs[(std::size_t)f.indice]["ruta"].comoCadena() == s->lecturaRuta) {
                    if (fy < s->lecturaListaScroll + E(s, 5)) s->lecturaListaScroll = fy - subAlto;  // con el titulo de su tema
                    else if (fy + h > s->lecturaListaScroll + hueco - E(s, 5)) s->lecturaListaScroll = fy + h - hueco + E(s, 10);
                    break;
                }
                fy += h;
            }
        }
        s->lecturaListaScroll = std::max(0, std::min(s->lecturaListaMax, s->lecturaListaScroll));
        cristal(dc, lista, E(s, 14));
        zona(s, lista, A_NADA);  // pulsar en la lista no empieza a seleccionar texto
        s->lecturaListaRect = lista;
        if (docs.tam() == 0)
            texto(dc, s->f.texto, tm.textoSuave, L"Tu Temario está vacío.", rect(x + E(s, 16), arriba, wl, filaAlto),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT dentro = rect(lista.left, lista.top + E(s, 2), wl, lista.bottom - lista.top - E(s, 4));
        int guardado = SaveDC(dc);
        IntersectClipRect(dc, dentro.left, dentro.top, dentro.right, dentro.bottom);
        int conBarra = s->lecturaListaMax > 0 ? E(s, 10) : 0;  // hueco a la derecha para su barra
        int fy = arriba + E(s, 5) - s->lecturaListaScroll;
        for (const FilaTema& f : filas) {
            int h = f.indice < 0 ? subAlto : filaAlto;
            if (fy + h <= lista.top || fy >= lista.bottom) {  // no se ve: ni se pinta ni se puede pulsar
                fy += h;
                continue;
            }
            if (f.indice < 0) {
                int hechos = 0;
                for (std::size_t j = 0; j < docs.tam(); ++j)
                    if (W(docs[j]["grupo"]) == f.grupo && docs[j]["leido"].comoBool()) ++hechos;
                wchar_t info[48];
                swprintf(info, 48, L"%d/%d", hechos, f.miembros);
                subtituloTema(s, dc, rect(x + E(s, 14), fy + E(s, 6), wl - E(s, 28) - conBarra, subAlto - E(s, 8)), f.grupo, info,
                              hechos == f.miembros && hechos > 0 ? VERDE : AZUL);
                fy += subAlto;
                continue;
            }
            const js::Valor& d = docs[(std::size_t)f.indice];
            RECT r = rect(x + E(s, 5), fy, wl - E(s, 10) - conBarra, filaAlto);
            fy += filaAlto;
            bool legible_ = legible(d);
            bool elegido = d["ruta"].comoCadena() == s->lecturaRuta;
            bool leido = d["leido"].comoBool();
            double pos = d["posicion"].comoNumero();
            bool empezado = !leido && pos > 0.02;
            if (elegido) {
                redondeado(dc, r, E(s, 10), mezcla(tm.panel, AZUL, tm.oscuro ? 0.20 : 0.14));
                redondeado(dc, rect(r.left, r.top + E(s, 12), E(s, 3), filaAlto - E(s, 24)), E(s, 2), AZUL);
            } else if (legible_ && encima(s, A_L_DOC, f.indice)) {
                redondeado(dc, r, E(s, 10), tm.panelAlt);
            }
            int cx = r.left + E(s, 20), cy = r.top + filaAlto / 2;
            if (leido) {
                circulo(dc, cx, cy, E(s, 9), mezcla(tm.panel, VERDE, 0.85));
                icono(s, dc, I_CHECK, cx, cy, E(s, 12), RGB(255, 255, 255));
            } else if (empezado) {
                // Anillo con lo que lleva leido
                circulo(dc, cx, cy, E(s, 8), tm.panel, mezcla(tm.borde, tm.textoSuave, 0.25), 2.0f);
                arco(dc, cx, cy, E(s, 8), 2.5f, -90.0f, (float)(360.0 * std::min(1.0, pos)), AZUL);
            } else {
                circulo(dc, cx, cy, E(s, 6), tm.panel, mezcla(tm.borde, tm.textoSuave, 0.4), 1.5f);
            }
            texto(dc, elegido ? s->f.negrita : s->f.texto, legible_ ? tm.texto : tm.textoSuave, W(d["tema"]),
                  rect(cx + E(s, 18), r.top + E(s, 6), r.right - cx - E(s, 26), E(s, 22)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            std::wstring info = !legible_ ? L"No se puede leer aquí: se abre aparte"
                                         : plural(d["paginas"].comoEntero(), L"página", L"páginas") +
                                               (leido ? L"  ·  leído" : empezado ? L"  ·  vas por el " + std::to_wstring(porcentaje(pos)) + L" %" : L"");
            texto(dc, s->f.mini, empezado ? mezcla(tm.textoSuave, AZUL, 0.5) : tm.textoSuave, info,
                  rect(cx + E(s, 18), r.top + E(s, 28), r.right - cx - E(s, 26), E(s, 18)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT clic;  // solo la parte de la fila que se ve se puede pulsar
            if (IntersectRect(&clic, &r, &dentro)) zona(s, clic, A_L_DOC, f.indice);
        }
        RestoreDC(dc, guardado);
        if (s->lecturaListaMax > 0) {
            // Degradados arriba y abajo cuando quedan filas por ese lado, y su propia barra (arrastrable)
            int velo_ = E(s, 14);
            if (s->lecturaListaScroll > 0)
                for (int k = 0; k < velo_; ++k)
                    velo(dc, rect(lista.left + E(s, 2), lista.top + E(s, 2) + k, wl - E(s, 4), 1), tm.panel, 200 - k * 200 / velo_);
            if (s->lecturaListaScroll < s->lecturaListaMax)
                for (int k = 0; k < velo_; ++k)
                    velo(dc, rect(lista.left + E(s, 2), lista.bottom - E(s, 3) - k, wl - E(s, 4), 1), tm.panel, 200 - k * 200 / velo_);
            int pista = (int)(lista.bottom - lista.top) - E(s, 20);
            int largo = std::max(E(s, 30), pista * (int)(lista.bottom - lista.top) / std::max(1, contenido));
            int posB = (pista - largo) * s->lecturaListaScroll / std::max(1, s->lecturaListaMax);
            s->listaCarril = rect(lista.right - E(s, 14), lista.top + E(s, 10), E(s, 14), pista);
            bool activa = s->barraArrastre == 2 || s->barraEncima == 2;
            int bw = activa ? E(s, 7) : E(s, 4);
            RECT pulgar = rect(lista.right - E(s, 8) - bw / 2, lista.top + E(s, 10) + posB, bw, largo);
            if (activa) redondeado(dc, rect(pulgar.left, s->listaCarril.top, bw, pista), bw / 2, mezcla(tm.panel, tm.textoSuave, 0.12));
            redondeado(dc, pulgar, bw / 2, mezcla(tm.panel, tm.textoSuave, activa ? 0.7 : 0.4));
            s->listaPulgar = rect(s->listaCarril.left, pulgar.top, E(s, 14), largo);
        }
    }

    // --- Documento
    int ry = y;
    const js::Valor& l = s->lectura;
    if (l.esNulo()) {
        RECT r = rect(xr, ry, wr, E(s, 160));
        cristal(dc, r, E(s, 18));
        texto(dc, s->f.texto, tm.textoSuave, s->lecturaRuta.empty() ? L"Elige un documento de la lista para leerlo." : L"Abriendo…",
              r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return (int)r.bottom - y0;
    }
    bool pdf = vistaPdf(s);
    int pad = s->enfoque ? E(s, 48) : E(s, 32);
    int tx = xr + pad, tw = wr - 2 * pad;
    int centro = xr + wr / 2;
    // Paginas ampliadas: crecen hacia los lados dentro de lo que se ve [vi, vd) sin tapar la lista; si
    // aun asi no caben, se centran y lecturaDx las mueve de lado
    int anchoHoja = std::max(E(s, 200), tw * s->lecturaZoom / 100);
    int vi = conLista ? xr + E(s, 12) : (int)s->cuerpo.left + E(s, 20), vd = derecha - E(s, 20);
    int visible = vd - vi;
    int limite = pdf ? std::max(0, (anchoHoja - visible) / 2) : 0;
    s->lecturaDx = std::max(-limite, std::min(limite, s->lecturaDx));
    int xHoja = anchoHoja <= visible ? std::max(vi, std::min(vd - anchoHoja, centro - anchoHoja / 2))
                                     : (vi + vd) / 2 - anchoHoja / 2 - s->lecturaDx;
    // Cabecera del documento
    // Botones de la cabecera: los que no caben en una fila pasan a la siguiente
    struct Btn {
        int ancho;
        Icono ic;
        std::wstring t;
        int accion;
        EstiloBoton e;
    };
    std::vector<Btn> btns;
    // Cada boton, tan ancho como su texto (icono + hueco + texto + margenes)
    auto ancho = [&](const wchar_t* t) { return anchoTexto(dc, s->f.negrita, t) + E(s, 24) + E(s, 26); };
    const wchar_t* tCompleta = s->enfoque ? L"Salir de pantalla completa" : L"Pantalla completa";
    btns.push_back({ancho(tCompleta), I_EXPORTAR, tCompleta, A_L_COMPLETA, PRIMARIO});
    // Cambiar de vista solo tiene sentido si el documento tiene las dos (PDF con texto)
    if (l["visual"].comoBool() && l["hojas"].tam() > 0 && l["paginas"].tam() > 0) {
        const wchar_t* tModo = pdf ? L"Solo el texto" : L"Ver como el PDF";
        btns.push_back({ancho(tModo), pdf ? I_RESUMEN : I_LIBRO, tModo, A_L_MODO, SECUNDARIO});
    }
    if (!s->enfoque) {
        const wchar_t* tLista = s->lecturaLista ? L"Ocultar lista" : L"Ver lista";
        btns.push_back({ancho(tLista), I_LIBRO, tLista, A_L_LISTA, FANTASMA});
    }
    btns.push_back({ancho(L"Abrir el original"), I_CARPETA, L"Abrir el original", A_L_ABRIR, FANTASMA});
    int bh = E(s, 38), filasBtn = 1;
    {
        int ocupado = 0;
        for (const Btn& b : btns) {
            if (ocupado > 0 && ocupado + b.ancho > tw) {
                ++filasBtn;
                ocupado = 0;
            }
            ocupado += b.ancho + E(s, 10);
        }
    }
    int cabeza = E(s, 164) + (pad - E(s, 32)) + (filasBtn - 1) * (bh + E(s, 10));  // en pantalla completa el margen es mayor
    int cy = ry + pad - E(s, 8);
    int textoArriba = ry + cabeza + E(s, 22);
    std::vector<std::wstring> ps;
    std::vector<RECT> hojas;
    int altoTexto_ = 0;
    // Vista de texto: lineas de unos 75 caracteres, como en un libro (las muy largas cansan la vista)
    int twT = pdf ? tw : std::min(tw, E(s, 46 * s->lecturaTam));
    int txT = tx + (tw - twT) / 2;
    if (pdf) {
        hojas = colocarHojas(s, xHoja, tw, textoArriba);
        if (!hojas.empty()) altoTexto_ = hojas.back().bottom - textoArriba;
    } else {
        ps = parrafos(l);
        // Alto de cada parrafo (se recalcula solo si cambia el ancho, la letra o el documento)
        if ((int)s->lecturaAltos.size() != (int)ps.size() || s->lecturaAncho != twT) {
            s->lecturaAltos.clear();
            for (const std::wstring& p : ps)
                s->lecturaAltos.push_back(p[0] == L'\x01' ? E(s, 44)
                                                          : maquetar(dc, esTitulo(p) ? s->f.subtitulo : s->f.lectura, p, twT, NULL) +
                                                                E(s, 12) * s->lecturaTam / 17);
            s->lecturaAncho = twT;
        }
        for (int h : s->lecturaAltos) altoTexto_ += h;
    }
    RECT tarjeta = rect(xr, ry, wr, cabeza + altoTexto_ + E(s, 150));
    if (pdf && anchoHoja + 2 * pad > wr) {  // paginas ampliadas: la tarjeta se ensancha con ellas (hasta donde se ve)
        tarjeta.left = std::max(conLista ? xr : (int)s->cuerpo.left + E(s, 12), std::min(xr, xHoja - pad));
        tarjeta.right = std::min(derecha - E(s, 12), std::max(xr + wr, xHoja + anchoHoja + pad));
    }
    cristal(dc, tarjeta, E(s, 18));

    texto(dc, s->f.mini, AZUL, W(l["grupo"]), rect(tx, cy, tw, E(s, 18)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    // «Leído»: una pastilla que se pulsa para marcarlo o desmarcarlo (antes era un boton mas en la fila)
    int anchoLeido = anchoTexto(dc, s->f.peque, L"Marcar como leído") + E(s, 30);
    {
        bool leido = l["leido"].comoBool(), h = encima(s, A_L_LEIDO);
        std::wstring t = leido ? (h ? L"Marcar sin leer" : L"✓  Leído") : L"Marcar como leído";
        int pw = leido && !h ? anchoTexto(dc, s->f.peque, t) + E(s, 30) : anchoLeido;
        RECT r = rect(tx + tw - pw, cy + E(s, 24), pw, E(s, 28));
        COLORREF c = leido ? VERDE : tm.textoSuave;
        redondeado(dc, r, E(s, 14), leido ? mezcla(tm.panel, VERDE, h ? 0.10 : 0.16) : (h ? tm.panelAlt : tm.panel),
                   mezcla(tm.panel, c, leido ? 0.45 : 0.35));
        texto(dc, s->f.peque, leido ? mezcla(VERDE, tm.texto, 0.2) : (h ? tm.texto : tm.textoSuave), t, r,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        zona(s, r, A_L_LEIDO);
    }
    texto(dc, s->f.titulo, tm.texto, W(l["tema"]), rect(tx, cy + E(s, 18), tw - anchoLeido - E(s, 16), E(s, 40)),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    // Paginas del original y, si no todas tienen texto, cuantas se pueden leer aqui
    int conTexto = (int)l["paginas"].tam();
    int i = indiceDoc(s, l["ruta"].comoCadena());
    int totales = i >= 0 ? docs[(std::size_t)i]["paginas"].comoEntero() : conTexto;
    wchar_t meta[160];
    std::wstring nPags = plural(pdf ? (int)hojas.size() : conTexto, L"página", L"páginas");
    if (pdf)
        swprintf(meta, 160, l["palabras"].comoEntero() ? L"%ls  ·  unos %d min de lectura" : L"%ls", nPags.c_str(),
                 l["minutos"].comoEntero());
    else if (totales > conTexto)
        swprintf(meta, 160, L"%d páginas (%d con texto)  ·  %d palabras  ·  unos %d min de lectura", totales, conTexto,
                 l["palabras"].comoEntero(), l["minutos"].comoEntero());
    else
        swprintf(meta, 160, L"%d páginas  ·  %d palabras  ·  unos %d min de lectura", conTexto, l["palabras"].comoEntero(),
                 l["minutos"].comoEntero());
    texto(dc, s->f.peque, tm.textoSuave, meta, rect(tx, cy + E(s, 60), tw, E(s, 20)), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    // Botones
    {
        int by = cy + E(s, 92), bx = tx;
        for (const Btn& b : btns) {
            if (bx > tx && bx + b.ancho > tx + tw) {
                bx = tx;
                by += bh + E(s, 10);
            }
            botonIcono(s, dc, rect(bx, by, b.ancho, bh), b.ic, b.t, b.accion, 0, b.e);
            bx += b.ancho + E(s, 10);
        }
    }

    // Paginas del PDF o texto, pagina a pagina (solo se dibuja lo que se ve)
    int py = textoArriba;
    std::wstring paginaActual;
    s->lecturaInicios.clear();  // para saltar de pagina en pagina (flechas de la barra, AvPág y RePág)
    for (const RECT& h : hojas) s->lecturaInicios.push_back((int)h.top - E(s, ETIQUETA_HOJA) + s->scroll);
    if (pdf) {
        int guardado = SaveDC(dc);  // lo que se sale de lado no pisa la lista ni el panel
        IntersectClipRect(dc, tarjeta.left, s->cuerpo.top, tarjeta.right, s->cuerpo.bottom);
        pintarHojas(s, dc, hojas, paginaActual);
        RestoreDC(dc, guardado);
        py = textoArriba + altoTexto_;
    }
    COLORREF colorSel = mezcla(tm.panel, tm.acento, tm.oscuro ? 0.34 : 0.24);
    for (std::size_t k = 0; k < ps.size(); ++k) {
        int h = s->lecturaAltos[k];
        if (ps[k][0] == L'\x01') s->lecturaInicios.push_back(py + s->scroll);
        if (ps[k][0] == L'\x01' && py <= s->cuerpo.top + (s->cuerpo.bottom - s->cuerpo.top) / 3) paginaActual = ps[k].substr(1);
        if (py + h >= s->cuerpo.top && py <= s->cuerpo.bottom) {
            if (ps[k][0] == L'\x01') {
                std::wstring pag = ps[k].substr(1);
                int pw = anchoTexto(dc, s->f.mini, pag) + E(s, 16);
                redondeado(dc, rect(txT + pw + E(s, 10), py + E(s, 20), twT - pw - E(s, 10), 1), 0, mezcla(tm.panel, tm.borde, 0.6));
                RECT pr = rect(txT, py + E(s, 10), pw, E(s, 20));
                redondeado(dc, pr, E(s, 10), tm.panel, mezcla(tm.panel, tm.borde, 0.8));
                texto(dc, s->f.mini, tm.textoSuave, pag, pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                bool titulo = esTitulo(ps[k]);
                HFONT f = titulo ? s->f.subtitulo : s->f.lectura;
                std::vector<Colocada> cs;
                maquetar(dc, f, ps[k], twT, &cs);
                int lh = interlineado(dc, f);
                TEXTMETRICW m;
                GetTextMetricsW(dc, &m);
                int bajada = (lh - m.tmHeight) / 2;
                // Fondo de lo seleccionado (hasta la palabra siguiente si tambien lo esta y va en la misma linea)
                for (std::size_t j = 0; j < cs.size(); ++j) {
                    if (!seleccionada(s, (int)k, (int)j)) continue;
                    int fin = cs[j].x + cs[j].w;
                    if (j + 1 < cs.size() && cs[j + 1].y == cs[j].y && seleccionada(s, (int)k, (int)j + 1)) fin = cs[j + 1].x;
                    RECT r = {txT + cs[j].x - E(s, 1), py + cs[j].y, txT + fin + E(s, 1), py + cs[j].y + lh};
                    redondeado(dc, r, E(s, 3), colorSel);
                    if (s->lecturaSel.empty() || s->lecturaArrastre) s->lecturaSelRect = r;
                    else if (j + 1 == cs.size() || !seleccionada(s, (int)k, (int)j + 1)) s->lecturaSelRect = r;
                }
                SelectObject(dc, f);
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, titulo ? tm.texto : mezcla(tm.texto, tm.textoSuave, 0.15));
                for (std::size_t j = 0; j < cs.size(); ++j) {
                    const Colocada& c = cs[j];
                    TextOutW(dc, txT + c.x, py + c.y + bajada, ps[k].c_str() + c.ini, (int)c.len);
                    Estado::PalabraVista pv;
                    pv.r = {txT + c.x, py + c.y, txT + c.x + c.w, py + c.y + lh};
                    pv.parrafo = (int)k;
                    pv.palabra = (int)j;
                    s->lecturaPalabras.push_back(pv);
                }
            }
        }
        py += h;
    }
    if (paginaActual.empty() && !ps.empty()) paginaActual = ps[0].substr(1);  // aun arriba del todo: la primera
    if (paginaActual.empty() && !s->lecturaVisibles.empty())
        paginaActual = L"página " + std::to_wstring(s->lecturaVisibles.front().first + 1);
    s->lecturaPagina = paginaActual;

    // Cuanto se ha visto ya: lo que queda por encima del borde de abajo de la pantalla
    double fraccion = py > textoArriba ? (double)(s->cuerpo.bottom - textoArriba) / (py - textoArriba) : 1.0;
    fraccion = std::max(0.0, std::min(1.0, fraccion));
    s->lecturaPos = fraccion;

    // Separador de la cabecera que hace de barra de progreso
    redondeado(dc, rect(tx, ry + cabeza, tw, E(s, 2)), E(s, 1), mezcla(tm.panel, tm.borde, 0.9));
    redondeado(dc, rect(tx, ry + cabeza, std::max(E(s, 2), (int)(tw * fraccion)), E(s, 2)), E(s, 1), AZUL);

    // Final: queda marcado como leido al llegar aqui
    int fy = py + E(s, 20);
    bool saltando = s->lecturaIrA >= 0;
    if (!saltando && fy <= s->cuerpo.bottom && !s->lecturaMarcada && !l["leido"].comoBool() && (!ps.empty() || !hojas.empty())) {
        s->lecturaMarcada = true;
        marcar(s, true);
        aviso(s, L"«" + W(l["tema"]) + L"» leído ✓");
    }
    texto(dc, s->f.negrita, tm.texto, L"Fin del documento", rect(tx, fy, tw, E(s, 24)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int ant = vecino(s, -1), sig = vecino(s, 1);
    int bw = std::min(E(s, 300), (tw - E(s, 12)) / 2);
    if (ant >= 0)
        botonIcono(s, dc, rect(tx, fy + E(s, 40), bw, E(s, 46)), I_FLECHA, L"Anterior: " + W(docs[(std::size_t)ant]["tema"]), A_L_ANTERIOR, 0,
                   SECUNDARIO);
    if (sig >= 0)
        botonIcono(s, dc, rect(tx + tw - bw, fy + E(s, 40), bw, E(s, 46)), I_FLECHA, L"Siguiente: " + W(docs[(std::size_t)sig]["tema"]),
                   A_L_SIGUIENTE, 0, PRIMARIO);
    int fin = std::max((int)tarjeta.bottom, fy + E(s, 110)) + E(s, 70);  // y sitio para la barra de abajo
    int alto = fin - y0;  // la lista va fija: no cuenta para lo que se desplaza

    // Franja fija arriba cuando la cabecera ya no se ve: titulo, pagina y cuanto llevas
    if (ry + cabeza < s->cuerpo.top) {
        int fh = E(s, 40);
        // Tan ancha como la tarjeta: con las paginas ampliadas, la tarjeta se ensancha y la franja con ella
        int fx = tarjeta.left, fw = tarjeta.right - tarjeta.left;
        RECT franja = rect(fx, s->cuerpo.top, fw, fh);
        trozoDeFondo(s, dc, franja);
        velo(dc, franja, tm.panel, tm.oscuro ? 235 : 245);
        redondeado(dc, rect(fx, s->cuerpo.top + fh - 1, fw, 1), 0, tm.borde);
        redondeado(dc, rect(fx, s->cuerpo.top + fh - E(s, 3), std::max(E(s, 3), (int)(fw * fraccion)), E(s, 3)), 0, AZUL);
        std::wstring der = (paginaActual.empty() ? L"" : paginaActual + L"  ·  ") + std::to_wstring(porcentaje(fraccion)) + L" %";
        int dw = anchoTexto(dc, s->f.peque, der) + E(s, 10);
        texto(dc, s->f.peque, tm.textoSuave, der, rect(fx + fw - pad - dw, s->cuerpo.top, dw, fh - E(s, 3)),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        texto(dc, s->f.negrita, tm.texto, W(l["tema"]), rect(tx, s->cuerpo.top, tw - dw - E(s, 16), fh - E(s, 3)),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    // Paginas mas anchas que la pantalla: barra lateral que dice por donde se va
    if (limite > 0) {
        int pista = std::min(E(s, 240), visible / 3), px = (vi + vd) / 2 - pista / 2;
        int largo = std::max(E(s, 30), pista * visible / std::max(1, anchoHoja));
        int pos = (int)((pista - largo) * (s->lecturaDx + limite) / (2.0 * limite));
        int ty = s->cuerpo.bottom - E(s, 74);
        redondeado(dc, rect(px, ty, pista, E(s, 4)), E(s, 2), mezcla(tm.fondo, tm.textoSuave, 0.25));
        redondeado(dc, rect(px + pos, ty, largo, E(s, 4)), E(s, 2), mezcla(tm.fondo, tm.textoSuave, 0.7));
    }

    // Barra de abajo, seleccion y panel del profesor (fijos: no se mueven con el texto)
    int suelo = s->cuerpo.bottom - pintarBarraLectura(s, dc, centro, derecha, (int)hojas.size()) - E(s, 6);
    if (!s->lecturaMenuTest && (!s->lecturaSel.empty() || s->lecturaSelPidiendo) && !s->lecturaArrastre)
        pintarBarraSeleccion(s, dc, derecha, suelo);
    if (anchoPanel > 0) pintarPanel(s, dc, rect(derecha + E(s, 4), s->cuerpo.top + E(s, 12), anchoPanel - E(s, 16),
                                                 s->cuerpo.bottom - s->cuerpo.top - E(s, 24)));
    // Seleccionando mientras el texto se desplaza (solo, en un borde, o con la rueda): el final de la
    // seleccion sigue al raton con lo que se acaba de pintar
    if (s->lecturaArrastre && extenderSeleccion(s, s->lecturaRaton)) InvalidateRect(s->ventana, NULL, FALSE);

    if (saltando) {
        // Llevar el texto a la fraccion pedida (seguir donde se dejo, o tras cambiar letra o ancho)
        double objetivo = s->lecturaIrA;
        s->lecturaIrA = -1;
        int maximo = std::max(0, alto + E(s, 40) - s->altoVisible);
        int destino = s->scroll + (int)std::lround(objetivo * (py - textoArriba)) - (s->cuerpo.bottom - textoArriba);
        destino = std::max(0, std::min(maximo, destino));
        if (destino != s->scroll) {
            s->scroll = destino;
            InvalidateRect(s->ventana, NULL, FALSE);
        }
    } else if (std::fabs(fraccion - s->lecturaGuardada) >= 0.05 || (fraccion >= 0.999 && s->lecturaGuardada < 0.999)) {
        guardarLectura(s);
    }
    return alto;
}

// ---------------------------------------------------------------- raton

bool temarioRaton(Estado* s, UINT msg, POINT pt) {
    if (msg == WM_LBUTTONDOWN) {
        if (s->vista != V_TEMARIO || s->lectura.esNulo() || s->ocupado || hayCapa(s) || s->panelTest || !s->zoom.empty())
            return false;
        if (!PtInRect(&s->cuerpo, pt)) return false;
        for (auto it = s->zonas.rbegin(); it != s->zonas.rend(); ++it)
            if (it->accion != A_NADA && PtInRect(&it->r, pt)) return false;  // un boton: lo atiende la ventana
        for (const Zona& z : s->zonas)  // barras y paneles flotantes (zonas sin accion encima del texto)
            if (z.accion == A_NADA && PtInRect(&z.r, pt) && z.r.right - z.r.left < (s->cuerpo.right - s->cuerpo.left)) return true;
        bool habia = !s->lecturaSel.empty() || s->selHoja >= 0 || haySeleccionTexto(s);
        s->lecturaInicio = s->lecturaRaton = pt;
        if (vistaPdf(s)) {
            for (const auto& v : s->lecturaVisibles) {
                const RECT& r = v.second;
                if (!PtInRect(&r, pt)) continue;
                quitarSeleccion(s);
                s->selHoja = s->selHojaFin = v.first;
                double fx = (double)(pt.x - r.left) / std::max(1L, r.right - r.left);
                double fy = (double)(pt.y - r.top) / std::max(1L, r.bottom - r.top);
                s->selCaja[0] = s->selCaja[2] = fx;
                s->selCaja[1] = s->selCaja[3] = fy;
                s->lecturaArrastre = 2;
                SetCapture(s->ventana);
                InvalidateRect(s->ventana, NULL, FALSE);
                return true;
            }
        } else if (!s->lecturaPalabras.empty()) {
            const Estado::PalabraVista* p = palabraCercana(s, pt);
            const Estado::PalabraVista& primera = s->lecturaPalabras.front();
            const Estado::PalabraVista& ultima = s->lecturaPalabras.back();
            if (p && pt.x >= primera.r.left - E(s, 30) && pt.y >= primera.r.top - E(s, 30) && pt.y <= ultima.r.bottom + E(s, 30)) {
                quitarSeleccion(s);
                s->selDesdeP = s->selHastaP = p->parrafo;
                s->selDesdeW = s->selHastaW = p->palabra;
                s->lecturaArrastre = 1;
                SetCapture(s->ventana);
                InvalidateRect(s->ventana, NULL, FALSE);
                return true;
            }
        }
        if (habia) {  // clic fuera del texto: se quita la seleccion
            quitarSeleccion(s);
            InvalidateRect(s->ventana, NULL, FALSE);
        }
        return false;
    }
    if (!s->lecturaArrastre) return false;
    if (msg == WM_MOUSEMOVE) {
        s->lecturaRaton = pt;
        extenderSeleccion(s, pt);
        // Cerca de los bordes, el texto se desplaza solo (y sigue mientras el raton este ahi) para
        // poder seleccionar varias paginas
        if (!velocidadArrastre(s)) {
            KillTimer(s->ventana, TIMER_ARRASTRE);
            s->lecturaDesplazado = 0;
        } else if (!s->lecturaDesplazado) {
            s->lecturaDesplazado = GetTickCount64();
            SetTimer(s->ventana, TIMER_ARRASTRE, 30, NULL);
        }
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    if (msg == WM_LBUTTONUP) {
        int tipo = s->lecturaArrastre;
        s->lecturaRaton = pt;
        extenderSeleccion(s, pt);
        s->lecturaArrastre = 0;
        s->lecturaDesplazado = 0;
        KillTimer(s->ventana, TIMER_ARRASTRE);
        ReleaseCapture();
        bool clic = std::abs(pt.x - s->lecturaInicio.x) < E(s, 5) && std::abs(pt.y - s->lecturaInicio.y) < E(s, 5);
        if (tipo == 1) {
            if (clic && s->selDesdeP == s->selHastaP && s->selDesdeW == s->selHastaW) {
                quitarSeleccion(s);
            } else {
                s->lecturaSel = textoSeleccionado(s, s->lecturaSelDonde);
                if (s->lecturaSel.empty()) quitarSeleccion(s);
            }
        } else if (tipo == 2) {
            int a, b;
            hojasDeLaCaja(s, a, b);
            double desde[4], hasta[4];
            cajaEnHoja(s, a, desde);
            cajaEnHoja(s, b, hasta);
            bool pequeno = desde[2] - desde[0] < 0.02 || (a == b && desde[3] - desde[1] < 0.01);
            if (clic || pequeno) {
                quitarSeleccion(s);
            } else {
                // El motor saca el texto de cada pagina: de la primera, desde arriba del recuadro; de la
                // ultima, hasta abajo del recuadro; las de en medio, enteras (entre las mismas x)
                js::Valor p = js::Valor::objeto();
                p["ruta"] = s->lectura["ruta"].comoCadena();
                p["indice"] = a;
                p["hasta"] = b;
                js::Valor caja = js::Valor::array();
                for (double v : {desde[0], desde[1], desde[2], hasta[3]}) caja.agregar(js::Valor(v));
                p["caja"] = caja;
                if (enviar(s, "texto_region", p)) s->lecturaSelPidiendo = true;
                else quitarSeleccion(s);
            }
        }
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    return false;
}

void temarioTick(Estado* s) {
    int paso = s->lecturaArrastre ? velocidadArrastre(s) : 0;
    if (!paso) {
        KillTimer(s->ventana, TIMER_ARRASTRE);
        s->lecturaDesplazado = 0;
        return;
    }
    // `paso` es lo que se avanza cada 30 ms: se cuenta el tiempo de verdad, porque si pintar va lento
    // (la IA trabajando a la vez) los tics llegan mas espaciados
    ULONGLONG ahora = GetTickCount64();
    int ms = (int)std::min<ULONGLONG>(250, ahora - (s->lecturaDesplazado ? s->lecturaDesplazado : ahora));
    s->lecturaDesplazado = ahora;
    if (ms > 0) desplazar(s, paso * ms / 30);  // al pintar, el final de la seleccion se pone donde quede el raton
}

bool temarioRueda(Estado* s, int delta, bool lateral, int teclas) {
    if (s->vista != V_TEMARIO || hayCapa(s) || s->panelTest) return false;
    // Con el raton encima de la lista de documentos, la rueda mueve la lista y no el documento
    POINT raton;
    GetCursorPos(&raton);
    ScreenToClient(s->ventana, &raton);
    // (si cabe entera no hay nada que mover y la rueda sigue moviendo el documento)
    if (!lateral && !(teclas & MK_CONTROL) && s->lecturaListaMax > 0 && !IsRectEmpty(&s->lecturaListaRect) &&
        PtInRect(&s->lecturaListaRect, raton)) {
        int nuevo = std::max(0, std::min(s->lecturaListaMax, s->lecturaListaScroll - delta * E(s, 90) / WHEEL_DELTA));
        if (nuevo != s->lecturaListaScroll) {
            s->lecturaListaScroll = nuevo;
            InvalidateRect(s->ventana, NULL, FALSE);
        }
        return true;
    }
    if (s->lectura.esNulo()) return false;
    if (!lateral && (teclas & MK_CONTROL)) {  // Ctrl + rueda: ampliar o reducir
        cambiarLetra(s, delta > 0 ? 1 : -1);
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    if (lateral || (teclas & MK_SHIFT)) {  // Mayus + rueda (o la rueda lateral): de lado
        if (!vistaPdf(s) || s->lecturaZoom <= 100) return lateral;
        s->lecturaDx += (lateral ? delta : -delta) * E(s, 120) / WHEEL_DELTA;
        InvalidateRect(s->ventana, NULL, FALSE);
        return true;
    }
    return false;
}

LPCWSTR temarioCursor(Estado* s, POINT pt) {
    if (s->vista != V_TEMARIO || s->lectura.esNulo() || hayCapa(s) || s->panelTest || s->lecturaMenuTest) return NULL;
    if (s->lecturaArrastre == 2) return IDC_CROSS;
    if (s->lecturaArrastre == 1) return IDC_IBEAM;
    for (const Zona& z : s->zonas)
        if (z.accion == A_NADA && PtInRect(&z.r, pt) && z.r.right - z.r.left < (s->cuerpo.right - s->cuerpo.left)) return NULL;
    for (const auto& v : s->lecturaVisibles)
        if (PtInRect(&v.second, pt)) return IDC_CROSS;
    for (const Estado::PalabraVista& p : s->lecturaPalabras)
        if (pt.y >= p.r.top && pt.y < p.r.bottom && pt.x >= p.r.left - E(s, 4) && pt.x < p.r.right + E(s, 8)) return IDC_IBEAM;
    return NULL;
}

// ------------------------------------------------------------------ motor

bool temarioRespuesta(Estado* s, const std::string& orden, const js::Valor& r) {
    if (orden == "temario") {
        s->temario = r;
        if (r["tam"].comoEntero() > 0 && r["tam"].comoEntero() != s->lecturaTam) ponerTam(s, r["tam"].comoEntero());
        if (s->abrirAlLlegar == V_TEMARIO) {
            s->abrirAlLlegar = V_ARRANCANDO;
            irAVista(s, V_TEMARIO);
        }
        if (s->lecturaRuta.empty()) {
            // El ultimo que se estaba leyendo, si no se termino; si no, el primero sin leer (o el primero)
            const js::Valor& t = r["temas"];
            int ultimo = indiceDoc(s, r["ultimo"].comoCadena());
            if (ultimo >= 0 && !t[(std::size_t)ultimo]["leido"].comoBool() && legible(t[(std::size_t)ultimo]))
                s->lecturaRuta = t[(std::size_t)ultimo]["ruta"].comoCadena();
            std::vector<FilaTema> filas = filasPorTema(t);
            for (const FilaTema& f : filas) {
                if (!s->lecturaRuta.empty()) break;
                if (f.indice >= 0 && legible(t[(std::size_t)f.indice]) && !t[(std::size_t)f.indice]["leido"].comoBool())
                    s->lecturaRuta = t[(std::size_t)f.indice]["ruta"].comoCadena();
            }
            if (s->lecturaRuta.empty() && t.tam() > 0) s->lecturaRuta = t[0]["ruta"].comoCadena();
        }
        if (s->vista == V_TEMARIO && (s->lectura.esNulo() || s->lectura["ruta"].comoCadena() != s->lecturaRuta)) pedirDoc(s, s->lecturaRuta);
        return true;
    }
    if (orden == "pagina") {
        auto it = s->lecturaPeticiones.find(r["id"].comoEntero());
        if (it == s->lecturaPeticiones.end()) return true;  // de un documento que ya se cerro
        int i = it->second;
        s->lecturaPeticiones.erase(it);
        if (r["ruta"].comoCadena() != s->lectura["ruta"].comoCadena()) return true;
        int ancho = r["ancho"].comoEntero();
        auto pedida = s->lecturaPedida.find(i);
        if (pedida != s->lecturaPedida.end() && pedida->second <= ancho) s->lecturaPedida.erase(pedida);
        if (ancho >= s->lecturaJpgAncho[i]) {
            s->lecturaJpg[i] = W(r["imagen"]);
            s->lecturaJpgAncho[i] = ancho;
            soltarHoja(s, i);  // se vuelve a escalar desde la imagen nueva
        }
        return true;
    }
    if (orden == "texto_region") {
        int a, b;
        hojasDeLaCaja(s, a, b);
        if (!s->lecturaSelPidiendo || r["ruta"].comoCadena() != s->lectura["ruta"].comoCadena() || r["indice"].comoEntero() != a)
            return true;  // la seleccion ya cambio
        s->lecturaSelPidiendo = false;
        s->lecturaSel = W(r["texto"]);
        s->lecturaSelDonde = W(r["ubicacion"]);
        if (s->lecturaSel.size() < 3) {
            quitarSeleccion(s);
            aviso(s, L"En esa zona no hay texto (puede ser una imagen o un esquema)");
        }
        return true;
    }
    if (orden == "leer") {
        if (r["ruta"].comoCadena() != s->lecturaRuta) return true;  // llego tarde: ya se eligio otro
        soltarHojas(s);
        quitarSeleccion(s);
        s->lectura = r;
        s->lecturaAltos.clear();
        s->lecturaMarcada = r["leido"].comoBool();
        s->lecturaDx = 0;
        s->scroll = 0;
        double pos = r["posicion"].comoNumero();
        s->lecturaPos = s->lecturaGuardada = pos;
        s->lecturaIrA = -1;
        if (pos > 0.02 && pos < 0.98) {
            s->lecturaIrA = pos;
            aviso(s, L"Sigues donde lo dejaste (" + std::to_wstring(porcentaje(pos)) + L" %)");
        }
        return true;
    }
    if (orden == "posicion_lectura") {
        cambiarDoc(s, r["ruta"].comoCadena(), "posicion", js::Valor(r["posicion"].comoNumero()));
        s->temario["ultimo"] = r["ruta"].comoCadena();
        return true;
    }
    if (orden == "tam_lectura") return true;
    if (orden == "marcar_leido") {
        cambiarDoc(s, r["ruta"].comoCadena(), "leido", js::Valor(r["leido"].comoBool()));
        s->temario["leidos"] = r["leidos"].comoEntero();
        if (s->lectura["ruta"].comoCadena() == r["ruta"].comoCadena()) s->lectura["leido"] = r["leido"].comoBool();
        return true;
    }
    return false;
}

bool temarioError(Estado* s, int id, const std::string& orden) {
    if (orden == "texto_region") {
        if (s->lecturaSelPidiendo) {
            quitarSeleccion(s);
            aviso(s, L"No he podido leer el texto de esa zona");
        }
        return true;
    }
    if (orden != "pagina") return false;
    auto it = s->lecturaPeticiones.find(id);
    if (it != s->lecturaPeticiones.end()) {
        s->lecturaFallida[it->second] = 1;  // no se vuelve a pedir: se avisa en la propia hoja
        s->lecturaPedida.erase(it->second);
        s->lecturaPeticiones.erase(it);
    }
    return true;
}

// ---------------------------------------------------------------- acciones

bool temarioAccion(Estado* s, int accion, int dato) {
    switch (accion) {
        case A_L_DOC: {
            const js::Valor& d = s->temario["temas"][(std::size_t)dato];
            if (!legible(d)) {
                gui::abrirEnSistema(s->ventana, W(carpetaDeAsignatura(s->asignatura, true)) + L"\\" + W(d["ruta"]));
                return true;
            }
            pedirDoc(s, d["ruta"].comoCadena());
            return true;
        }
        case A_L_SIGUIENTE:
        case A_L_ANTERIOR: {
            int j = vecino(s, accion == A_L_SIGUIENTE ? 1 : -1);
            if (j >= 0) pedirDoc(s, s->temario["temas"][(std::size_t)j]["ruta"].comoCadena());
            return true;
        }
        case A_L_ABRIR:
            if (!gui::abrirEnSistema(s->ventana, W(s->lectura["archivo"]))) s->mensaje = L"No he podido abrir el documento.";
            return true;
        case A_L_COMPLETA:
            s->lecturaIrA = s->lecturaPos;  // el ancho cambia: seguir en el mismo sitio del texto
            alternarEnfoque(s);
            return true;
        case A_L_LEIDO: {
            bool nuevo = !s->lectura["leido"].comoBool();
            s->lecturaMarcada = true;  // a mano manda el alumno
            marcar(s, nuevo);
            return true;
        }
        case A_L_LISTA:
            s->lecturaIrA = s->lecturaPos;
            s->lecturaLista = !s->lecturaLista;
            return true;
        case A_L_LETRA: cambiarLetra(s, dato); return true;
        case A_L_AJUSTAR:  // el tamaño de inicio: páginas al 100 % o la letra de siempre
            if (vistaPdf(s)) {
                ponerZoom(s, 100);
                s->lecturaDx = 0;
            } else {
                ponerLetra(s, TAM_INICIAL);
            }
            return true;
        case A_L_PAGINA: irAPagina(s, dato); return true;
        case A_L_MODO:
            s->lecturaIrA = s->lecturaPos;  // seguir mas o menos por el mismo sitio
            s->lecturaPdf = !vistaPdf(s);
            quitarSeleccion(s);
            aviso(s, s->lecturaPdf ? L"Vista del PDF: las páginas tal cual, con sus figuras" : L"Vista de solo texto");
            return true;
        case A_L_SEGUIR: abrirTemario(s, s->temario["ultimo"].comoCadena()); return true;
        // --- tests
        case A_L_MENU_TEST: s->lecturaMenuTest = !s->lecturaMenuTest; return true;
        case A_L_CERRAR_MENU: s->lecturaMenuTest = false; return true;
        case A_L_TEST:
            empezarTestDeLectura(s, configParaEnviar(s), L"Preparando un test de «" + W(s->lectura["tema"]) + L"»…");
            return true;
        case A_L_TEST_RAPIDO: {
            js::Valor c = configParaEnviar(s);
            c["n"] = 5;
            c["modo"] = "rapido";
            empezarTestDeLectura(s, c, L"Preparando 5 preguntas de «" + W(s->lectura["tema"]) + L"»…");
            return true;
        }
        case A_L_TEST_PAGINA: {
            int j = paginaConTexto(s);
            if (j < 0) {
                aviso(s, L"Esta página no tiene texto del que sacar preguntas");
                return true;
            }
            const js::Valor& pags = s->lectura["paginas"];
            js::Valor paginas = js::Valor::array();
            paginas.agregar(pags[(std::size_t)j]["ubicacion"].comoCadena());
            if ((std::size_t)j + 1 < pags.tam()) paginas.agregar(pags[(std::size_t)j + 1]["ubicacion"].comoCadena());
            js::Valor c = configParaEnviar(s);
            c["paginas"] = paginas;
            c["n"] = 5;
            empezarTestDeLectura(s, c, L"Preparando preguntas de lo que estás leyendo…");
            return true;
        }
        case A_L_SIMULACRO: {
            js::Valor c = configParaEnviar(s);
            c["simulacro"] = true;
            c["n"] = 20;
            c["correccion"] = "final";
            c["penalizar"] = true;
            c["limite"] = 60;
            empezarTestDeLectura(s, c, L"Preparando un simulacro de «" + W(s->lectura["tema"]) + L"»…");
            return true;
        }
        case A_L_REPASO: {
            js::Valor c = configParaEnviar(s);
            c["repaso"] = true;
            c["modo"] = "rapido";
            empezarTestDeLectura(s, c, L"Reuniendo tus fallos de «" + W(s->lectura["tema"]) + L"»…");
            return true;
        }
        case A_L_DESARROLLO:
            s->lecturaMenuTest = false;
            s->desarrolloRuta = s->lecturaRuta;
            abrirVista(s, V_DESARROLLO);
            aviso(s, L"Pulsa «Nueva pregunta» para una de «" + W(s->lectura["tema"]) + L"»");
            return true;
        case A_L_RESUMEN: abrirResumenDe(s, s->lecturaRuta); return true;
        // --- preguntar al profesor
        case A_L_EXPLICAR: {
            if (!exigirIa(s)) return true;
            int j = paginaConTexto(s);
            if (j < 0) {
                aviso(s, L"Esta página no tiene texto que pueda leer: selecciona un trozo con el ratón");
                return true;
            }
            std::wstring t = W(s->lectura["paginas"][(std::size_t)j]["texto"]);
            if (t.size() > 1500) t = t.substr(0, 1500);
            std::wstring pag = s->lecturaPagina;
            if (!pag.empty()) pag[0] = (wchar_t)towupper(pag[0]);
            abrirPanel(s, pag + L" de «" + W(s->lectura["tema"]) + L"»",
                       gui::aUtf8(L"Texto de la " + s->lecturaPagina + L" de los apuntes «" + W(s->lectura["tema"]) + L"»:\n" + t),
                       L"Sobre la " + s->lecturaPagina + L" de «" + W(s->lectura["tema"]) + L"»");
            preguntar(s, L"Explícame esta página de mis apuntes: las ideas clave, con palabras sencillas y algún ejemplo.");
            return true;
        }
        case A_L_PREGUNTAR:
            if (!exigirIa(s)) return true;
            if (!s->lecturaSel.empty()) return temarioAccion(s, A_L_SEL, SEL_PREGUNTAR);
            abrirPanel(s, L"", "", L"");  // pregunta libre, sin trozo de contexto
            SetFocus(s->edit);
            return true;
        case A_L_SEL: {
            if (dato == SEL_CERRAR || s->lecturaSel.empty()) {
                quitarSeleccion(s);
                return true;
            }
            if (dato == SEL_COPIAR) {
                copiar(s->ventana, s->lecturaSel);
                aviso(s, L"Texto copiado");
                return true;
            }
            if (dato == SEL_BUSCAR) {
                std::wstring t = s->lecturaSel;
                for (wchar_t& c : t)
                    if (c == L'\n') c = L' ';
                quitarSeleccion(s);
                buscarEnTemario(s, t);
                return true;
            }
            if (dato == SEL_TEST) {
                if (s->lecturaSel.size() < SEL_MIN_TEST) {
                    aviso(s, L"Selecciona un trozo más largo (un par de frases) para sacar preguntas");
                    return true;
                }
                js::Valor c = configParaEnviar(s);
                c["texto"] = gui::aUtf8(s->lecturaSel);
                c["ubicacion"] = gui::aUtf8(s->lecturaSelDonde);
                c["n"] = 5;
                empezarTestDeLectura(s, c, L"Preparando preguntas de lo que has seleccionado…");
                quitarSeleccion(s);
                return true;
            }
            // Preguntar al profesor: el panel se abre con el trozo y las preguntas preparadas para elegir
            if (!exigirIa(s)) return true;
            abrirPanel(s, s->lecturaSel, contextoDeSeleccion(s), L"Sobre lo que has seleccionado");
            s->lecturaChatDesde = s->chat.size();  // trozo nuevo: conversacion nueva en el panel
            SetFocus(s->edit);
            quitarSeleccion(s);
            return true;
        }
        case A_L_SUGERIDA: {
            if (!exigirIa(s)) return true;
            std::vector<Sugerida> lista = sugeridas(s);
            if (dato < 0 || dato >= (int)lista.size()) return true;
            if (lista[(std::size_t)dato].pedido.empty()) return temarioAccion(s, A_L_EXPLICAR, 0);
            preguntar(s, lista[(std::size_t)dato].pedido);
            return true;
        }
        case A_L_RAPIDA: {
            if (!exigirIa(s)) return true;
            const wchar_t* dudas[] = {L"No lo he entendido. Explícamelo más fácil, paso a paso.",
                                      L"Ponme otro ejemplo, distinto, de la vida real.",
                                      L"Hazme una pregunta corta sobre esto para comprobar si lo he entendido. No me digas la respuesta todavía."};
            if (dato == 3) {
                s->lecturaPanel = false;
                abrirVista(s, V_CHAT);
            } else if (dato >= 0 && dato < 3) {
                preguntar(s, dudas[dato]);
            }
            return true;
        }
        case A_L_CERRAR_PANEL:
            s->lecturaPanel = false;
            s->lecturaIrA = s->lecturaPos;
            s->lecturaCampo = RECT();
            colocarEdit(s);
            SetFocus(s->ventana);
            return true;
        default: return false;
    }
}

// Teclado del lector: Espacio avanza una pantalla (Mayús+Espacio retrocede), Inicio/Fin, ← y →
// cambian de documento, AvPág/RePág saltan a la página siguiente o anterior, + y - cambian la letra
// (o el tamaño de las páginas), 0 lo deja como al principio,
// T alterna entre el PDF y solo el texto, L muestra u oculta la lista y P abre el panel para
// preguntar. Ctrl+C copia lo seleccionado; Esc quita la selección, el menú o el panel.
bool temarioTecla(Estado* s, WPARAM tecla) {
    if (s->lectura.esNulo()) return false;
    if (tecla == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) && !s->lecturaSel.empty()) {
        ejecutar(s, A_L_SEL, 4);
        return true;
    }
    if (tecla == VK_ESCAPE) {
        if (s->lecturaMenuTest) ejecutar(s, A_L_CERRAR_MENU, 0);
        else if (!s->lecturaSel.empty() || s->lecturaSelPidiendo || s->selHoja >= 0 || haySeleccionTexto(s)) ejecutar(s, A_L_SEL, 5);
        else if (s->lecturaPanel) ejecutar(s, A_L_CERRAR_PANEL, 0);
        else return false;
        return true;
    }
    if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000)) return false;
    bool mayus = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    switch (tecla) {
        case VK_SPACE: desplazar(s, (mayus ? -1 : 1) * s->altoVisible * 85 / 100); return true;
        case VK_HOME: desplazar(s, -s->scroll); return true;
        case VK_NEXT: irAPagina(s, 1); return true;    // AvPág: principio de la página siguiente
        case VK_PRIOR: irAPagina(s, -1); return true;  // RePág: principio de la anterior
        case VK_END: desplazar(s, s->altoContenido); return true;
        case VK_RIGHT: ejecutar(s, A_L_SIGUIENTE, 0); return true;
        case VK_LEFT: ejecutar(s, A_L_ANTERIOR, 0); return true;
        case VK_OEM_PLUS:
        case VK_ADD: ejecutar(s, A_L_LETRA, 1); return true;
        case VK_OEM_MINUS:
        case VK_SUBTRACT: ejecutar(s, A_L_LETRA, -1); return true;
        case '0':
        case VK_NUMPAD0:
            ejecutar(s, A_L_AJUSTAR, 0);
            return true;
        case 'P': ejecutar(s, A_L_PREGUNTAR, 0); return true;
        case 'T':
            if (!(s->lectura["visual"].comoBool() && s->lectura["paginas"].tam() > 0)) return false;
            ejecutar(s, A_L_MODO, 0);
            return true;
        case 'L':
            if (s->enfoque) return false;
            ejecutar(s, A_L_LISTA, 0);
            return true;
        default: return false;
    }
}

}  // namespace ui
}  // namespace profesor
