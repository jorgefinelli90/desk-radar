#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Candado de los datos que cruzan entre la tarea de red (core 0) y el loop
// principal (core 1): la lista de aviones, los titulares y el clima.
//
// La regla que hace que esto no arruine lo que ARQ-2 vino a arreglar: se toma
// SOLO para publicar o leer un resultado ya armado, nunca durante una request.
// Publicar es un swap de vector; leer es dibujar una pantalla. Microsegundos y
// milisegundos, no los segundos que tarda un fetch.
//
// Vive en un módulo aparte y no dentro de NetTask para que los clientes de
// services/ puedan publicar sus datos sin tener que conocer la tarea que los
// llama.
namespace DataLock {

// Llamar una vez en setup(), antes de arrancar la tarea de red.
void begin();

// Antes de begin() no hacen nada: hasta que la tarea existe no hay con quién
// competir, y así el firmware sigue funcionando igual si alguna vez se saca la
// tarea del medio.
void lock();
void unlock();

// RAII, para no perder un unlock en un return temprano.
struct Guard {
  Guard()  { lock(); }
  ~Guard() { unlock(); }

  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};

} // namespace DataLock
