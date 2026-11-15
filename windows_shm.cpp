#include <windows.h>
#include <iostream>
#include <cstdlib>
#include <ctime>

#define SHM_NAME "Local\\SO_MemoriePartajata"
#define SEM_NAME "Local\\SO_SemaforBinar"
#define TARGET 1000

int main() {
    DWORD pid = GetCurrentProcessId();
    srand((unsigned int)(time(NULL) ^ pid));

    // 1. Creare / deschidere semafor numit (inițial 1, maxim 1)
    HANDLE hSem = CreateSemaphoreA(NULL, 1, 1, SEM_NAME);
    if (hSem == NULL) {
        std::cerr << "Eroare CreateSemaphore: " << GetLastError() << "\n";
        return 1;
    }

    // 2. Creare / deschidere obiect mapat în memorie (File Mapping backed by paging file)
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // Folosește memoria RAM / paging file
        NULL,                    // Securitate implicită
        PAGE_READWRITE,          // Acces citire/scriere
        0,                       // Dimensiune maximă (high-order DWORD)
        sizeof(int),             // Dimensiune maximă (low-order DWORD)
        SHM_NAME                 // Numele obiectului partajat
    );

    if (hMapFile == NULL) {
        std::cerr << "Eroare CreateFileMapping: " << GetLastError() << "\n";
        CloseHandle(hSem);
        return 1;
    }

    // Verificăm dacă noi am creat obiectul primii pentru a-l inițializa cu 0
    bool first_process = (GetLastError() != ERROR_ALREADY_EXISTS);

    // Mapăm zona de memorie în spațiul procesului
    int* shared_counter = (int*)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(int)
    );

    if (shared_counter == NULL) {
        std::cerr << "Eroare MapViewOfFile: " << GetLastError() << "\n";
        CloseHandle(hMapFile);
        CloseHandle(hSem);
        return 1;
    }

    if (first_process) {
        *shared_counter = 0;
    }

    std::cout << "[PID " << pid << "] Proces Windows pornit...\n";

    while (true) {
        // Rezervă semaforul
        WaitForSingleObject(hSem, INFINITE);

        // Citește memoria
        int current = *shared_counter;

        if (current >= TARGET) {
            ReleaseSemaphore(hSem, 1, NULL);
            break;
        }

        // Dă cu banul: random(2) -> 1 sau 2
        int coin = (rand() % 2) + 1;

        // Cât timp cade 2, scrie în memorie următorul număr
        while (coin == 2 && *shared_counter < TARGET) {
            (*shared_counter)++;
            std::cout << "[PID " << pid << "] A dat 2 -> a scris: " << *shared_counter << "\n";
            coin = (rand() % 2) + 1;
        }

        if (*shared_counter < TARGET) {
            std::cout << "[PID " << pid << "] A dat " << coin << " -> elibereaza semaforul (curent: " << *shared_counter << ")\n";
        }

        // Eliberează semaforul
        ReleaseSemaphore(hSem, 1, NULL);

        Sleep(1); // 1 ms pauză pentru alternanța proceselor
    }

    std::cout << "[PID " << pid << "] Finalizat! Valoare finala: " << *shared_counter << "\n";

    // Curățare handle-uri (Windows distruge automat obiectele când ultimul handle se închide)
    UnmapViewOfFile(shared_counter);
    CloseHandle(hMapFile);
    CloseHandle(hSem);

    return 0;
}