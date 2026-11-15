#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

#define SHM_NAME "/so_memorie_partajata"
#define SEM_NAME "/so_semafor_binar"
#define TARGET 1000

int main() {
    srand(time(NULL) ^ getpid());

    sem_t* sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("Eroare sem_open");
        return 1;
    }

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Eroare shm_open");
        return 1;
    }

    if (ftruncate(shm_fd, sizeof(int)) == -1) {
        perror("Eroare ftruncate");
        return 1;
    }

    int* shared_counter = (int*)mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_counter == MAP_FAILED) {
        perror("Eroare mmap");
        return 1;
    }

    pid_t pid = getpid();
    std::cout << "[PID " << pid << "] Proces pornit. Se asteapta accesul la memorie...\n";

    bool is_creator = false;

    while (true) {
        sem_wait(sem);
        int current = *shared_counter;

        if (current >= TARGET) {
            sem_post(sem);
            break;
        }

        int coin = (rand() % 2) + 1;

        while (coin == 2 && *shared_counter < TARGET) {
            (*shared_counter)++;
            std::cout << "[PID " << pid << "] A dat 2 -> a scris: " << *shared_counter << "\n";
            coin = (rand() % 2) + 1;
        }

        if (*shared_counter < TARGET) {
            std::cout << "[PID " << pid << "] A dat " << coin << " -> elibereaza semaforul (curent: " << *shared_counter << ")\n";
        } else {
            is_creator = true;
        }

        sem_post(sem);
        usleep(1000);
    }

    std::cout << "[PID " << pid << "] Finalizat! Valoare finala in memorie: " << *shared_counter << "\n";

    munmap(shared_counter, sizeof(int));
    close(shm_fd);
    sem_close(sem);

    if (is_creator) {
        sleep(1);
        shm_unlink(SHM_NAME);
        sem_unlink(SEM_NAME);
        std::cout << "[PID " << pid << "] Resursele sistemului (SHM & SEM) au fost curatate.\n";
    }

    return 0;
}