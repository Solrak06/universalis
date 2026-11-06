// ProfesorUi.hpp - Piezas internas de la ventana del profesor, compartidas por
// GuiProfesor.cpp (ventana, navegacion, tests, progreso, dudas) y
// GuiProfesorEstudio.cpp (inicio, tarjetas, resumenes, desarrollo, texto con formato).
//
// No es una interfaz publica: la unica entrada al profesor es profesor::abrir().
#pragma once

#include "Gui.hpp"

#include <gdiplus.h>

#include <map>
#include <string>
#include <vector>

#include "Json.hpp"
#include "Modelo.hpp"
#include "ProfesorMotor.hpp"

namespace profesor {
namespace ui {

const UINT WM_MOTOR = WM_APP + 40;
const int ID_CHAT = 3001;
const int ID_RESPUESTA = 3002;
const int ID_BUSQUEDA = 3003;
const UINT_PTR TIMER_ANIMACION = 1;
const UINT_PTR TIMER_RELOJ = 2;
const UINT_PTR TIMER_BUSQUEDA = 3;  // espera a que dejes de teclear para buscar
const UINT_PTR TIMER_POMODORO = 4;
const UINT_PTR TIMER_ENFOQUE = 5;   // reloj de la capsula del modo enfoque
const UINT_PTR TIMER_ARRASTRE = 6;  // lector: desplaza el texto mientras se selecciona cerca de un borde

// Las vistas se usan como dato de las zonas (A_PESTANA, A_NAV): deben ser < 100.
enum Vista {
    V_ARRANCANDO, V_INICIO, V_PREGUNTA, V_RESULTADO, V_FALLOS, V_PROGRESO, V_CHAT, V_ERROR,
    V_HUB, V_TARJETAS, V_RESUMENES, V_DESARROLLO, V_BUSCAR, V_TEMARIO
};

enum Accion {
    A_NADA, A_MODO, A_CANTIDAD, A_DIFICULTAD, A_TIPO, A_CORRECCION, A_PENALIZAR, A_REVISION, A_AUTOGUARDAR,
    A_MODELO, A_TEMA, A_TODOS, A_EMPEZAR, A_CONTINUAR, A_OPCION, A_COMPROBAR, A_SIGUIENTE, A_ANTERIOR, A_IR,
    A_TERMINAR, A_ABRIR_FUENTE, A_DESCARTAR, A_GUARDAR, A_DUDA_PREGUNTA, A_VER, A_RELLENAR, A_ARRANCAR_IA,
    A_ENVIAR, A_SUGERENCIA, A_QUITAR_CONTEXTO, A_FUENTE, A_CANCELAR_DUDA, A_PRACTICAR_TEMA, A_REINTENTAR,
    A_ABRIR_TEMARIO, A_ZOOM, A_CERRAR_ZOOM,
    // navegacion y ventana
    A_PESTANA, A_ATRAS, A_PANTALLA_COMPLETA, A_MINIMIZAR, A_MAXIMIZAR, A_CERRAR,
    // mis fallos
    A_N_FALLOS, A_TEST_FALLOS, A_QUITAR_FALLO, A_QUITAR_ACERTADAS,
    A_LIMITE,
    // inicio
    A_PLAN, A_SIMULACRO, A_REPASO, A_EXPLICAR_FALLO,
    // tarjetas
    A_T_EMPEZAR, A_T_CANTIDAD, A_T_VOLTEAR, A_T_NOTA, A_T_TRUCO, A_T_DUDA, A_T_EXTRA, A_T_TERMINAR, A_T_ABRIR,
    A_T_ZOOM,
    // resumenes
    A_R_TEMA, A_R_GENERAR, A_R_TODOS, A_R_DETENER, A_R_TEST, A_R_ABRIR, A_R_COPIAR, A_R_CANCELAR, A_R_TARJETAS,
    // desarrollo
    A_D_TEMA, A_D_NUEVA, A_D_ENTREGAR, A_D_RENDIRSE, A_D_OTRA, A_D_REPETIR, A_D_CANCELAR, A_D_DUDA, A_D_ABRIR,
    A_D_DESCARTAR, A_D_SIN_IA, A_D_DIFICULTAD, A_D_VOLVER,
    // progreso y dudas
    A_ANALIZAR, A_ANALISIS_CANCELAR, A_NUEVA_CONVERSACION, A_COPIAR, A_SEGUIMIENTO,
    // buscador, mapa, calendario, pomodoro, logros, exportar, cambio de asignatura
    A_B_MODO, A_B_RESULTADO, A_B_CONCEPTO, A_B_LETRA, A_R_VISTA, A_R_MAPA, A_CAL_DIA, A_POMODORO, A_POMODORO_PARAR,
    A_EXPORTAR, A_SELECTOR, A_ASIGNATURA, A_CERRAR_SELECTOR,
    // temas agrupados
    A_TEMA_GRUPO, A_TEMAS_IA,
    // modo enfoque y musica
    A_ENFOQUE, A_MUSICA, A_MUSICA_SONIDO, A_MUSICA_VOLUMEN, A_MUSICA_CARPETA, A_MUSICA_SIGUIENTE, A_MUSICA_CERRAR,
    A_ENFOQUE_POMODORO,
    // paleta de comandos, ayuda de atajos y pantalla de descanso
    A_COMANDOS, A_COMANDO, A_CERRAR_COMANDOS, A_ATAJOS, A_CERRAR_ATAJOS, A_DESCANSO, A_DESCANSO_CERRAR,
    A_DESCANSO_ESTUDIAR, A_BLOQUEO,
    // lector del temario
    A_L_DOC, A_L_ABRIR, A_L_COMPLETA, A_L_LEIDO, A_L_SIGUIENTE, A_L_ANTERIOR, A_L_TEST, A_L_LISTA, A_L_LETRA, A_L_SEGUIR,
    A_L_MODO,
    // formatos de pregunta del test (definicion, completar, caso practico...)
    A_FORMATO, A_RESERVA, A_REPETIR_TEST, A_DUDOSA,
    // panel «Hacer un test» (modo enfoque y Ctrl+T)
    A_PANEL_TEST, A_CERRAR_PANEL_TEST, A_PT_RAPIDO, A_PT_TEMA, A_PT_FALLOS, A_PT_SIMULACRO, A_PT_CONFIGURAR, A_PT_SEGUIR,
    A_PT_CANTIDAD, A_PT_MODO, A_PT_REPASO,
    // lector: seleccion, barra de abajo, menu de tests y panel del profesor
    A_L_SEL, A_L_MENU_TEST, A_L_CERRAR_MENU, A_L_TEST_PAGINA, A_L_TEST_RAPIDO, A_L_SIMULACRO, A_L_REPASO,
    A_L_DESARROLLO, A_L_RESUMEN, A_L_EXPLICAR, A_L_PREGUNTAR, A_L_AJUSTAR, A_L_CERRAR_PANEL, A_L_RAPIDA,
    A_L_SUGERIDA, A_L_PAGINA
};

struct Zona {
    RECT r;
    int accion;
    int dato;
};

struct Fuentes {
    HFONT titulo = NULL, subtitulo = NULL, pregunta = NULL, texto = NULL, negrita = NULL, peque = NULL,
          grande = NULL, mini = NULL, iniciales = NULL, tarjeta = NULL, enorme = NULL,
          lectura = NULL;  // texto del temario, algo mas grande para leer comodo
};

struct Mensaje {
    bool alumno = false;
    std::wstring texto;
    std::wstring contexto;   // "Sobre la pregunta 3" (solo en los del alumno)
    js::Valor fuentes;
    bool enCurso = false;
};

// Una tarea de la IA que se esta escribiendo (resumen, pregunta, correccion, analisis, truco).
struct TareaIA {
    int id = 0;               // id de la peticion (0 = ninguna en curso)
    std::wstring parcial;     // texto recibido hasta ahora
    int caracteres = 0;       // para las que devuelven JSON: progreso aproximado
    ULONGLONG inicio = 0;
    bool activa() const { return id != 0; }
};

struct Estado {
    HWND ventana = NULL;
    HWND edit = NULL;        // campo del chat de dudas
    HWND respuesta = NULL;   // campo de las preguntas de desarrollo
    Asignatura asignatura;
    Motor motor;
    Gdiplus::Image* retrato = NULL;
    Gdiplus::Bitmap* retratoCache = NULL;  // retrato escalado a la portada de Inicio
    Gdiplus::Bitmap* retratoPeque = NULL;  // copia reducida para los avatares
    int retratoAncho = 0, retratoAlto = 0;
    std::map<std::wstring, Gdiplus::Image*> imagenes;  // figuras de las preguntas
    Fuentes f;
    double factor = 1.0;  // ppp del monitor * escala de la interfaz
    HBITMAP fondo = NULL;  // fondo con halos de color, se rehace al cambiar de tamano
    int fondoAncho = 0, fondoAlto = 0;
    HBITMAP marco = NULL;     // ultimo fotograma pintado (doble bufer reutilizable)
    HBITMAP anterior = NULL;  // fotograma de la pantalla que se va (fundido)
    int marcoAncho = 0, marcoAlto = 0;
    ULONGLONG transicion = 0;
    bool entregando = false;  // el tiempo se acabo y el test se esta entregando

