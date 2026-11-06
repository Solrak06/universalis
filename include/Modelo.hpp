// Modelo.hpp - Entidades del dominio: Examen, Asignatura y Curso.
#pragma once

#include <string>
#include <vector>

#include "Fecha.hpp"

// Una prueba evaluable: parcial, practica entregable, trabajo, final...
struct Examen {
    std::string nombre;        // "Parcial 1", "Practica 3"
    Fecha fecha;
    double ponderacion = 0.0;  // % que aporta a la nota final de la asignatura
    bool calificado = false;
    double nota = 0.0;         // 0-10, solo valido si calificado
    std::string apuntes;       // temas que entran, recordatorios

    double aportacion() const;  // puntos sobre 10 que este examen ya ha aportado
    bool esFuturo() const;      // aun no ha llegado su fecha
};

struct Asignatura {
    std::string codigo;              // "PROG2"
    std::string nombre;              // "Programacion Avanzada"
    std::string profesor;
    int creditos = 6;
    std::string cuatrimestre;        // texto libre: "2 curso", "optativa"...
    int cuatri = 1;                  // 1 o 2: bloque en el que se agrupa
    std::string diasClase;           // dias de clase: "LMX", "MJ", "LXV"...
    std::string color;               // "#5b9dff" (vacio = color automatico)
    std::string carpetaTemario;      // relativa a la carpeta del programa
    std::string carpetaPracticas;    // relativa a la carpeta del programa
    std::vector<Examen> examenes;

    double ponderacionTotal() const;      // suma de ponderaciones definidas
    double ponderacionEvaluada() const;   // suma de ponderaciones ya calificadas
    double notaMediaActual() const;        // media ponderada solo de lo calificado (0-10)
    double notaAcumulada() const;          // puntos sobre 10 conseguidos del total
    double notaNecesaria(double objetivo = 5.0) const;  // media requerida en lo que queda
    bool tieneNotas() const;
    void ordenarExamenes();
};

// Preferencias del programa (se guardan en el mismo JSON).
struct Ajustes {
    std::string colorAcento = "#5b9dff";  // color principal de la interfaz
    int escalaInterfaz = 100;             // 100 = normal, 115 y 130 = mas grande
    bool primerDiaLunes = true;           // primera columna del calendario
    bool mostrarFinDeSemana = true;       // sabado y domingo en el calendario
    int diasDeAviso = 7;                  // resaltar pruebas a menos de N dias
    double notaObjetivo = 5.0;            // objetivo para la barra de la ficha
    bool ordenarPorDia = false;           // ordenar asignaturas por dia de clase
    bool confirmarBorrado = true;         // preguntar antes de borrar

    // Apariencia (ventana de Ajustes; ver gui::Estilo)
    bool temaSistema = false;             // claro u oscuro segun Windows
    int paleta = 0;                       // tinte del fondo
    std::string fuente;                   // tipo de letra ("" = el de Windows)
    int tamTexto = 100;                   // tamano de letra en %
    int esquinas = 100;                   // redondeo en %
    int fondo = 2;                        // halos del fondo: 0 liso ... 3 intensos
    bool cristal = true;                  // tarjetas con brillo y sombra
    bool animaciones = true;              // fundidos entre pantallas
    int barra = 0;                        // color de la barra superior
    bool contraste = false;               // textos mas marcados
    int anchura = 0;                      // ancho del contenido: 0 normal, 1 amplio, 2 completo
    int pomodoro = 25;                    // minutos de estudio del temporizador
    int descanso = 5;                     // minutos de descanso
    bool musicaAlConcentrarse = false;    // el temporizador pone la ultima musica de fondo
    bool campana = true;                  // campanilla al acabar cada bloque y cada descanso
    bool respiracion = true;              // pantalla de descanso con respiracion guiada
    int objetivoDiario = 60;              // minutos de concentracion al dia (0 = sin objetivo)
    bool efectosAmbiente = true;          // efectos visuales de la musica de fondo
};

struct Curso {
    std::string titulo = "Mi curso";
    bool temaOscuro = true;          // preferencia de la interfaz grafica
    Ajustes ajustes;
    std::vector<Asignatura> asignaturas;

