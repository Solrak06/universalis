#include "Json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace js {

namespace {
const Valor kNulo;  // devuelto cuando una clave/índice no existe
}

Valor::Valor(bool b) : tipo_(Tipo::Booleano), bool_(b) {}
Valor::Valor(double n) : tipo_(Tipo::Numero), num_(n) {}
Valor::Valor(int n) : tipo_(Tipo::Numero), num_(static_cast<double>(n)) {}
Valor::Valor(const char* s) : tipo_(Tipo::Cadena), cad_(s ? s : "") {}
Valor::Valor(std::string s) : tipo_(Tipo::Cadena), cad_(std::move(s)) {}

Valor Valor::array() {
    Valor v;
    v.tipo_ = Tipo::Array;
    return v;
}

Valor Valor::objeto() {
    Valor v;
    v.tipo_ = Tipo::Objeto;
    return v;
}

bool Valor::comoBool(bool porDefecto) const {
    if (tipo_ == Tipo::Booleano) return bool_;
    if (tipo_ == Tipo::Numero) return num_ != 0.0;
    return porDefecto;
}

double Valor::comoNumero(double porDefecto) const {
    if (tipo_ == Tipo::Numero) return num_;
    return porDefecto;
}

int Valor::comoEntero(int porDefecto) const {
    if (tipo_ == Tipo::Numero) return static_cast<int>(num_);
    return porDefecto;
}

std::string Valor::comoCadena(const std::string& porDefecto) const {
    if (tipo_ == Tipo::Cadena) return cad_;
    return porDefecto;
}

std::size_t Valor::tam() const {
    if (tipo_ == Tipo::Array) return elementos_.size();
    if (tipo_ == Tipo::Objeto) return claves_.size();
    return 0;
}

const Valor& Valor::operator[](std::size_t i) const {
    if (tipo_ == Tipo::Array && i < elementos_.size()) return elementos_[i];
    return kNulo;
}

void Valor::agregar(Valor v) {
    if (tipo_ != Tipo::Array) {
        *this = Valor::array();
    }
    elementos_.push_back(std::move(v));
}

bool Valor::contiene(const std::string& clave) const {
    if (tipo_ != Tipo::Objeto) return false;
    for (const auto& k : claves_) {
        if (k == clave) return true;
    }
    return false;
}

const Valor& Valor::operator[](const std::string& clave) const {
    if (tipo_ == Tipo::Objeto) {
        for (std::size_t i = 0; i < claves_.size(); ++i) {
            if (claves_[i] == clave) return valores_[i];
        }
    }
    return kNulo;
}

Valor& Valor::operator[](const std::string& clave) {
    if (tipo_ != Tipo::Objeto) {
        *this = Valor::objeto();
    }
    for (std::size_t i = 0; i < claves_.size(); ++i) {
        if (claves_[i] == clave) return valores_[i];
    }
    claves_.push_back(clave);
    valores_.emplace_back();
    return valores_.back();
}

// ---------------------------------------------------------------- serializar

static void escaparCadena(const std::string& s, std::string& out) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);  // UTF-8 pasa tal cual
                }
        }
    }
    out += '"';
}

static void escribirNumero(double n, std::string& out) {
    char buf[64];
    if (n == std::floor(n) && std::fabs(n) < 1e15) {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
    } else {
        std::snprintf(buf, sizeof(buf), "%.10g", n);
    }
    out += buf;
}

void Valor::volcarEn(std::string& out, int sangria, int nivel) const {
    const bool bonito = sangria > 0;
    const std::string ind(bonito ? sangria * (nivel + 1) : 0, ' ');
    const std::string indCierre(bonito ? sangria * nivel : 0, ' ');
    const char* salto = bonito ? "\n" : "";

    switch (tipo_) {
        case Tipo::Nulo: out += "null"; break;
        case Tipo::Booleano: out += (bool_ ? "true" : "false"); break;
        case Tipo::Numero: escribirNumero(num_, out); break;
        case Tipo::Cadena: escaparCadena(cad_, out); break;
        case Tipo::Array:
            if (elementos_.empty()) { out += "[]"; break; }
            out += "[";
            out += salto;
            for (std::size_t i = 0; i < elementos_.size(); ++i) {
                out += ind;
                elementos_[i].volcarEn(out, sangria, nivel + 1);
                if (i + 1 < elementos_.size()) out += ",";
                out += salto;
            }
            out += indCierre;
            out += "]";
            break;
        case Tipo::Objeto:
            if (claves_.empty()) { out += "{}"; break; }
            out += "{";
            out += salto;
            for (std::size_t i = 0; i < claves_.size(); ++i) {
                out += ind;
                escaparCadena(claves_[i], out);
                out += bonito ? ": " : ":";
                valores_[i].volcarEn(out, sangria, nivel + 1);
                if (i + 1 < claves_.size()) out += ",";
                out += salto;
            }
            out += indCierre;
            out += "}";
            break;
    }
}

std::string Valor::volcar(int sangria) const {
    std::string out;
    volcarEn(out, sangria, 0);
    return out;
}

// -------------------------------------------------------------------- parsear

namespace {

class Lector {
public:
    Lector(const std::string& t) : t_(t) {}

