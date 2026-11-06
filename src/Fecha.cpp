#include "Fecha.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {
const char* kMeses[] = {"enero", "febrero", "marzo",      "abril",   "mayo",      "junio",
                        "julio", "agosto",  "septiembre", "octubre", "noviembre", "diciembre"};
const char* kMesesCortos[] = {"ene", "feb", "mar", "abr", "may", "jun",
                              "jul", "ago", "sep", "oct", "nov", "dic"};
const char* kDias[] = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
}  // namespace

bool Fecha::bisiesto(int anio) {
    return (anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0);
}

int Fecha::diasEnMes(int anio, int mes) {
    static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mes < 1 || mes > 12) return 0;
    if (mes == 2 && bisiesto(anio)) return 29;
    return dias[mes - 1];
}

const char* Fecha::nombreMes(int mes) {
    return (mes >= 1 && mes <= 12) ? kMeses[mes - 1] : "?";
}

const char* Fecha::nombreMesCorto(int mes) {
    return (mes >= 1 && mes <= 12) ? kMesesCortos[mes - 1] : "?";
}

bool Fecha::valida() const {
    if (anio < 1900 || anio > 2200) return false;
    if (mes < 1 || mes > 12) return false;
    if (dia < 1 || dia > diasEnMes(anio, mes)) return false;
    if (hora > 23 || minuto < 0 || minuto > 59) return false;
    return true;
}

// Numero de dia continuo (algoritmo de Howard Hinnant, days_from_civil).
long Fecha::numeroDeDia() const {
    long y = anio;
    const unsigned m = static_cast<unsigned>(mes);
    const unsigned d = static_cast<unsigned>(dia);
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long>(doe) - 719468;
}

int Fecha::diaSemana() const {
    // numeroDeDia()==0 fue jueves (1970-01-01) -> desplazamos a lunes=0.
    long d = (numeroDeDia() + 3) % 7;
    if (d < 0) d += 7;
    return static_cast<int>(d);
}

long Fecha::diasHasta(const Fecha& otra) const {
    return otra.numeroDeDia() - numeroDeDia();
}

bool Fecha::desdeTexto(const std::string& texto, Fecha& salida) {
    Fecha f;
    int a = 0, m = 0, d = 0, h = -1, mi = 0;

    // ISO: 2026-09-14 [10:30]
    int leidos = std::sscanf(texto.c_str(), "%d-%d-%d %d:%d", &a, &m, &d, &h, &mi);
    if (leidos < 3) {
        // Europeo: 14/09/2026 [10:30]
        h = -1;
        mi = 0;
        leidos = std::sscanf(texto.c_str(), "%d/%d/%d %d:%d", &d, &m, &a, &h, &mi);
        if (leidos < 3) return false;
    }
    if (leidos == 4) mi = 0;   // hora sin minutos
    if (leidos < 4) h = -1;    // sin hora

    f.anio = a;
    f.mes = m;
    f.dia = d;
    f.hora = h;
    f.minuto = mi;
    if (!f.valida()) return false;
    salida = f;
    return true;
}

std::string Fecha::aTexto() const {
    char buf[32];
    if (tieneHora()) {
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", anio, mes, dia, hora, minuto);
    } else {
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", anio, mes, dia);
    }
    return buf;
}

std::string Fecha::aTextoCorto() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d", dia, mes, anio);
    return buf;
}

std::string Fecha::aTextoLargo() const {
    char buf[64];
    const char* ds = kDias[diaSemana()];
    if (tieneHora()) {
        std::snprintf(buf, sizeof(buf), "%s %d %s %d, %02d:%02d", ds, dia, nombreMesCorto(mes),
                      anio, hora, minuto);
    } else {
        std::snprintf(buf, sizeof(buf), "%s %d %s %d", ds, dia, nombreMesCorto(mes), anio);
    }
    return buf;
}

Fecha Fecha::hoy() {
    std::time_t t = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    local = *std::localtime(&t);
#endif
    Fecha f;
    f.anio = local.tm_year + 1900;
    f.mes = local.tm_mon + 1;
    f.dia = local.tm_mday;
    f.hora = local.tm_hour;
    f.minuto = local.tm_min;
    return f;
}

bool operator<(const Fecha& a, const Fecha& b) {
    if (a.anio != b.anio) return a.anio < b.anio;
    if (a.mes != b.mes) return a.mes < b.mes;
    if (a.dia != b.dia) return a.dia < b.dia;
    int ha = a.hora < 0 ? 0 : a.hora;
    int hb = b.hora < 0 ? 0 : b.hora;
    if (ha != hb) return ha < hb;
    return a.minuto < b.minuto;
}

bool operator==(const Fecha& a, const Fecha& b) {
    return a.anio == b.anio && a.mes == b.mes && a.dia == b.dia && a.hora == b.hora &&
           a.minuto == b.minuto;
}