    // Ventana
    bool pantallaCompleta = false;
    WINDOWPLACEMENT colocacion = WINDOWPLACEMENT();

    // Navegacion
    Vista vista = V_ARRANCANDO;
    Vista abrirAlLlegar = V_ARRANCANDO;  // vista que se abre cuando llegue su respuesta del motor
    std::vector<Vista> historial;
    int scroll = 0, altoContenido = 0, altoVisible = 0;
    RECT cuerpo = RECT();  // zona del contenido (entre la barra de titulo y el pie)
    // Barra de desplazamiento de la derecha: se puede arrastrar o pulsar en su carril
    // (y la de la lista del Temario, con su propio desplazamiento)
    RECT barraCarril = RECT(), barraPulgar = RECT();  // vacios si todo cabe
    RECT listaCarril = RECT(), listaPulgar = RECT();
    int barraArrastre = 0;     // 0 nada, 1 la barra de la pagina, 2 la de la lista del Temario
    int barraAgarre = 0;       // distancia del raton al borde de arriba del pulgar al empezar a arrastrar
    int barraEncima = 0;       // la barra que esta bajo el raton (se ensancha)
    std::vector<Zona> zonas;
    int hoverAccion = A_NADA, hoverDato = -1;
    bool cerrando = false;
    std::wstring zoom;  // imagen ampliada (vacio = ninguna)
    std::wstring zoomPie;

