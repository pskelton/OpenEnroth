#pragma once

#include "Library/Serialization/SerializationFwd.h"

enum class RandomEngineType {
    RANDOM_ENGINE_MERSENNE_TWISTER,
    RANDOM_ENGINE_SEQUENTIAL,
};
using enum RandomEngineType;
MM_DECLARE_SERIALIZATION_FUNCTIONS(RandomEngineType)
