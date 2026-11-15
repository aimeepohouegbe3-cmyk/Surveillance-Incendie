#include <stddef.h>
#include "sensors.h"

struct mesures read_simulated_sensors(int scenario)
{
    struct mesures m = {0};

    /* Valeurs normales */
    m.temperature = 25.0f;
    m.humidity = 50.0f;
    m.smoke_value = 100;
    m.flame_detected = 0;
    m.sensor_status = CAPTEURS_TOUS_OK;

    if (scenario == 0)
    {
        /* Fonctionnement normal */
    }
    else if (scenario == 1)
    {
        /* Simulation d'une anomalie */
        m.temperature = 65.0f;
        m.humidity = 25.0f;
        m.smoke_value = 850;
        m.flame_detected = 1;
    }
    else if (scenario == 2)
    {
        /* Simulation d'une absence du DHT22 */
        m.sensor_status &= ~CAPTEUR_TEMP_OK;
    }
    else
    {
        /* Tous les capteurs sont indisponibles */
        m.sensor_status = 0;
    }

    return m;
}
