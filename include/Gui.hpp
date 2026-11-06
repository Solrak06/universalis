// Gui.hpp - Utilidades comunes de la interfaz grafica Win32.
//
// Toda la GUI usa la API "ancha" (UTF-16) de Windows y convierte a UTF-8
// para hablar con el modelo (que trabaja con std::string en UTF-8).
#pragma once

#ifndef UNICODE
#  define UNICODE
#endif
#ifndef _UNICODE
#  define _UNICODE
#endif
#ifndef WINVER
#  define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#ifndef _WIN32_IE
#  define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

namespace gui {

// ---------------------------------------------------------------- conversion
std::wstring aW(const std::string& utf8);
std::string aUtf8(const std::wstring& texto);

// -------------------------------------------------------------------- tema
struct Tema {
    bool oscuro;
    COLORREF fondo;       // fondo de la ventana
    COLORREF panel;       // fondo de listas y cajas
    COLORREF panelAlt;    // fondo de botones
    COLORREF texto;
    COLORREF textoSuave;
    COLORREF borde;
    COLORREF acento;
    COLORREF seleccion;
    COLORREF textoSeleccion;
    COLORREF ok;
    COLORREF medio;
    COLORREF mal;
};

const Tema& tema();

// Personalizacion global (ventana de Ajustes). La leen todas las ventanas al pintar.
// Se pone con ponerEstilo() antes de ponerTema().
struct Estilo {
    int paleta = 0;            // tinte del fondo (ver nombrePaleta)
    std::wstring fuente;       // tipo de letra ("" = el de Windows: Segoe UI Variable / Segoe UI)
    int texto = 100;           // tamano de la letra en % (90 - 130)
    int esquinas = 100;        // redondeo de tarjetas y botones en % (0 = rectas)
    int fondo = 2;             // halos de color del fondo: 0 liso, 1 suaves, 2 normales, 3 intensos
    bool cristal = true;       // tarjetas con brillo, transparencia y sombra
    bool animaciones = true;   // fundidos al cambiar de pantalla
    int barra = 0;             // barra superior: 0 negra, 1 grafito, 2 del color de acento, 3 de la paleta
    bool contraste = false;    // textos secundarios y bordes mas marcados
    int anchura = 0;           // contenido de las ventanas grandes: 0 normal, 1 amplio, 2 todo el ancho
    int pomodoro = 25;         // minutos de estudio del temporizador
    int descanso = 5;          // minutos de descanso
    bool musicaAlConcentrarse = false;  // el temporizador pone la ultima musica de fondo
    bool campana = true;       // campanilla al acabar cada bloque y cada descanso
    bool respiracion = true;   // pantalla de descanso con respiracion guiada
    int objetivoDiario = 60;   // minutos de concentracion al dia (0 = sin objetivo)
    bool efectosAmbiente = true;  // lluvia, olas, brasas... en el fondo mientras suena la musica
};
const Estilo& estilo();
void ponerEstilo(const Estilo& e);
int numPaletas();
const wchar_t* nombrePaleta(int paleta);
COLORREF muestraPaleta(int paleta, bool oscuro);  // color de fondo de la paleta (para las muestras)
bool sistemaEnOscuro();                         // Windows usa el modo oscuro en las aplicaciones
int radioEstilo(int radio);                     // aplica el redondeo elegido
double anchuraEstilo();                         // factor del ancho maximo del contenido
COLORREF textoBarraSuperior();

// Color de acento y escala (100 = normal, 115, 130...) salen de los ajustes.
void ponerTema(bool oscuro, COLORREF acento = 0, int escala = 100);
int escalaInterfaz();
int escalar(int medida);  // aplica la escala a una medida en pixeles
HBRUSH brochaFondo();
HBRUSH brochaPanel();
HBRUSH brochaAlt();

COLORREF mezclar(COLORREF a, COLORREF b, double t);  // t=0 -> a, t=1 -> b
COLORREF colorDeAsignatura(const std::string& colorHex, std::size_t indice);
COLORREF colorDeNota(double nota);       // color segun suspenso/aprobado/bien/...
COLORREF colorDeNivel(int nivel);
// Barra superior negra de la aplicacion.
COLORREF colorBarraSuperior();

// ------------------------------------------------------------------ fuentes
HFONT fuenteInterfaz();
HFONT fuenteNegrita();
HFONT fuenteTitulo();
void aplicarFuente(HWND ventana);

// ------------------------------------------------------------------ controles
HWND crearEtiqueta(HWND padre, const std::wstring& texto, int id = -1);
HWND crearBoton(HWND padre, const std::wstring& texto, int id, bool destacado = false);
// Marca un boton como parte de la barra superior negra (cambia sus colores).
void botonEnBarra(HWND boton);

// Iconos lineales (GDI+, suavizados). Sirven para botones, titulos y decoracion.
enum Icono {
    ICONO_NINGUNO = 0, ICONO_EXPORTAR, ICONO_TEMA, ICONO_AJUSTES,
    ICONO_MAS, ICONO_EDITAR, ICONO_DUPLICAR, ICONO_BORRAR, ICONO_CARPETA, ICONO_LIBRO, ICONO_MATRAZ,
    ICONO_ESTRELLA, ICONO_BIRRETE, ICONO_CALENDARIO, ICONO_AYUDA, ICONO_AGENDA, ICONO_NOTAS, ICONO_FICHA,
    ICONO_FLECHA, ICONO_ANTERIOR, ICONO_SIGUIENTE, ICONO_INFO, ICONO_AVISO,
    ICONO_TEXTO, ICONO_VENTANA
};
void dibujarIcono(HDC dc, Icono icono, int cx, int cy, int tam, COLORREF color, int alfa = 255);
// Botones con icono dibujado (barra superior).
HWND crearBotonIcono(HWND padre, Icono icono, int id, const std::wstring& ayuda);
// Boton normal con icono delante del texto y explicacion al pasar el raton.
HWND crearBotonConIcono(HWND padre, Icono icono, const std::wstring& texto, int id, bool destacado,
                        const std::wstring& ayuda);
void ponerAyuda(HWND control, const std::wstring& ayuda);
void cambiarIcono(HWND boton, Icono icono);
// Dibujo decorativo: assets/iconos/<nombre>.png (monocromo sobre transparente) tenido del color pedido
// y con la opacidad dada (0-255). Si el fichero no existe, se dibuja el icono lineal de reserva.
void decoracion(HDC dc, const std::wstring& nombre, const RECT& r, COLORREF color, int alfa, Icono reserva);
bool hayDecoracion(const std::wstring& nombre);  // existe assets/iconos/<nombre>.png
HWND crearLista(HWND padre, int id);
void dibujarBoton(const DRAWITEMSTRUCT* di);
void tematizarLista(HWND lista);
void alturaFilasLista(HWND lista, int alto);

// Dibujo de tarjetas y marcos (para dar aire a la ventana).
void panelRedondeado(HDC dc, const RECT& r, COLORREF fondo, COLORREF borde, int radio);
void marcoAlrededorDe(HDC dc, HWND control, COLORREF borde, int radio, int margen = 1);
void barraProgresoDibujada(HDC dc, const RECT& r, double porcentaje, COLORREF color,
                           COLORREF fondo);
void textoSeccion(HDC dc, const RECT& r, const std::wstring& texto);
// Titulo de seccion con icono en un cuadradito de color y una pista a la derecha
// («Pulsa un dia para ver su agenda»). Si no cabe, la pista se recorta.
void tituloConIcono(HDC dc, const RECT& r, Icono icono, const std::wstring& texto, const std::wstring& pista,
                    COLORREF color, COLORREF colorPista = CLR_INVALID);
// Etiqueta redondeada de color (Suspenso, Notable, ...). Devuelve su ancho.
int pastilla(HDC dc, int x, int y, int alto, const std::wstring& texto, COLORREF color);

void columna(HWND lista, int indice, const std::wstring& titulo, int ancho);
void limpiarLista(HWND lista);
int anadirFila(HWND lista, const std::wstring& primerTexto, LPARAM datos);
void ponerTexto(HWND lista, int fila, int columna, const std::wstring& texto);
int filaSeleccionada(HWND lista);
void seleccionarFila(HWND lista, int fila);
LPARAM datosDeFila(HWND lista, int fila);

// ------------------------------------------------------- integracion con Windows
// Pide a Windows el modo oscuro para los controles nativos (barras de
// desplazamiento, bordes). Todo con carga dinamica: si no existe, no pasa nada.
void modoOscuroDelSistema(bool oscuro);
// Pinta en oscuro la barra de titulo de una ventana (Win10 2004+).
void barraDeTituloOscura(HWND ventana);
void tematizarControlNativo(HWND control, const wchar_t* claseTema);
void tematizarDesplazamiento(HWND control);  // listas: barras de scroll oscuras

// -------------------------------------------------------------------- avisos
// Cuadros dibujados por el programa: los de Windows siempre salen blancos.
void avisar(HWND padre, const std::wstring& texto);
void avisarError(HWND padre, const std::wstring& texto);
bool preguntar(HWND padre, const std::wstring& texto);
// Selector de color propio (rejilla de muestras).
bool elegirColor(HWND padre, COLORREF actual, COLORREF& elegido);

// -------------------------------------------------------------------- shell
bool abrirEnSistema(HWND padre, const std::wstring& ruta);

// --------------------------------------------------------------- formulario
struct Campo {
    enum Tipo { Texto, Numero, Fecha, Casilla, Color, Accion };

