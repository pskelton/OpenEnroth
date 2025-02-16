#pragma once

#include <string>

#include "Engine/Objects/ItemEnchantment.h"
#include "Engine/Objects/Items.h"

#include "Utility/IndexedArray.h"

class GameResourceManager;
class Blob;

struct BonusRange {
    unsigned int minR;
    unsigned int maxR;
}; // TODO(captainurist): Segment<int>?

struct ItemTable {
    void Initialize(GameResourceManager *resourceManager);
    void LoadPotions(const Blob &potions);
    void LoadPotionNotes(const Blob &potionNotes);

    /**
     * @offset 0x456620
     */
    void generateItem(ItemTreasureLevel treasureLevel, RandomItemType uTreasureType, ItemGen *pItem);
    void SetSpecialBonus(ItemGen *pItem);
    bool IsMaterialSpecial(const ItemGen *pItem);
    bool IsMaterialNonCommon(const ItemGen *pItem);

    /** Item data for all items in the game. */
    IndexedArray<ItemDesc, ITEM_FIRST_VALID, ITEM_LAST_VALID> items;

    /** Data for standard item enchantments. */
    IndexedArray<ItemEnchantmentTable, ATTRIBUTE_FIRST_ENCHANTABLE, ATTRIBUTE_LAST_ENCHANTABLE> standardEnchantments;

    /** Data for special item enchantments. */
    IndexedArray<ItemSpecialEnchantmentTable, ITEM_ENCHANTMENT_FIRST_VALID, ITEM_ENCHANTMENT_LAST_VALID> specialEnchantments;

    char field_9FC4[5000];
    char field_B348[5000];
    char field_C6D0[5000];
    char field_DA58[5000];
    char field_EDE0[384];

    /** Mapping `potion1 x potion2 => potion3`. Alchemy recipes, basically.
     *
     * `ITEM_NULL` means "can't mix even if you try", and is set for cases when `potion1 == potion2`. Item ids in
     * `[1, 4]` denote damage level from mixing. */
    IndexedArray<IndexedArray<ItemId, ITEM_FIRST_REAL_POTION, ITEM_LAST_REAL_POTION>, ITEM_FIRST_REAL_POTION, ITEM_LAST_REAL_POTION> potionCombination;

    /** Index of autonote bit (`Party::_autonoteBits`) for the potion recipe. */
    IndexedArray<IndexedArray<uint16_t, ITEM_FIRST_REAL_POTION, ITEM_LAST_REAL_POTION>, ITEM_FIRST_REAL_POTION, ITEM_LAST_REAL_POTION> potionNotes;

    /** Items have a per-treasure level chances to be randomly generated. This array is a per-treasure level sum of
     * these chances for all items. Effectively used for weighted random sampling in item generation. */
    IndexedArray<unsigned int, ITEM_TREASURE_LEVEL_FIRST_RANDOM, ITEM_TREASURE_LEVEL_LAST_RANDOM> itemChanceSumByTreasureLevel;

    /** Chance to get a standard enchantment on a non-weapon item, by treasure level. Number in `[0, 100]`. */
    IndexedArray<unsigned int, ITEM_TREASURE_LEVEL_FIRST_RANDOM, ITEM_TREASURE_LEVEL_LAST_RANDOM> standardEnchantmentChanceForEquipment;

    /** Chance to get an attribute enchantment OR a special enchantment on a non-weapon item, by treasure level.
     * Number in `[0, 100]`. This basically a cumulative chance to get some enchantment. */
    IndexedArray<unsigned int, ITEM_TREASURE_LEVEL_FIRST_RANDOM, ITEM_TREASURE_LEVEL_LAST_RANDOM> specialEnchantmentChanceForEquipment;

    /** Chance to get a special enchantment on a weapon, by treasure level. Number in `[0, 100]`. */
    IndexedArray<unsigned int, ITEM_TREASURE_LEVEL_FIRST_RANDOM, ITEM_TREASURE_LEVEL_LAST_RANDOM> specialEnchantmentChanceForWeapons;

    /** Standard enchantments have a per-item type chance to be randomly generated. This array is a per-item type
     * sum of these chances for all standard enchantments. Effectively used for weighted random sampling in standard
     * enchantment generation. */
    IndexedArray<unsigned int, ITEM_TYPE_FIRST_NORMAL_ENCHANTABLE, ITEM_TYPE_LAST_NORMAL_ENCHANTABLE> standardEnchantmentChanceSumByItemType;

    /** Ranges of standard enchantment strength by item treasure level. */
    IndexedArray<BonusRange, ITEM_TREASURE_LEVEL_FIRST_RANDOM, ITEM_TREASURE_LEVEL_LAST_RANDOM> standardEnchantmentRangeByTreasureLevel;
};

extern ItemTable *pItemTable;
