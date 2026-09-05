#pragma once
#include "core/DisplayManager.h"
#include "services/NewsClient.h"

// La noticia abierta desde la lista: el titular completo, el medio, la hora y
// el resumen que manda GNews.
//
// En la lista el titular entra en dos renglones y lo que sobra se corta con
// "..."; acá hay pantalla entera, así que se lee todo. Es la misma idea que la
// ficha de un avión: se toca algo de una lista y se ve el detalle.
class NewsDetailScreen {
  public:
    explicit NewsDetailScreen(DisplayManager& display) : _display(display) {}

    // El item se recibe por copia desde main, no por referencia al cliente: la
    // tarea de red puede republicar los titulares mientras estás leyendo uno.
    void render(const NewsItem& item);

  private:
    DisplayManager& _display;
};
