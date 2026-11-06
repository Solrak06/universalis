#include "ProfesorMotor.hpp"

#include <vector>

#include "Rutas.hpp"

namespace profesor {

namespace {

bool existeArchivo(const std::wstring& ruta) {
    DWORD a = GetFileAttributesW(ruta.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring variable(const wchar_t* nombre) {
    wchar_t valor[2048];
    DWORD n = GetEnvironmentVariableW(nombre, valor, 2048);
    return (n > 0 && n < 2048) ? std::wstring(valor, n) : std::wstring();
}

struct Lector {
    HANDLE tuberia;
    HWND ventana;
    UINT mensaje;
};

}  // namespace

Motor::~Motor() { detener(); }

std::wstring Motor::buscarPython() {
    std::vector<std::wstring> candidatos;
    // 1) Elegido por el usuario.
    candidatos.push_back(variable(L"UNIVERSALIS_PYTHON"));
    // 2) Un Python portable dentro de la carpeta del programa.
    std::wstring base = gui::aW(rutas::directorioDelEjecutable());
    candidatos.push_back(base + L"\\profesor\\python\\python.exe");
    // 3) Runtime con las librerias de PDF ya instaladas.
    candidatos.push_back(variable(L"USERPROFILE") +
                         L"\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe");
    for (const std::wstring& c : candidatos) {
        if (!c.empty() && existeArchivo(c)) return c;
    }
    // 4) El python.exe del PATH (sin el acceso directo de la Microsoft Store).
    wchar_t ruta[MAX_PATH];
    if (SearchPathW(NULL, L"python.exe", NULL, MAX_PATH, ruta, NULL)) {
        std::wstring encontrado = ruta;
        if (encontrado.find(L"WindowsApps") == std::wstring::npos) return encontrado;
    }
    // 5) Instalaciones habituales de python.org.
    std::wstring local = variable(L"LOCALAPPDATA") + L"\\Programs\\Python\\";
    for (const wchar_t* v : {L"Python313", L"Python312", L"Python311", L"Python310"}) {
        if (existeArchivo(local + v + L"\\python.exe")) return local + v + L"\\python.exe";
    }
    return L"";
}

bool Motor::arrancar(HWND ventana, UINT mensaje, std::wstring& error) {
    detener();
    ventana_ = ventana;
    mensaje_ = mensaje;

    std::wstring python = buscarPython();
    if (python.empty()) {
        error = L"No encuentro Python. Instálalo desde python.org (marca «Add to PATH») "
                L"o indica su ruta en la variable UNIVERSALIS_PYTHON.";
        return false;
    }
    std::wstring base = gui::aW(rutas::directorioDelEjecutable());
    std::wstring script = base + L"\\profesor\\servidor.py";
    if (!existeArchivo(script)) {
        error = L"Falta la carpeta «profesor» junto al programa (profesor\\servidor.py).";
        return false;
    }
    std::wstring datos = base + L"\\datos\\profesor";
    rutas::crearDirectorios(gui::aUtf8(datos));

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hijoLee = NULL, hijoEscribe = NULL;
    if (!CreatePipe(&hijoLee, &escritura_, &sa, 0) || !CreatePipe(&lectura_, &hijoEscribe, &sa, 0)) {
        error = L"No se pudo crear la comunicación con el profesor.";
        return false;
    }
    // Nuestros extremos no los hereda el hijo.
    SetHandleInformation(escritura_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(lectura_, HANDLE_FLAG_INHERIT, 0);

    // Los errores de Python van a un registro para poder diagnosticar.
    std::wstring registro = datos + L"\\motor.log";
    HANDLE errores = CreateFileW(registro.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, NULL);

    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
    SetEnvironmentVariableW(L"PYTHONUTF8", L"1");
    SetEnvironmentVariableW(L"PYTHONDONTWRITEBYTECODE", L"1");

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hijoLee;
    si.hStdOutput = hijoEscribe;
    si.hStdError = errores != INVALID_HANDLE_VALUE ? errores : hijoEscribe;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    std::wstring orden = L"\"" + python + L"\" -u \"" + script + L"\" \"" + datos + L"\"";
    std::vector<wchar_t> buffer(orden.begin(), orden.end());
    buffer.push_back(0);
    BOOL ok = CreateProcessW(python.c_str(), buffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                             base.c_str(), &si, &pi);
    CloseHandle(hijoLee);
    CloseHandle(hijoEscribe);
    if (errores != INVALID_HANDLE_VALUE) CloseHandle(errores);
    if (!ok) {
        error = L"No se pudo iniciar Python (" + python + L").";
        detener();
        return false;
    }
    CloseHandle(pi.hThread);
    proceso_ = pi.hProcess;

    Lector* lector = new Lector{lectura_, ventana_, mensaje_};
    hilo_ = CreateThread(NULL, 0, hiloLector, lector, 0, NULL);
    if (!hilo_) {
        delete lector;
        error = L"No se pudo crear el hilo de lectura.";
        detener();
        return false;
    }
    return true;
}

DWORD WINAPI Motor::hiloLector(LPVOID datos) {
    Lector* lector = static_cast<Lector*>(datos);
    std::string pendiente;
    char trozo[8192];
    DWORD leidos = 0;
    while (ReadFile(lector->tuberia, trozo, sizeof(trozo), &leidos, NULL) && leidos > 0) {
        pendiente.append(trozo, leidos);
        std::size_t fin;
        while ((fin = pendiente.find('\n')) != std::string::npos) {
            std::string* linea = new std::string(pendiente.substr(0, fin));
            pendiente.erase(0, fin + 1);
            if (!PostMessageW(lector->ventana, lector->mensaje, 0, (LPARAM)linea)) delete linea;
        }
    }
    // Fin de la tuberia: el proceso ha terminado (o se ha cerrado la ventana).
    PostMessageW(lector->ventana, lector->mensaje, 1, 0);
    delete lector;
    return 0;
}

int Motor::enviar(js::Valor peticion) {
    if (!vivo()) return 0;
    int id = siguienteId_++;
    peticion["id"] = id;
    std::string linea = peticion.volcar(-1);
    // El volcado con sangria negativa ya es una sola linea; por si acaso:
    for (char& c : linea) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    linea += '\n';
    DWORD escritos = 0;
    if (!WriteFile(escritura_, linea.data(), (DWORD)linea.size(), &escritos, NULL)) return 0;
    return id;
}

bool Motor::vivo() const {
    return proceso_ && WaitForSingleObject(proceso_, 0) == WAIT_TIMEOUT;
}

void Motor::detener() {
    if (escritura_) {
        if (vivo()) {
            const char salir[] = "{\"cmd\": \"salir\"}\n";
            DWORD n = 0;
            WriteFile(escritura_, salir, sizeof(salir) - 1, &n, NULL);
        }
        CloseHandle(escritura_);
        escritura_ = NULL;
    }
    if (proceso_) {
        if (WaitForSingleObject(proceso_, 1500) == WAIT_TIMEOUT) TerminateProcess(proceso_, 1);
        CloseHandle(proceso_);
        proceso_ = NULL;
    }
    if (hilo_) {
        WaitForSingleObject(hilo_, 1000);
        CloseHandle(hilo_);
        hilo_ = NULL;
    }
    if (lectura_) {
        CloseHandle(lectura_);
        lectura_ = NULL;
    }
}

}  // namespace profesor
