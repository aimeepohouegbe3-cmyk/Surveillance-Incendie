/* alert.h — interface du processus P3 (alerte et journalisation) */
#ifndef ALERT_H
#define ALERT_H

#include "shared_data.h"

/* Boucle principale de P3. Revient quand SIGTERM ou SIGINT est reçu.
 * Attend une notification via shm->notif_p3, lit l'état sous mutex,
 * affiche la signalisation en console et écrit dans fichier_log. */
void processus_alerte(struct shared_data *shm, const char *fichier_log);

#endif
