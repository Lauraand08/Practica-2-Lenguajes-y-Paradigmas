/* =========================================================================
   Integrantes: Sophia Narvaez y Laura Rodriguez
   =========================================================================

   PRACTICE - Integrated Itinerary and Fare Engine - Medellin Metro System
   Programming Languages and Paradigms - Universidad EAFIT

   Build: g++ -std=c++17 -Wall -Wextra -o practice main.cpp
   ========================================================================= */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>
#include <map>
#include <functional>

/* -------------------------------------------------------------------------
   Utility: Colombian peso formatting (3820 -> "$ 3.820")
   ------------------------------------------------------------------------- */
std::string formatCOP(long long value) {
    std::string digits = std::to_string(value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        out.push_back(*it);
        if (++count % 3 == 0 && it + 1 != digits.rend()) {
            out.push_back('.');
        }
    }
    std::reverse(out.begin(), out.end());
    return "$ " + out;
}

/* -------------------------------------------------------------------------
   PART 1. Abstract base class of the transport-mode hierarchy
   ------------------------------------------------------------------------- */
class TransportMode {
public:
    TransportMode(std::string name, int stations)
        : name_(std::move(name)), stations_(stations) {}

    virtual ~TransportMode() = default;          // mandatory: deleted through base pointer

    const std::string& name() const { return name_; }
    int stations() const { return stations_; }

    // TODO 1.1: la clase base no sabe cuanto cuesta ni que tipo es, por eso son virtuales puras
    virtual long long baseFare() const = 0;
    virtual std::string serviceType() const = 0;

    // TODO 1.2: comportamiento por defecto (true); cada hija lo cambia solo si es distinta
    virtual bool allowsStudentFlatFare() const { return true; }
    virtual bool allowsIntegration() const { return true; }
    virtual bool opensIntegrationWindow() const { return true; }

    // TODO 1.3: metodo plantilla NO virtual; llama a los virtuales puros de cada hija
    std::string describe() const {
        return serviceType() + " " + name_ + " (" + std::to_string(stations_) +
               " stations) - full fare " + formatCOP(baseFare());
    }

private:
    std::string name_;
    int stations_;
};

// TODO 1.4: los cinco modos concretos
class MetroLine : public TransportMode {
public:
    using TransportMode::TransportMode;   // hereda el constructor de la clase base
    long long baseFare() const override { return 3820; }
    std::string serviceType() const override { return "Metro line"; }
};

class TramLine : public TransportMode {
public:
    using TransportMode::TransportMode;
    long long baseFare() const override { return 3820; }
    std::string serviceType() const override { return "Tram line"; }
};

class CableLine : public TransportMode {
public:
    using TransportMode::TransportMode;
    long long baseFare() const override { return 3820; }
    std::string serviceType() const override { return "Cable line"; }
};

class TouristCable : public CableLine {
public:
    TouristCable(std::string name, int stations, long long surcharge)
        : CableLine(std::move(name), stations), surcharge_(surcharge) {}

    // Reutiliza la tarifa de la clase padre con una llamada calificada (sin repetir el literal)
    long long baseFare() const override { return CableLine::baseFare() + surcharge_; }
    std::string serviceType() const override { return "Tourist cable"; }

    // Cambia los tres comportamientos por defecto: no tarifa de estudiante, no integra, no abre ventana
    bool allowsStudentFlatFare() const override { return false; }
    bool allowsIntegration() const override { return false; }
    bool opensIntegrationWindow() const override { return false; }

private:
    long long surcharge_;   // recargo recibido por el constructor
};

class FeederRoute : public TransportMode {
public:
    using TransportMode::TransportMode;
    long long baseFare() const override { return 0; }   // gratis: la tarifa troncal ya lo cubre
    std::string serviceType() const override { return "Feeder route"; }
    bool opensIntegrationWindow() const override { return false; }   // no abre ventana (clausula d)
};