    // Datos recibidos del motor
    js::Valor hola, config, sesion, pregunta, resultado, progreso, generacion, fallos;
    js::Valor reserva;  // preguntas de IA que se preparan en segundo plano (ultimo evento «reserva»)
    std::vector<bool> temas;  // seleccion de documentos del Temario
    int seleccion = -1;
    bool dudosa = false;      // «No estoy seguro» marcado en la pregunta actual
    bool panelTest = false;   // panel «Hacer un test» abierto
    int nFallos = 0;  // tamano del test de mis fallos (0 = todas)
    ULONGLONG relojBase = 0;
    int segundosBase = 0;

    std::wstring mensaje;   // bocadillo del profesor
    std::wstring cargando;  // texto mientras se espera al motor
    std::wstring error;
    std::string iaFase;     // "", "cargando", "lista"
    std::map<int, std::string> peticiones;  // id -> orden
    bool ocupado = false;
    int animacion = 0;
    bool irACorreccion = false;
    int finCorreccion = 0;
    // Animaciones del test: entrada de cada pregunta, respuesta corregida y nota final (0 = ninguna)
    ULONGLONG animPregunta = 0, animCorreccion = 0, animResultado = 0;

    // Chat de dudas
    std::vector<Mensaje> chat;
    bool chatEnCurso = false;
    ULONGLONG chatDesde = 0;  // cuando se envio la duda en curso (para el cronometro de la espera)
    bool chatAlFinal = false;
    int contextoIndice = -1;     // pregunta del test a la que se refiere la duda (-1 = ninguna)
    std::string contextoPregunta;  // o id de una pregunta de «Mis fallos» / texto de una tarjeta
    std::wstring contextoEtiqueta; // como se muestra ese contexto ("Sobre una tarjeta")

    // Inicio
    js::Valor inicio;  // plan del dia, proximo examen y cifras de las herramientas

    // Tarjetas
    js::Valor tarjetas, mazo;  // sesion en curso y estado del mazo
    int tarjeta = 0;           // indice de la tarjeta actual
    bool volteada = false;
    ULONGLONG volteo = 0;      // inicio de la animacion de giro
    int nTarjetas = 20;
    int tarjetasBien = 0, tarjetasOtra = 0;
    bool tarjetasFin = false;
    std::map<std::string, std::wstring> trucos;  // id de tarjeta -> truco
    std::string trucoTarjeta;
    TareaIA truco;

    // Resumenes
    js::Valor resumenes;       // lista de temas con su estado
    std::string resumenRuta;   // tema elegido
    js::Valor resumen;         // resumen mostrado
    TareaIA tareaResumen;
    js::Valor resumenesFondo;  // progreso de «resumir todos» (nulo si no trabaja)

    // Desarrollo
    js::Valor desarrollo;      // temas e intentos
    std::string desarrolloRuta;  // "" = automatico
    std::string desarrolloDificultad = "Intermedia";
    js::Valor abierta, correccion;
    TareaIA tareaAbierta, tareaCorreccion;
    std::wstring respuestaEnviada;
    RECT rectRespuesta = RECT();  // donde va el campo de la respuesta (vacio = oculto)

    // Analisis del progreso
    js::Valor analisis;
    TareaIA tareaAnalisis;