    std::wstring etiqueta;
    std::wstring valor;  // entrada y salida
    Tipo tipo = Texto;
    double minimo = 0.0;
    double maximo = 0.0;
    void (*accion)(HWND) = NULL;  // solo para Tipo::Accion
    std::wstring pista;   // ejemplo en gris dentro del campo vacio («Ej.: SOA»)
    std::wstring ayuda;   // explicacion corta bajo la etiqueta

    Campo() = default;
    Campo(std::wstring et, std::wstring val, Tipo t = Texto)
        : etiqueta(std::move(et)), valor(std::move(val)), tipo(t) {}
    Campo(std::wstring et, std::wstring val, Tipo t, double mn, double mx)
        : etiqueta(std::move(et)), valor(std::move(val)), tipo(t), minimo(mn), maximo(mx) {}
};

// Con subtitulo e icono, el formulario lleva una cabecera que explica para que sirve.
bool formulario(HWND padre, const std::wstring& titulo, std::vector<Campo>& campos,
                const std::wstring& subtitulo = L"", Icono icono = ICONO_NINGUNO);
// Pone ejemplo y ayuda al ultimo campo anadido (para no alargar las llamadas).
void pistaDelUltimo(std::vector<Campo>& campos, const std::wstring& pista, const std::wstring& ayuda = L"");

}  // namespace gui