/* -------------------------------------------------------------------------
   PART 2. Passenger hierarchy, Non-Virtual Interface idiom
   ------------------------------------------------------------------------- */
// TODO 2.1: clase base de los pasajeros (idioma NVI)
class Passenger {
public:
    explicit Passenger(std::string name) : name_(std::move(name)) {}
    virtual ~Passenger() = default;

    const std::string& name() const { return name_; }
    virtual std::string profile() const = 0;   // cada perfil dice como se llama

    // NO es virtual: aqui se aplica el limite (nunca negativo), una sola vez para todos los perfiles
    long long fareOn(const TransportMode& mode) const {
        return std::max(0LL, computeFare(mode));
    }

protected:
    // Comportamiento por defecto: la tarifa completa del modo
    virtual long long computeFare(const TransportMode& mode) const {
        return mode.baseFare();
    }

private:
    std::string name_;
};

// TODO 2.2: los cinco perfiles
class FrequentPassenger : public Passenger {
public:
    using Passenger::Passenger;
    std::string profile() const override { return "Civica frequent"; }
    // No sobrescribe computeFare: hereda el comportamiento por defecto
};

class StudentPassenger : public Passenger {
public:
    using Passenger::Passenger;
    std::string profile() const override { return "Municipal student"; }

protected:
    long long computeFare(const TransportMode& mode) const override {
        if (mode.allowsStudentFlatFare()) return 1600;   // pregunta la politica al modo, no su clase
        return Passenger::computeFare(mode);             // llamada calificada a la base (el "super" de C++)
    }
};

class SeniorPassenger : public Passenger {
public:
    using Passenger::Passenger;
    std::string profile() const override { return "Senior citizen"; }

protected:
    long long computeFare(const TransportMode& mode) const override {
        return Passenger::computeFare(mode) - 490;
    }
};

class DisabilityPassenger : public Passenger {
public:
    using Passenger::Passenger;
    std::string profile() const override { return "Person with disability"; }

protected:
    long long computeFare(const TransportMode& mode) const override {
        return Passenger::computeFare(mode) - 1100;
    }
};

class BearerPassenger : public Passenger {
public:
    using Passenger::Passenger;
    std::string profile() const override { return "Bearer (occasional)"; }

protected:
    long long computeFare(const TransportMode& mode) const override {
        return Passenger::computeFare(mode) + 580;
    }
};

/* -------------------------------------------------------------------------
   PART 3. The itinerary and the integration rule
   ------------------------------------------------------------------------- */
struct Leg {
    int minute;                   // minutes elapsed since the journey started
    const TransportMode* mode;    // non-owning observer pointer
};

struct ChargedLeg {
    int minute;
    std::string modeName;
    std::string serviceType;
    long long charge;
    bool integrated;              // true: free transfer inside the window
};

// TODO 3.1: la red es duena de sus modos (unique_ptr); main solo los observa
class Network {
    std::vector<std::unique_ptr<TransportMode>> modes_;

public:
    // Guarda el modo y devuelve una referencia al objeto guardado
    TransportMode& add(std::unique_ptr<TransportMode> mode) {
        modes_.push_back(std::move(mode));
        return *modes_.back();
    }

    const std::vector<std::unique_ptr<TransportMode>>& modes() const { return modes_; }

    // Una linea por modo, sin bucle explicito (solo se permite uno en todo el programa)
    void printInventory() const {
        std::for_each(modes_.begin(), modes_.end(),
            [](const std::unique_ptr<TransportMode>& m) {
                std::cout << "  - " << m->describe() << '\n';
            });
    }
};