    // Buscador y glosario
    HWND busqueda = NULL;      // campo de busqueda
    RECT rectBusqueda = RECT();
    int modoBusqueda = 0;      // 0 = buscar en los apuntes, 1 = glosario
    js::Valor resultados, glosario;
    std::wstring letra;        // filtro del glosario por inicial ("" = todas)

    // Mapa mental (en Resumenes)
    bool verMapa = false;
    js::Valor mapa;
    TareaIA tareaMapa;

    // Temporizador de estudio (pomodoro)
    int pomodoro = 0;          // 0 = parado, 1 = estudiando, 2 = descanso
    bool pomodoroPausa = false;
    ULONGLONG pomodoroInicio = 0, pomodoroPausadoEn = 0;
    int pomodoroDuracion = 0;  // segundos

    // Logros y avisos
    js::Valor logros;
    std::wstring toast;        // aviso flotante (logro conseguido, exportado...)
    ULONGLONG toastHasta = 0;

    // Cambio de asignatura sin salir del profesor
    std::vector<Asignatura> todas;
    bool selector = false;

    // La IA esta poniendo nombre a los temas ("" = no; "trabajando")
    std::string temasIa;

    // Modo enfoque (pantalla completa sin distracciones) y musica de fondo
    bool enfoque = false;
    bool completaAntesDeEnfoque = false;  // ya estaba en pantalla completa al entrar
    bool panelMusica = false;
    ULONGLONG movido = 0;                 // ultimo movimiento del raton (la capsula se atenua sola)

    // Paleta de comandos (Ctrl+K): ir a cualquier pantalla, tema o accion escribiendo
    bool comandos = false;
    std::wstring comandosTexto;
    int comandosSel = 0;
    int comandosPrimero = 0;              // primera fila visible de la lista
    ULONGLONG comandosAbierta = 0;
    bool atajos = false;                  // ayuda con los atajos de teclado (F1)

    // Pantalla de descanso con respiracion guiada
    bool pantallaDescanso = false;
    ULONGLONG descansoInicio = 0;