    Asignatura* buscarPorCodigo(const std::string& codigo);
    double mediaExpediente() const;   // media de notas actuales ponderada por creditos
    int creditosConNota() const;
};

// Un examen junto con la asignatura a la que pertenece (para vistas globales).
struct ExamenConAsignatura {
    const Asignatura* asignatura = nullptr;
    const Examen* examen = nullptr;
};

std::vector<ExamenConAsignatura> todosLosExamenes(const Curso& curso);
std::vector<ExamenConAsignatura> proximosExamenes(const Curso& curso, int limite = 8);

// -------------------------------------------------------- vistas del curso
// Indices de las asignaturas de un cuatrimestre, en el orden pedido.
std::vector<std::size_t> asignaturasDe(const Curso& curso, int cuatri, bool ordenarPorDia);
// Primer dia de clase (0 = lunes ... 6 = domingo, 7 si no tiene dias puestos).
int primerDiaDeClase(const Asignatura& a);
// Dias que faltan para la proxima prueba (-1 si no queda ninguna).
long diasHastaProximaPrueba(const Asignatura& a);
// Cuantas pruebas quedan sin calificar en todo el curso.
int pruebasPendientes(const Curso& curso);
// Media ponderada por creditos de un solo cuatrimestre.
double mediaDeCuatrimestre(const Curso& curso, int cuatri);
// Copia una asignatura para duplicarla (con nombre y codigo nuevos).
Asignatura duplicarAsignatura(const Asignatura& original);

// ---------------------------------------------------------------- carpetas
// Las carpetas de material viven dentro de la carpeta del programa:
//   <programa>/Asignaturas/<CODIGO>/Temario
//   <programa>/Asignaturas/<CODIGO>/Practicas
std::string nombreSeguroParaCarpeta(const std::string& texto);
// Carpeta raiz de la asignatura: <programa>/Asignaturas/<CODIGO>
std::string carpetaRaizDeAsignatura(const Asignatura& a);
std::string carpetaRelativaPorDefecto(const Asignatura& a, bool temario);
// Devuelve la ruta absoluta final (usa la personalizada si esta definida).
std::string carpetaDeAsignatura(const Asignatura& a, bool temario);
// Crea la carpeta si no existe. Devuelve la ruta absoluta o cadena vacia si falla.
std::string prepararCarpetaDeAsignatura(const Asignatura& a, bool temario);
// Crea el arbol completo de carpetas de todas las asignaturas del curso.
void prepararCarpetasDelCurso(const Curso& curso);

// ----------------------------------------------------------------- colores
// Paleta usada cuando la asignatura no tiene color propio.
std::size_t coloresDisponibles();
std::string colorDePaleta(std::size_t indice);
// "#5b9dff" -> 0x5b9dff ; devuelve false si el texto no es valido
bool colorDesdeTexto(const std::string& texto, unsigned long& rgb);
std::string colorATexto(unsigned long rgb);

// ------------------------------------------------------- dias de la semana
// Los dias se guardan como letras: L M X J V S D (0 = lunes ... 6 = domingo).
const char* letraDiaSemana(int diaSemana);
std::string nombreDiaSemana(int diaSemana);
// Deja solo letras validas, en orden y sin repetir: "xLm" -> "LMX"
std::string normalizarDiasClase(const std::string& texto);
bool hayClase(const Asignatura& a, int diaSemana);
// "LMX" -> "lun, mar, mie"
std::string diasClaseLegibles(const Asignatura& a);

// ------------------------------------------------------------ calificaciones
// Escala espanola:  <5 suspenso | 5-6 aprobado | 6-7 bien | 7-9 notable | >=9 sobresaliente
enum Nivel { NIVEL_SUSPENSO = 0, NIVEL_APROBADO, NIVEL_BIEN, NIVEL_NOTABLE, NIVEL_SOBRESALIENTE };

int nivelDeNota(double nota);
std::string calificacionDeNota(double nota);   // "Suspenso", "Notable", ...
std::string calificacionDeNivel(int nivel);

// Utilidades de formato compartidas.
std::string formatearNota(double nota, int decimales = 2);
std::string barraProgreso(double porcentaje, int ancho = 20);
