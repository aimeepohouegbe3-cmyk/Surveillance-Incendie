
/* shared_data.h — Données partagées entre les processus */

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <semaphore.h>
#include <time.h>

/* États du système */
enum etat_systeme {
    ETAT_NORMAL    = 0,
    ETAT_ATTENTION = 1,
    ETAT_ALERTE    = 2,
    ETAT_DEFAUT    = 3
};

/* Bits de validité des capteurs */
#define CAPTEUR_TEMP_OK    0x1u
#define CAPTEUR_FUMEE_OK   0x2u
#define CAPTEUR_FLAMME_OK  0x4u

#define CAPTEURS_TOUS_OK \
    (CAPTEUR_TEMP_OK | CAPTEUR_FUMEE_OK | CAPTEUR_FLAMME_OK)

/* Seuils de démonstration */
#define TEMP_ATTENTION     45.0f
#define FUMEE_ATTENTION    400
#define FUMEE_MAX          1023
#define DELAI_PERIME_S     5.0
#define NB_CONFIRMATION    3

/* Mesures produites par P1 */
struct mesures {
    unsigned long sequence;
    struct timespec timestamp;
    float temperature;
    float humidity;
    int smoke_value;
    int flame_detected;
    unsigned int sensor_status;
};

/* Mémoire partagée */
struct shared_data {
    sem_t mutex;
    sem_t notif_p3;

    struct mesures mesures;
    int system_state;

    /* Dernière séquence analysée par P2 */
    unsigned long analysed_sequence;
};

#endif

