#pragma once

#include <cstdint>
#include <compare>
#include <type_traits>

struct CivilDuration {
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
};

struct LongCivilDuration {
    int years = 0;
    int months = 0;
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
};

class Duration {
 public:
    static constexpr int64_t TICKS_PER_REALTIME_SECOND = 128;
    static constexpr int64_t GAME_SECONDS_IN_REALTIME_SECOND = 30; // Game time runs 30x faster than real time.

    constexpr Duration() = default;
    constexpr Duration(int seconds, int minutes, int hours, int days, int weeks, int months, int years) {
        value = seconds + 60ll * minutes + 3600ll * hours + 86400ll * days + 604800ll * weeks + 2419200ll * months + 29030400ll * years;
        value = value * TICKS_PER_REALTIME_SECOND / GAME_SECONDS_IN_REALTIME_SECOND;
    }

    [[nodiscard]] constexpr static Duration fromTicks(int64_t ticks) {
        Duration result;
        result.value = ticks;
        return result;
    }

    [[nodiscard]] constexpr static Duration fromSeconds(int seconds) { return Duration(seconds, 0, 0, 0, 0, 0, 0); }
    [[nodiscard]] constexpr static Duration fromMinutes(int minutes) { return Duration(0, minutes, 0, 0, 0, 0, 0); }
    [[nodiscard]] constexpr static Duration fromHours(int hours) { return Duration(0, 0, hours, 0, 0, 0, 0); }
    [[nodiscard]] constexpr static Duration fromDays(int days) { return Duration(0, 0, 0, days, 0, 0, 0); }
    [[nodiscard]] constexpr static Duration fromMonths(int months) { return Duration(0, 0, 0, 0, 0, months, 0); }
    [[nodiscard]] constexpr static Duration fromYears(int years) { return Duration(0, 0, 0, 0, 0, 0, years); }

    [[nodiscard]] constexpr int64_t ticks() const { return value; }
    [[nodiscard]] constexpr int64_t toSeconds() const { return value * GAME_SECONDS_IN_REALTIME_SECOND / TICKS_PER_REALTIME_SECOND; }
    [[nodiscard]] constexpr int64_t toMinutes() const { return toSeconds() / 60; }
    [[nodiscard]] constexpr int64_t toHours() const { return toMinutes() / 60; }
    [[nodiscard]] constexpr int toDays() const { return toHours() / 24; }
    [[nodiscard]] constexpr int toWeeks() const { return toDays() / 7; }
    [[nodiscard]] constexpr int toMonths() const { return toWeeks() / 4; }
    [[nodiscard]] constexpr int toYears() const { return toMonths() / 12; }

    [[nodiscard]] constexpr static Duration fromRealtimeSeconds(int64_t seconds) { return fromTicks(seconds * TICKS_PER_REALTIME_SECOND); }
    [[nodiscard]] constexpr static Duration fromRealtimeMilliseconds(int64_t msec) { return fromTicks(msec * TICKS_PER_REALTIME_SECOND / 1000); }

    [[nodiscard]] constexpr int64_t toRealtimeSeconds() const { return ticks() / TICKS_PER_REALTIME_SECOND; }
    [[nodiscard]] constexpr int64_t toRealtimeMilliseconds() const { return ticks() * 1000 / TICKS_PER_REALTIME_SECOND; }

    [[nodiscard]] constexpr CivilDuration toCivilDuration() const {
        CivilDuration result;
        result.days = toDays();
        result.hours = toHours() % 24;
        result.minutes = toMinutes() % 60;
        result.seconds = toSeconds() % 60;
        return result;
    };

    [[nodiscard]] constexpr LongCivilDuration toLongCivilDuration() const {
        LongCivilDuration result;
        result.years = toYears();
        result.months = toMonths() % 12;
        result.days = toDays() % 28;
        result.hours = toHours() % 24;
        result.minutes = toMinutes() % 60;
        result.seconds = toSeconds() % 60;
        return result;
    }

    [[nodiscard]] constexpr friend Duration operator+(const Duration &l, const Duration &r) {
        return Duration::fromTicks(l.value + r.value);
    }

    [[nodiscard]] constexpr friend Duration operator-(const Duration &l, const Duration &r) {
        return Duration::fromTicks(l.value - r.value);
    }

    template<class L> requires std::is_arithmetic_v<L>
    [[nodiscard]] constexpr friend Duration operator*(L l, const Duration &r) {
        return Duration::fromTicks(l * r.value);
    }

    template<class R> requires std::is_arithmetic_v<R>
    [[nodiscard]] constexpr friend Duration operator*(const Duration &l, R r) {
        return Duration::fromTicks(l.value * r);
    }

    [[nodiscard]] constexpr friend Duration operator%(const Duration &l, const Duration &r) {
        return Duration::fromTicks(l.value % r.value);
    }

    constexpr Duration &operator+=(const Duration &rhs) {
        value += rhs.value;
        return *this;
    }

    constexpr Duration &operator-=(const Duration &rhs) {
        value -= rhs.value;
        return *this;
    }

    template<class R> requires std::is_arithmetic_v<R>
    constexpr Duration &operator*=(R r) {
        value *= r;
        return *this;
    }

    [[nodiscard]] constexpr friend bool operator==(const Duration &l, const Duration &r) = default;
    [[nodiscard]] constexpr friend auto operator<=>(const Duration &l, const Duration &r) = default;

    [[nodiscard]] constexpr explicit operator bool() const {
        return value != 0;
    }

    [[nodiscard]] constexpr static Duration zero() {
        return {};
    }

 private:
    int64_t value = 0;
};