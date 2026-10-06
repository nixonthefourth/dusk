//
// Fuel tank modules: how big the ship's tank is. Like the cargo bays, a better module replaces a
// worse one rather than stacking. Fuel itself (how much is aboard, what it costs, how it's burned)
// lives on the Ship and in systems/refuelling.h++.
//

#ifndef DUSK_FUEL_TANK_H
#define DUSK_FUEL_TANK_H

/** Tank size of the standard ship, with no module fitted, in tonnes. */
constexpr float baseFuelCapacity = 6.f;

/** The fuel tank module fitted to the ship. A Mk2 tank is a bigger tank, not a second one. */
enum class FuelTankModule { None = 0, Mk1 = 1, Mk2 = 2 };

/** Extra tank space a module adds: Mk1 +4 t, Mk2 +8 t. */
inline float fuelTankBonus(FuelTankModule module)
{
    switch (module)
    {
        case FuelTankModule::None: return 0.f;
        case FuelTankModule::Mk1: return 4.f;
        case FuelTankModule::Mk2: return 8.f;
    }

    return 0.f;
}

/** Total tank size with a module fitted: 6 t standard, 10 t with Mk1, 14 t with Mk2. */
inline float fuelCapacityFor(FuelTankModule module)
{
    return baseFuelCapacity + fuelTankBonus(module);
}

/** Display name of a module ("STANDARD TANK", "FUEL TANK MK1", ...). */
inline const char* fuelTankName(FuelTankModule module)
{
    switch (module)
    {
        case FuelTankModule::None: return "STANDARD TANK";
        case FuelTankModule::Mk1: return "FUEL TANK MK1";
        case FuelTankModule::Mk2: return "FUEL TANK MK2";
    }

    return "";
}

/** Converts a saved integer to a module, treating anything unknown as no module. */
inline FuelTankModule fuelTankFromInt(int value)
{
    return value >= 2 ? FuelTankModule::Mk2 : (value == 1 ? FuelTankModule::Mk1 : FuelTankModule::None);
}

#endif //DUSK_FUEL_TANK_H
