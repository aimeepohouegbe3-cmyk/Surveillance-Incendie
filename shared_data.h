/* shared_data.h — structure partagée entre le superviseur, P1, P2 et P3.
 * PROPOSITION : à valider avec toute l'équipe avant de coder.
 */
#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <semaphore.h>
#include <time.h>

/* États du système */
enum etat_systeme {
    ETAT_NORMAL    = 0,
    ETAT_ATTENTION = 1,
    ETAT_ALERTE    = 2,
    ETAT_DEFAUT    = 3   /* DÉFAUT CAPTEUR */
};

/* Bits de sensor_status : 1 = capteur valide (mis par P1) */
#define CAPTEUR_TEMP_OK    0x1u   /* DHT22 (température + humidité) */
#define CAPTEUR_FUMEE_OK   0x2u   /* MQ-2 via ADC */
#define CAPTEUR_FLAMME_OK  0x4u   /* capteur optique de flamme */
#define CAPTEURS_TOUS_OK   (CAPTEUR_TEMP_OK | CAPTEUR_FUMEE_OK | CAPTEUR_FLAMME_OK)

/* Seuils de DÉMONSTRATION uniquement (pas des seuils de sécurité) */
#define TEMP_ATTENTION     45.0f  /* °C */
#define FUMEE_ATTENTION    400    /* valeur brute ADC */
#define FUMEE_MAX          1023   /* ADC 10 bits (MCP3008) */
#define DELAI_PERIME_S     5.0    /* au-delà, la mesure est trop vieille */
#define NB_CONFIRMATION    3      /* mesures identiques avant de changer d'état */

/* Une mesure publiée par P1 */
struct mesures {
    unsigned long   sequence;       /* +1 à chaque publication, 0 = aucune mesure */
    struct timespec timestamp;      /* clock_gettime(CLOCK_MONOTONIC, ...) */
    float           temperature;    /* °C */
    float           humidity;       /* % */
    int             smoke_value;    /* valeur brute 0..FUMEE_MAX */
    int             flame_detected; /* 0 ou 1 */
    unsigned int    sensor_status;  /* bits CAPTEUR_*_OK */
};

/* Zone partagée créée par le superviseur avec mmap() */
struct shared_data {
    sem_t          mutex;        /* protège tout le reste, initialisé à 1 */
    sem_t          notif_p3;     /* P2 -> P3 : l'état a changé, initialisé à 0 */
    struct mesures mesures;      /* écrit par P1 */
    int            system_state; /* écrit par P2, lu par P3 */
};

#endif
