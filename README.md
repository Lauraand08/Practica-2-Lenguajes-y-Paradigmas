# Práctica 2 - Integrated Itinerary and Fare Engine

Universidad EAFIT, Lenguajes y Paradigmas de Computación (SI1002-7), 2026-2

## Integrantes

- Sophia Narvaez
- Laura Rodriguez

## Qué hace el programa?

Es el motor de tarifas del Metro del Valle de Aburrá hecho en C++17. Calcula cuánto paga cada pasajero según el modo de transporte que toma, el tipo de tarjeta que tiene (frecuente, estudiante, adulto mayor, discapacidad o portador) y la regla de integración: la primera validación abre una ventana de 90 minutos y los siguientes dos transbordos dentro de esa ventana salen gratis.

La idea del ejercicio es resolverlo con polimorfismo y no con una cadena de if. Así, si aparece un modo o un perfil nuevo, solo hay que escribir una clase más.

## Cómo compilarlo

```bash
g++ -std=c++17 -Wall -Wextra -o practice main.cpp
./practice
```

## Cómo está organizado el código

Todo está en `main.cpp`, dividido en las cuatro partes del enunciado.

**Parte 1, los modos de transporte.** `TransportMode` es una clase abstracta. `baseFare()` y `serviceType()` son virtuales puras porque la base no puede saber ni el precio ni el tipo. También tiene tres funciones (`allowsStudentFlatFare()`, `allowsIntegration()` y `opensIntegrationWindow()`) que devuelven `true` por defecto, y cada hija las cambia solo si le toca. `describe()` no es virtual: está escrita una sola vez en la base y llama a las virtuales de cada hija. El destructor sí es virtual. Las hijas son `MetroLine`, `TramLine`, `CableLine`, `TouristCable` (que hereda de `CableLine`) y `FeederRoute`.

**Parte 2, los pasajeros.** `Passenger` usa el idioma NVI. `fareOn()` es pública y no virtual: llama a `computeFare()`, que es protegida y virtual, y después aplica `std::max(0LL, ...)` para que nunca salga una tarifa negativa. Los perfiles que cambian la tarifa llaman a `Passenger::computeFare(mode)` para sacar la tarifa completa y a partir de ahí suman o restan. `FrequentPassenger` no sobrescribe nada.

**Parte 3, el itinerario.** `Network` guarda los modos en un `std::vector` de `std::unique_ptr`, así que ella es la dueña y `main` solo los observa. `chargeItinerary` recibe los tramos por valor, los ordena con `std::sort` y una lambda, y los recorre con el único bucle explícito del programa, aplicando las cláusulas de la regla de integración.

**Parte 4, los reportes.** Ninguno usa bucles. `totalPaid` usa `std::accumulate`, `freeTransferCount` usa `std::count_if`, `paidByServiceType` usa `std::for_each` con un `std::map` capturado por referencia, `namesMatching` recibe el criterio como `std::function` y `joined` usa `std::accumulate` sobre strings.

## Justificaciones de la sección 1

### Clase abstracta con virtuales puras frente a un enum con switch

Con un enum y un switch, la información de cada modo queda repartida por todo el programa, porque cada función que depende del tipo (tarifa, descripción, integración, tarifa de estudiante) necesita su propio switch. Para agregar un modo nuevo hay que abrir todos esos switch, y si se nos pasa uno, normalmente el compilador no avisa y el error aparece cuando ya está corriendo.

Con una clase abstracta el modo nuevo es una sola subclase que implementa `baseFare()` y `serviceType()`, y no se toca `Network`, `Passenger` ni `chargeItinerary`. Además, el compilador no deja instanciar una subclase a la que le falte una virtual pura, así que no se puede olvidar nada. Y el resto del código solo conoce la interfaz de `TransportMode`, no los tipos concretos.

### Por qué el destructor virtual es obligatorio

`Network` guarda punteros a la base (`unique_ptr<TransportMode>`) que en realidad apuntan a objetos derivados. Cuando la red se destruye, cada `unique_ptr` hace `delete` sobre un `TransportMode*`. Si el destructor no es virtual, ese `delete` es comportamiento indefinido y no hay garantía de que corra el destructor de la clase derivada. Por eso es un tema de que el programa sea correcto y no de estilo. Lo que pasa exactamente se explica en la pregunta 1 de la defensa.

### Qué gana uno con la STL frente a un bucle hecho a mano

Un algoritmo dice lo que hace con solo leer el nombre: `accumulate` suma, `count_if` cuenta y `for_each` aplica algo a cada elemento. Con eso se evitan los índices, los iteradores manuales y los errores de límites que pasan con los bucles. Además, lo que cambia de un caso a otro se pasa como lambda o `std::function`, y por eso `namesMatching` puede recibir el criterio de quien la llama sin que la red lo conozca. Y sirve con cualquier contenedor que tenga iteradores.

## Preguntas de defensa

### Pregunta 1: ¿qué pasa si el destructor de `TransportMode` no es virtual?

La red guarda `unique_ptr<TransportMode>`, o sea punteros a la base que apuntan a objetos como `MetroLine` o `TouristCable`. Cuando se destruyen, se hace `delete` sobre el puntero de la base. Si el destructor no es virtual, eso es comportamiento indefinido según el estándar. En la práctica, solo corre el destructor de `TransportMode` y el de la clase derivada nunca se ejecuta, así que lo que haya reservado la hija no se libera bien.

En este programa no se notaría porque las hijas solo agregan un `long long`, pero si una guardara un `std::string` o memoria dinámica habría una fuga. Con `virtual ~TransportMode() = default;` el `delete` llama primero al destructor de la clase derivada que corresponde y después al de la base.

### Pregunta 2

La segunda pregunta de defensa no está especificada en el documento del enunciado.