// TODO 3.2: aplica la regla de integracion
std::vector<ChargedLeg> chargeItinerary(std::vector<Leg> legs,
                                        const Passenger& passenger) {
    constexpr int windowMinutes = 90;      // duracion de la ventana
    constexpr int maxFreeTransfers = 2;    // transbordos gratis permitidos

    // Ordena los tramos por minuto (llegan desordenados desde main)
    std::sort(legs.begin(), legs.end(),
        [](const Leg& a, const Leg& b) { return a.minute < b.minute; });

    std::vector<ChargedLeg> result;
    bool windowOpen = false;   // hay una ventana abierta?
    int windowStart = 0;       // minuto en que se abrio
    int freeUsed = 0;          // transbordos gratis ya usados en esta ventana

    // unico bucle explicito del programa: es el algoritmo en si
    for (const Leg& leg : legs) {
        const TransportMode& mode = *leg.mode;
        const long long ownFare = passenger.fareOn(mode);

        // Clausulas c) y d): paga su tarifa y no toca la ventana (ni la abre ni la reinicia)
        if (!mode.allowsIntegration() || !mode.opensIntegrationWindow()) {
            result.push_back({leg.minute, mode.name(), mode.serviceType(), ownFare, false});
            continue;
        }

        // Cae dentro de la ventana y aun quedan transbordos gratis?
        const bool insideWindow = windowOpen &&
                                  (leg.minute - windowStart) <= windowMinutes &&
                                  freeUsed < maxFreeTransfers;

        if (insideWindow) {                                  // clausula b): transbordo gratis
            ++freeUsed;
            result.push_back({leg.minute, mode.name(), mode.serviceType(), 0, true});
        } else {                                             // clausulas a) y e): paga y abre ventana nueva
            windowOpen = true;
            windowStart = leg.minute;
            freeUsed = 0;
            result.push_back({leg.minute, mode.name(), mode.serviceType(), ownFare, false});
        }
    }
    return result;
}

/* -------------------------------------------------------------------------
   PART 4. Reports built exclusively with STL algorithms and lambdas
   ------------------------------------------------------------------------- */
// TODO 4.1: total pagado (suma de todos los cobros)
long long totalPaid(const std::vector<ChargedLeg>& legs) {
    return std::accumulate(legs.begin(), legs.end(), 0LL,
        [](long long acc, const ChargedLeg& c) { return acc + c.charge; });
}

// TODO 4.2: cantidad de transbordos gratis
long long freeTransferCount(const std::vector<ChargedLeg>& legs) {
    return std::count_if(legs.begin(), legs.end(),
        [](const ChargedLeg& c) { return c.integrated; });
}

// TODO 4.3: monto pagado por tipo de servicio (el map se captura por referencia)
std::map<std::string, long long> paidByServiceType(const std::vector<ChargedLeg>& legs) {
    std::map<std::string, long long> totals;
    std::for_each(legs.begin(), legs.end(),
        [&totals](const ChargedLeg& c) { totals[c.serviceType] += c.charge; });
    return totals;
}

// TODO 4.4: nombres de los modos que cumplen un criterio que inyecta quien llama
std::vector<std::string> namesMatching(
        const Network& network,
        const std::function<bool(const TransportMode&)>& policy) {
    std::vector<std::string> names;
    std::for_each(network.modes().begin(), network.modes().end(),
        [&names, &policy](const std::unique_ptr<TransportMode>& m) {
            if (policy(*m)) names.push_back(m->name());
        });
    return names;
}

// TODO 4.5: une los textos con ", " (el primero es el valor inicial para no dejar coma al comienzo)
std::string joined(const std::vector<std::string>& items) {
    if (items.empty()) return "";
    return std::accumulate(items.begin() + 1, items.end(), items.front(),
        [](std::string acc, const std::string& s) { return std::move(acc) + ", " + s; });
}

/* -------------------------------------------------------------------------
   MAIN
   ------------------------------------------------------------------------- */