    bool parsearValor(Valor& salida) {
        saltarEspacios();
        if (i_ >= t_.size()) return fallo("fin de texto inesperado");
        char c = t_[i_];
        switch (c) {
            case '{': return parsearObjeto(salida);
            case '[': return parsearArray(salida);
            case '"': {
                std::string s;
                if (!parsearCadena(s)) return false;
                salida = Valor(s);
                return true;
            }
            case 't':
                if (t_.compare(i_, 4, "true") == 0) { i_ += 4; salida = Valor(true); return true; }
                return fallo("se esperaba 'true'");
            case 'f':
                if (t_.compare(i_, 5, "false") == 0) { i_ += 5; salida = Valor(false); return true; }
                return fallo("se esperaba 'false'");
            case 'n':
                if (t_.compare(i_, 4, "null") == 0) { i_ += 4; salida = Valor(); return true; }
                return fallo("se esperaba 'null'");
            default:
                return parsearNumero(salida);
        }
    }

    void saltarEspacios() {
        while (i_ < t_.size() &&
               (t_[i_] == ' ' || t_[i_] == '\t' || t_[i_] == '\n' || t_[i_] == '\r')) {
            ++i_;
        }
    }

    bool finLimpio() {
        saltarEspacios();
        if (i_ != t_.size()) return fallo("contenido sobrante tras el valor");
        return true;
    }

    std::string error;

private:
    bool fallo(const std::string& msg) {
        if (error.empty()) {
            error = msg + " (posicion " + std::to_string(i_) + ")";
        }
        return false;
    }

    bool parsearCadena(std::string& salida) {
        if (t_[i_] != '"') return fallo("se esperaba '\"'");
        ++i_;
        salida.clear();
        while (i_ < t_.size()) {
            char c = t_[i_++];
            if (c == '"') return true;
            if (c != '\\') { salida += c; continue; }
            if (i_ >= t_.size()) return fallo("escape incompleto");
            char e = t_[i_++];
            switch (e) {
                case '"':  salida += '"';  break;
                case '\\': salida += '\\'; break;
                case '/':  salida += '/';  break;
                case 'b':  salida += '\b'; break;
                case 'f':  salida += '\f'; break;
                case 'n':  salida += '\n'; break;
                case 'r':  salida += '\r'; break;
                case 't':  salida += '\t'; break;
                case 'u': {
                    if (i_ + 4 > t_.size()) return fallo("escape \\u incompleto");
                    unsigned cp = static_cast<unsigned>(
                        std::strtoul(t_.substr(i_, 4).c_str(), nullptr, 16));
                    i_ += 4;
                    // Codificar en UTF-8 (sin soporte de pares suplentes).
                    if (cp < 0x80) {
                        salida += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        salida += static_cast<char>(0xC0 | (cp >> 6));
                        salida += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        salida += static_cast<char>(0xE0 | (cp >> 12));
                        salida += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        salida += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: return fallo("escape desconocido");
            }
        }
        return fallo("cadena sin cerrar");
    }

    bool parsearNumero(Valor& salida) {
        std::size_t inicio = i_;
        if (i_ < t_.size() && (t_[i_] == '-' || t_[i_] == '+')) ++i_;
        bool digitos = false;
        while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) { ++i_; digitos = true; }
        if (i_ < t_.size() && t_[i_] == '.') {
            ++i_;
            while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) { ++i_; digitos = true; }
        }
        if (i_ < t_.size() && (t_[i_] == 'e' || t_[i_] == 'E')) {
            ++i_;
            if (i_ < t_.size() && (t_[i_] == '-' || t_[i_] == '+')) ++i_;
            while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) ++i_;
        }
        if (!digitos) return fallo("numero no valido");
        salida = Valor(std::strtod(t_.substr(inicio, i_ - inicio).c_str(), nullptr));
        return true;
    }

    bool parsearArray(Valor& salida) {
        ++i_;  // '['
        salida = Valor::array();
        saltarEspacios();
        if (i_ < t_.size() && t_[i_] == ']') { ++i_; return true; }
        while (true) {
            Valor v;
            if (!parsearValor(v)) return false;
            salida.agregar(std::move(v));
            saltarEspacios();
            if (i_ >= t_.size()) return fallo("array sin cerrar");
            if (t_[i_] == ',') { ++i_; continue; }
            if (t_[i_] == ']') { ++i_; return true; }
            return fallo("se esperaba ',' o ']'");
        }
    }

    bool parsearObjeto(Valor& salida) {
        ++i_;  // '{'
        salida = Valor::objeto();
        saltarEspacios();
        if (i_ < t_.size() && t_[i_] == '}') { ++i_; return true; }
        while (true) {
            saltarEspacios();
            std::string clave;
            if (i_ >= t_.size() || t_[i_] != '"') return fallo("se esperaba una clave");
            if (!parsearCadena(clave)) return false;
            saltarEspacios();
            if (i_ >= t_.size() || t_[i_] != ':') return fallo("se esperaba ':'");
            ++i_;
            Valor v;
            if (!parsearValor(v)) return false;
            salida[clave] = std::move(v);
            saltarEspacios();
            if (i_ >= t_.size()) return fallo("objeto sin cerrar");
            if (t_[i_] == ',') { ++i_; continue; }
            if (t_[i_] == '}') { ++i_; return true; }
            return fallo("se esperaba ',' o '}'");
        }
    }

    const std::string& t_;
    std::size_t i_ = 0;
};

}  // namespace

bool Valor::parsear(const std::string& texto, Valor& salida, std::string& error) {
    Lector lector(texto);
    if (!lector.parsearValor(salida) || !lector.finLimpio()) {
        error = lector.error;
        return false;
    }
    error.clear();
    return true;
}

}  // namespace js