    // Lector del temario
    js::Valor temario;              // documentos por temas (y si ya se leyeron)
    js::Valor lectura;              // documento abierto, pagina a pagina
    std::string lecturaRuta;
    bool lecturaMarcada = false;    // ya se aviso al motor de que se leyo hasta el final
    bool lecturaLista = true;       // ver la lista de documentos junto al texto
    int lecturaListaScroll = 0;     // la lista va fija a la izquierda y se desplaza por su cuenta
    int lecturaListaMax = 0;        // lo que puede bajar la lista (0 = cabe entera)
    RECT lecturaListaRect = RECT(); // donde se ve la lista (la rueda encima la mueve a ella)
    std::string lecturaListaRuta;   // documento que la lista ya ha puesto a la vista
    std::vector<int> lecturaAltos;  // alto de cada parrafo (cache por ancho y letra)
    int lecturaAncho = 0;
    int lecturaTam = 17;            // tamaño de la letra de lectura (px a 96 ppp)
    double lecturaPos = 0;          // fraccion del texto que ya se ha visto
    double lecturaGuardada = 0;     // la ultima posicion que se guardo en el motor
    double lecturaIrA = -1;         // al pintar, llevar el texto a esta fraccion (>= 0)
    // Vista «como el PDF»: cada pagina pintada como imagen por el motor y escalada aqui una vez
    bool lecturaPdf = true;                          // ver las paginas tal cual (false = solo el texto)
    int lecturaZoom = 100;                           // ancho de la pagina, en % de la columna
    std::map<int, std::wstring> lecturaJpg;          // pagina -> imagen recibida del motor
    std::map<int, int> lecturaJpgAncho;              // pagina -> ancho (px) de esa imagen
    std::map<int, int> lecturaPedida;                // pagina -> ancho pedido y aun sin respuesta
    std::map<int, int> lecturaPeticiones;            // id de la peticion -> pagina
    std::map<int, int> lecturaFallida;               // paginas que el motor no pudo pintar
    std::map<int, Gdiplus::Bitmap*> lecturaHojas;    // pagina -> imagen ya escalada a su ancho en pantalla
    int lecturaDx = 0;                               // paginas ampliadas mas que la columna: desplazamiento lateral
    std::vector<int> lecturaInicios;                 // donde empieza cada pagina (y del contenido, sin desplazar)
    std::wstring lecturaPagina;                      // pagina que se esta leyendo («página 3»)
    // Seleccion para preguntar al profesor: en el texto, de una palabra a otra; en el PDF, un recuadro
    struct PalabraVista {
        RECT r;
        int parrafo, palabra;
    };
    std::vector<PalabraVista> lecturaPalabras;       // palabras visibles en el ultimo pintado
    std::vector<std::pair<int, RECT> > lecturaVisibles;  // paginas del PDF visibles (indice, sitio)
    int lecturaArrastre = 0;                         // 0 nada, 1 texto, 2 recuadro del PDF
    POINT lecturaInicio = POINT();                   // donde se pulso (para distinguir clic de arrastre)
    int selDesdeP = -1, selDesdeW = 0, selHastaP = -1, selHastaW = 0;  // parrafo y palabra
    int selHoja = -1, selHojaFin = -1;               // PDF: pagina donde empieza el recuadro y donde acaba
    double selCaja[4] = {0, 0, 0, 0};                // x, y del inicio (en selHoja) y del final (en selHojaFin), en fracciones
    POINT lecturaRaton = POINT();                    // donde esta el raton mientras se selecciona
    ULONGLONG lecturaDesplazado = 0;                 // ultimo paso del desplazamiento solo al seleccionar (0 = parado)
    std::wstring lecturaSel;                         // texto elegido (vacio = sin seleccion lista)
    std::wstring lecturaSelDonde;                    // «página 3»
    bool lecturaSelPidiendo = false;                 // esperando al motor el texto del recuadro
    RECT lecturaSelRect = RECT();                    // donde se ve la seleccion (para la barra flotante)
    bool lecturaMenuTest = false;                    // menu «Test» de la barra de abajo
    bool lecturaPanel = false;                       // panel lateral con las respuestas del profesor
    std::size_t lecturaChatDesde = 0;                // mensajes del chat que pertenecen al panel
    std::wstring lecturaPanelCita;                   // de que se esta hablando en el panel
    RECT lecturaCampo = RECT();                      // donde va el campo de escribir del panel
};

// ================================================================ utilidades
std::wstring W(const std::string& s);
std::wstring W(const js::Valor& v);
int E(Estado* s, int px);
Gdiplus::Color gc(COLORREF c, int alfa = 255);
COLORREF mezcla(COLORREF a, COLORREF b, double t);
RECT rect(int x, int y, int w, int h);
std::wstring numero(double v);
std::wstring tiempo(int segundos);
// "1 tarjeta" / "3 tarjetas"
inline std::wstring plural(int n, const wchar_t* uno, const wchar_t* varios) {
    return std::to_wstring(n) + L" " + (n == 1 ? uno : varios);
}
std::wstring calificacion(double nota);
std::wstring unaLinea(std::wstring t);

// ================================================================ dibujo
void caminoRedondeado(Gdiplus::GraphicsPath& p, const RECT& r, int radio);
void redondeado(HDC dc, const RECT& r, int radio, COLORREF fondo, COLORREF borde = CLR_INVALID, int grosor = 1);
void cristal(HDC dc, const RECT& r, int radio, COLORREF base = CLR_INVALID, COLORREF borde = CLR_INVALID,
             int grosor = 1);
void degradado(HDC dc, const RECT& r, int radio, COLORREF arriba, COLORREF abajo);
void velo(HDC dc, const RECT& r, COLORREF c, int alfa);
void contorno(HDC dc, const RECT& r, int radio, COLORREF color, float grosor = 1);
void circulo(HDC dc, int cx, int cy, int radio, COLORREF fondo, COLORREF borde = CLR_INVALID, float grosor = 1);
void arco(HDC dc, int cx, int cy, int radio, float grosor, float inicio, float barrido, COLORREF color);
void linea(HDC dc, float x1, float y1, float x2, float y2, COLORREF color, float grosor);
void texto(HDC dc, HFONT f, COLORREF c, const std::wstring& t, RECT r,
           UINT flags = DT_LEFT | DT_TOP | DT_WORDBREAK);
int altoTexto(HDC dc, HFONT f, const std::wstring& t, int ancho);
int anchoTexto(HDC dc, HFONT f, const std::wstring& t);
void imagenAjustada(Gdiplus::Image* im, HDC dc, RECT r);
Gdiplus::Image* imagen(Estado* s, const std::wstring& ruta);

// Iconos lineales dibujados a mano (navegacion, botones, herramientas).
enum Icono {
    I_INICIO, I_TEST, I_JUGAR, I_TARJETAS, I_RESUMEN, I_DESARROLLO, I_DUDAS, I_PROGRESO, I_FALLOS, I_RESULTADOS,
    I_RELOJ, I_BOMBILLA, I_COPIAR, I_CHISPA, I_LIBRO, I_FLECHA, I_REPETIR, I_CARPETA, I_CHECK, I_DIANA,
    I_BUSCAR, I_MAPA, I_TROFEO, I_EXPORTAR, I_CALENDARIO, I_FUEGO, I_PAUSA,
    I_AMPLIAR  // solo con glifo(): el dibujo «ampliar» (si falta, la lupa)
};
void icono(Estado* s, HDC dc, Icono i, int cx, int cy, int tam, COLORREF c);
// Para los iconos destacados (cuadros de color, menus, barras flotantes): el dibujo macizo de
// assets/iconos si existe (test, preguntar, explicar...) y, si no, el lineal. `nombre` fuerza uno.
void glifo(Estado* s, HDC dc, Icono i, int cx, int cy, int tam, COLORREF c, const wchar_t* nombre = NULL);

// ============================================================ componentes
enum EstiloBoton { PRIMARIO, SECUNDARIO, FANTASMA, PELIGRO };

void zona(Estado* s, RECT r, int accion, int dato = 0);
bool encima(Estado* s, int accion, int dato = 0);
void boton(Estado* s, HDC dc, RECT r, const std::wstring& t, int accion, int dato = 0, EstiloBoton estilo = SECUNDARIO,
           bool activo = true);
// Boton con un icono delante del texto.
void botonIcono(Estado* s, HDC dc, RECT r, Icono i, const std::wstring& t, int accion, int dato = 0,
                EstiloBoton estilo = SECUNDARIO, bool activo = true);
int chip(Estado* s, HDC dc, int x, int y, const std::wstring& t, bool elegido, int accion, int dato);
int etiqueta(Estado* s, HDC dc, int x, int y, const std::wstring& t, COLORREF c);
void interruptor(Estado* s, HDC dc, int x, int y, bool activo, const std::wstring& t, int accion);
void casilla(Estado* s, HDC dc, RECT r, bool marcada);
void marca(HDC dc, int cx, int cy, int radio, bool bien);
void spinner(Estado* s, HDC dc, int cx, int cy, int radio);
void barra(HDC dc, RECT r, double fraccion, COLORREF color);
// Con icono (un Icono), se dibuja tenue en la esquina: dice de que va la cifra sin tener que leer.
void cifra(Estado* s, HDC dc, RECT r, const std::wstring& valor, const std::wstring& nombre, COLORREF color,
           int icono = -1);
void tituloSeccion(Estado* s, HDC dc, int x, int& y, int w, const std::wstring& t, const std::wstring& sub = L"");
void avatar(Estado* s, HDC dc, int cx, int cy, int radio);
void tarjetaImagen(Estado* s, HDC dc, RECT r, const js::Valor& p);
// Retrato del profesor recortado dentro de un rectangulo redondeado (lo mas abajo posible).
void retratoEn(Estado* s, HDC dc, RECT r, int radio);
// Markdown ligero (## titulo, - vinetas, 1. listas, **negrita**, [1] citas). Devuelve el alto ocupado;
// con dibujar=false solo mide.
int textoRico(Estado* s, HDC dc, const std::wstring& md, int x, int y, int w, bool dibujar = true,
              COLORREF tinta = CLR_INVALID);
// Cabecera de pantalla: icono en un cuadrado de color, titulo y subtitulo. Devuelve el alto ocupado.
int cabecera(Estado* s, HDC dc, int x, int y, int w, Icono i, COLORREF c, const std::wstring& titulo,
             const std::wstring& sub);
// Listas por temas: el motor manda en cada elemento "grupo" («Tema 1 · Introducción»), "grupo_orden" y
// "orden". filasPorTema devuelve las filas a pintar en orden, con un subtitulo (indice -1) delante de cada
// tema. Si todo es de un solo tema no hay subtitulos.
struct FilaTema {
    int indice;            // elemento de la lista, o -1 si es el subtitulo de un tema
    std::wstring grupo;    // tema al que pertenece (o que encabeza)
    int miembros;          // en los subtitulos: cuantos apartados tiene el tema
    int grupo_n;           // numero de tema en la lista (0, 1, 2...)
};
std::vector<FilaTema> filasPorTema(const js::Valor& lista);
bool hayTemas(const std::vector<FilaTema>& filas);
// Subtitulo de un tema: «TEMA 1» en una pastilla y el titulo. Con `marcado` (0 nada, 1 parte, 2 todo)
// dibuja una casilla delante. Devuelve el alto usado.
void subtituloTema(Estado* s, HDC dc, RECT r, const std::wstring& grupo, const std::wstring& info, COLORREF c,
                   int marcado = -1);
// Rehace la lista con los temas nuevos ({ruta: {grupo, grupo_orden, orden}}) sin cambiar su orden.
void aplicarGrupos(js::Valor& lista, const js::Valor& grupos);
// Copia un texto al portapapeles.
bool copiar(HWND ventana, const std::wstring& t);
// Quita el Markdown ligero (para copiar al portapapeles).
std::wstring sinFormato(const std::wstring& md);

// ============================================ ventana (tambien las usan los Ajustes)
void crearFuentes(Estado* s);
void borrarFuentes(Estado* s);
void prepararFondo(Estado* s, HDC dc, int w, int h);  // degradado con halos, uno por tamano
void trozoDeFondo(Estado* s, HDC dc, const RECT& r);
void iconoVentana(Estado* s, HDC dc, RECT r, int accion, COLORREF c);  // A_MINIMIZAR, A_MAXIMIZAR, A_CERRAR

// =============================================================== motor
int enviar(Estado* s, const std::string& orden, js::Valor peticion = js::Valor::objeto(), bool bloquea = false,
           const std::wstring& espera = L"");
std::string cfg(Estado* s, const char* clave, const char* defecto = "");
js::Valor configParaEnviar(Estado* s);
bool hayTest(Estado* s);
bool testTerminado(Estado* s);
bool iaDisponible(Estado* s);
std::wstring servidorIa(Estado* s);  // como se llama la IA conectada («Ollama», «LM Studio»…), ver CONECTAR_IA.md
bool iaExterna(Estado* s);           // la IA es un servidor externo (compatible con OpenAI), no Ollama
// Para lo que solo puede hacer la IA (responder dudas): si no esta, lo avisa, vuelve a mirar si se ha
// encendido y devuelve false.
bool exigirIa(Estado* s);
void irAVista(Estado* s, Vista v, bool recordar = true);
void abrirVista(Estado* s, Vista v);
void pedirPregunta(Estado* s, int indice);
void empezar(Estado* s, bool deFallos, int soloTema = -1);
void empezarCon(Estado* s, js::Valor config, const std::wstring& espera);
void enviarDuda(Estado* s, const std::wstring& duda);
const Examen* proximaPrueba(Estado* s, long& dias);
void colocarEdit(Estado* s);

// ========================================== pantallas de GuiProfesorEstudio.cpp
struct Area {
    int x, y, w;  // origen y ancho del contenido (y ya incluye el desplazamiento)
};

int pintarHub(Estado* s, HDC dc, Area a);
int pintarTarjetas(Estado* s, HDC dc, Area a);
void pieTarjetas(Estado* s, HDC dc, RECT pie);
int pintarResumenes(Estado* s, HDC dc, Area a);
void pieResumenes(Estado* s, HDC dc, RECT pie);
int pintarDesarrollo(Estado* s, HDC dc, Area a);
void pieDesarrollo(Estado* s, HDC dc, RECT pie);
int pintarAnalisis(Estado* s, HDC dc, int x, int y, int w);  // tarjeta dentro de Progreso
bool tienePie(Estado* s, Vista v);

// Devuelven true si han atendido el mensaje.
bool estudioRespuesta(Estado* s, const std::string& orden, const js::Valor& r);
bool estudioError(Estado* s, const std::string& orden, const std::wstring& mensaje);
bool estudioEvento(Estado* s, const js::Valor& ev);
bool estudioAccion(Estado* s, int accion, int dato);
bool estudioTecla(Estado* s, WPARAM tecla);
void abrirEstudio(Estado* s, Vista v);  // pide al motor lo que necesita cada pantalla nueva
bool animandoEstudio(Estado* s);         // hay algo que animar (giro de tarjeta, IA escribiendo)
void colocarRespuesta(Estado* s);        // muestra u oculta el campo de desarrollo

// ================================== enfoque y musica de GuiProfesorEnfoque.cpp
int pintarChipMusica(Estado* s, HDC dc, int derecha, int alto);   // barra de titulo; devuelven el ancho
int pintarChipEnfoque(Estado* s, HDC dc, int derecha, int alto);
void pintarPanelMusica(Estado* s, HDC dc, RECT cliente);
void pintarCapsulaEnfoque(Estado* s, HDC dc, RECT cliente);
void pintarPanelTest(Estado* s, HDC dc, RECT cliente);  // «Hacer un test» (Ctrl+T y capsula del enfoque)
// Efectos visuales de la musica de fondo (lluvia que cae, olas, brasas...) detras del contenido.
void pintarAmbiente(Estado* s, HDC dc, RECT r);
bool hayAmbiente(Estado* s);
bool enfoqueAccion(Estado* s, int accion, int dato);  // true si la ha atendido
void alternarEnfoque(Estado* s);
void alternarPantallaCompleta(Estado* s);
void empezarConcentracion(Estado* s);  // pomodoro (y la musica, si asi esta en los Ajustes)
// Pantalla de descanso: respiracion guiada, consejos y lo estudiado hoy frente al objetivo diario.
void pintarDescanso(Estado* s, HDC dc, RECT cliente);
void abrirDescanso(Estado* s);
void empezarDescanso(Estado* s);       // descanso del temporizador ahora mismo (y su pantalla)
int minutosHoy(Estado* s);             // minutos de concentracion de hoy (contando el bloque en curso)

// ================================ paleta de comandos y atajos de GuiProfesorComandos.cpp
void abrirComandos(Estado* s);
void pintarComandos(Estado* s, HDC dc, RECT cliente);
void pintarAtajos(Estado* s, HDC dc, RECT cliente);
// Barra de titulo: caja «Buscar o ir a…  Ctrl+K». Devuelve el ancho usado (0 si no cabe).
int pintarCajaComandos(Estado* s, HDC dc, int izquierda, int derecha, int alto);
bool comandosTecla(Estado* s, WPARAM tecla);    // true si la paleta o la ayuda la han atendido
bool comandosCaracter(Estado* s, wchar_t c);
bool comandosAccion(Estado* s, int accion, int dato);
bool hayCapa(Estado* s);                         // paleta, ayuda o descanso abiertos (tapan lo demas)

// De GuiProfesor.cpp: ejecuta una accion como si se hubiera pulsado su boton.
void ejecutar(Estado* s, int accion, int dato);
void volver(Estado* s);
void desplazar(Estado* s, int delta);

// ===================================== lector del temario de GuiProfesorTemario.cpp
int pintarTemario(Estado* s, HDC dc, Area a);
void abrirTemario(Estado* s, const std::string& ruta = "");  // "" = el primero sin leer
bool temarioRespuesta(Estado* s, const std::string& orden, const js::Valor& r);
bool temarioError(Estado* s, int id, const std::string& orden);
void soltarHojas(Estado* s);     // libera las paginas pintadas del documento abierto
bool temarioAccion(Estado* s, int accion, int dato);
bool temarioTecla(Estado* s, WPARAM tecla);
void guardarLectura(Estado* s);  // guarda por donde va el documento abierto
// Raton del lector (seleccionar texto o un recuadro del PDF). true si lo ha atendido.
bool temarioRaton(Estado* s, UINT msg, POINT pt);
void temarioTick(Estado* s);  // TIMER_ARRASTRE: sigue desplazando mientras se selecciona en un borde
// Ctrl+rueda amplia y Mayus+rueda mueve de lado (`teclas`: MK_CONTROL, MK_SHIFT del mensaje).
bool temarioRueda(Estado* s, int delta, bool lateral, int teclas);
LPCWSTR temarioCursor(Estado* s, POINT pt);             // cursor sobre el texto (NULL = el normal)
int anchoPanelLectura(Estado* s);                       // panel del profesor abierto: lo que ocupa a la derecha
bool campoEnPanelLectura(Estado* s);                    // el campo del chat va dentro de ese panel
// De GuiProfesorComandos.cpp: abre el resumen de un tema.
void abrirResumenDe(Estado* s, const std::string& ruta);

// ===================================== herramientas de GuiProfesorHerramientas.cpp
int pintarBuscar(Estado* s, HDC dc, Area a);
void colocarBusqueda(Estado* s);
int pintarMapa(Estado* s, HDC dc, int x, int y, int w);        // dentro de Resumenes
int pintarCalendario(Estado* s, HDC dc, int x, int y, int w);  // dentro de Inicio
int pintarLogros(Estado* s, HDC dc, int x, int y, int w);      // dentro de Progreso
int pintarPomodoro(Estado* s, HDC dc, int derecha, int alto);  // en la barra de titulo; devuelve el ancho
void pintarToast(Estado* s, HDC dc, RECT cuerpo);
void pintarSelector(Estado* s, HDC dc, RECT lado);
void aviso(Estado* s, const std::wstring& t);                  // aviso flotante unos segundos
bool herramientasRespuesta(Estado* s, const std::string& orden, const js::Valor& r);
bool herramientasError(Estado* s, const std::string& orden, const std::wstring& mensaje);
bool herramientasEvento(Estado* s, const js::Valor& ev);
bool herramientasAccion(Estado* s, int accion, int dato);
void abrirHerramientas(Estado* s, Vista v);
void buscarEnTemario(Estado* s, const std::wstring& texto);  // abre «Buscar» con ese texto ya buscado
void tickPomodoro(Estado* s);
void tickBusqueda(Estado* s);
bool animandoHerramientas(Estado* s);

}  // namespace ui
}  // namespace profesor