int main() {
    std::cout << "============================================================\n";
    std::cout << "   INTEGRATED ITINERARY AND FARE ENGINE - Valle de Aburra\n";
    std::cout << "============================================================\n\n";

    Network network;
    TransportMode& lineA  = network.add(std::make_unique<MetroLine>("A", 21));
    TransportMode& lineB  = network.add(std::make_unique<MetroLine>("B", 7));
    TransportMode& tramTA = network.add(std::make_unique<TramLine>("T-A Ayacucho", 9));
    TransportMode& cableK = network.add(std::make_unique<CableLine>("K Santo Domingo", 4));
    TransportMode& arvi   = network.add(std::make_unique<TouristCable>("L Arvi", 2, 9180));
    TransportMode& feeder = network.add(std::make_unique<FeederRoute>("130 Robledo", 18));

    std::cout << ">> 1. NETWORK INVENTORY (polymorphic describe)\n";
    network.printInventory();
    std::cout << '\n';

    /* Itinerary deliberately given out of chronological order, so that the
       sort step is observable. */
    const std::vector<Leg> itinerary = {
        {45,  &cableK},
        {0,   &feeder},
        {12,  &lineA},
        {150, &lineB},
        {70,  &tramTA},
        {200, &arvi}
    };

    const FrequentPassenger frequent("Diego");
    const StudentPassenger  student("Valentina");
    const SeniorPassenger   senior("Dona Amparo");

    std::cout << ">> 2. FULL FARE PER PROFILE ON METRO LINE A\n";
    std::cout << "  " << frequent.profile() << " pays " << formatCOP(frequent.fareOn(lineA)) << '\n';
    std::cout << "  " << student.profile()  << " pays " << formatCOP(student.fareOn(lineA))  << '\n';
    std::cout << "  " << senior.profile()   << " pays " << formatCOP(senior.fareOn(lineA))   << '\n';
    std::cout << "  " << student.profile()  << " on the tourist cable pays "
              << formatCOP(student.fareOn(arvi)) << " (no flat fare there)\n\n";

    const std::vector<ChargedLeg> charged = chargeItinerary(itinerary, frequent);

    std::cout << ">> 3. CHARGED ITINERARY (frequent profile)\n";
    std::for_each(charged.begin(), charged.end(), [](const ChargedLeg& c) {
        std::cout << "  min " << c.minute << " | " << c.serviceType << " " << c.modeName
                  << " | " << formatCOP(c.charge)
                  << (c.integrated ? "  [free transfer]" : "") << '\n';
    });
    std::cout << '\n';

    std::cout << ">> 4. REPORTS BUILT WITH STL ALGORITHMS\n";
    std::cout << "  accumulate: total paid -> " << formatCOP(totalPaid(charged)) << '\n';
    std::cout << "  count_if: free transfers -> " << freeTransferCount(charged) << '\n';

    const auto byType = paidByServiceType(charged);
    std::for_each(byType.begin(), byType.end(),
        [](const std::pair<const std::string, long long>& entry) {
            std::cout << "  map: paid on " << entry.first << " -> "
                      << formatCOP(entry.second) << '\n';
        });

    const auto firstPaid = std::find_if(charged.begin(), charged.end(),
        [](const ChargedLeg& c) { return c.charge > 0; });
    std::cout << "  find_if: first leg that actually paid -> "
              << (firstPaid == charged.end() ? std::string("none") : firstPaid->modeName) << '\n';

    const std::function<bool(const TransportMode&)> bigModes =
        [](const TransportMode& m) { return m.stations() > 8; };
    std::cout << "  injected policy: modes with more than 8 stations -> "
              << joined(namesMatching(network, bigModes)) << '\n';

    std::cout << "  total for the student on the same itinerary -> "
              << formatCOP(totalPaid(chargeItinerary(itinerary, student))) << '\n';
    std::cout << "  total for the senior on the same itinerary -> "
              << formatCOP(totalPaid(chargeItinerary(itinerary, senior))) << '\n';

    std::cout << "\n============================================================\n";
    std::cout << "  End of demonstration.\n";
    std::cout << "============================================================\n";
    return 0;
}
