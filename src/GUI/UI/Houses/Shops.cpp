#include <cstdlib>
#include <algorithm>
#include <string>

#include "Engine/Engine.h"
#include "Engine/Graphics/IRender.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/LocationFunctions.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Localization.h"
#include "Engine/MapInfo.h"
#include "Engine/Objects/Items.h"
#include "Engine/Party.h"
#include "Engine/PriceCalculator.h"
#include "Engine/Tables/AwardTable.h"
#include "Engine/Tables/MerchantTable.h"

#include "GUI/GUIWindow.h"
#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIStatusBar.h"
#include "GUI/UI/UIPopup.h"
#include "GUI/UI/Houses/Shops.h"

#include "Io/Mouse.h"

#include "Media/Audio/AudioPlayer.h"

#include "Library/Random/Random.h"

GraphicsImage *shop_ui_background = nullptr;

std::array<GraphicsImage *, 12> shop_ui_items_in_store;

bool isStealingModeActive() {
    return keyboardInputHandler->IsStealingToggled() && pParty->activeCharacter().CanSteal();
}

void GUIWindow_Shop::mainDialogue() {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    if (HouseUI_CheckIfPlayerCanInteract()) {
        pShopOptions[0] = localization->GetString(LSTR_STANDARD);
        pShopOptions[1] = localization->GetString(LSTR_SPECIAL);
        pShopOptions[2] = localization->GetString(LSTR_DISPLAY);
        pShopOptions[3] = localization->GetString(LSTR_LEARN_SKILLS);

        int all_text_height = 0;
        for (int i = 0; i < 4; ++i)
            all_text_height += pFontArrus->CalcTextHeight(pShopOptions[i], dialogwin.uFrameWidth, 0);

        int textspacings = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - all_text_height) / 4;
        int textoffset = SIDE_TEXT_BOX_BODY_TEXT_OFFSET - (textspacings / 2);
        int pNumString = 0;

        for (int i = pDialogueWindow->pStartingPosActiveItem; i < pDialogueWindow->pNumPresenceButton + pDialogueWindow->pStartingPosActiveItem; ++i) {
            GUIButton *pButton = pDialogueWindow->GetControl(i);
            pButton->uY = textspacings + textoffset;
            pButton->uHeight = pFontArrus->CalcTextHeight(pShopOptions[pNumString], dialogwin.uFrameWidth, 0);
            textoffset = pButton->uY + pFontArrus->CalcTextHeight(pShopOptions[pNumString], dialogwin.uFrameWidth, 0) - 1;
            pButton->uW = textoffset + 6;

            Color pColorText = colorTable.Jonquil;
            if (pDialogueWindow->pCurrentPosActiveItem != i) {
                pColorText = colorTable.White;
            }

            dialogwin.DrawTitleText(pFontArrus, 0, pButton->uY, pColorText, pShopOptions[pNumString], 3);
            ++pNumString;
        }
    }
}

void GUIWindow_Shop::displayEquipmentDialogue() {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    draw_leather();
    CharacterUI_InventoryTab_Draw(&pParty->activeCharacter(), true);

    pShopOptions[0] = localization->GetString(LSTR_SELL);
    pShopOptions[1] = localization->GetString(LSTR_IDENTIFY);
    pShopOptions[2] = localization->GetString(LSTR_REPAIR);

    int options;
    if (_buildingType == BuildingType_AlchemistShop) {
        options = 2;
    } else {
        options = 3;
    }

    int all_text_height = 0;
    for (int i = 0; i < options; ++i)
        all_text_height += pFontArrus->CalcTextHeight(pShopOptions[i], dialogwin.uFrameWidth, 0);

    int textspacings = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - all_text_height) / options;
    int textoffset = SIDE_TEXT_BOX_BODY_TEXT_OFFSET - (textspacings / 2);
    int pNumString = 0;

    for (int i = pDialogueWindow->pStartingPosActiveItem; i < pDialogueWindow->pNumPresenceButton + pDialogueWindow->pStartingPosActiveItem; ++i) {
        GUIButton *pButton = pDialogueWindow->GetControl(i);
        pButton->uY = textspacings + textoffset;
        pButton->uHeight = pFontArrus->CalcTextHeight(pShopOptions[pNumString], dialogwin.uFrameWidth, 0);
        textoffset = pButton->uY + pFontArrus->CalcTextHeight(pShopOptions[pNumString], dialogwin.uFrameWidth, 0) - 1;
        pButton->uW = textoffset + 6;

        Color pColorText = colorTable.Jonquil;
        if (pDialogueWindow->pCurrentPosActiveItem != i)
            pColorText = colorTable.White;
        dialogwin.DrawTitleText(pFontArrus, 0, pButton->uY, pColorText, pShopOptions[pNumString], 3);
        ++pNumString;
    }
}

void GUIWindow_Shop::sellDialogue() {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    draw_leather();
    CharacterUI_InventoryTab_Draw(&pParty->activeCharacter(), true);

    if (HouseUI_CheckIfPlayerCanInteract()) {
        GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_SELECT_ITEM_TO_SELL), Color());

        Pointi pt = dialogwin.mouse->GetCursorPos();

        int invindex = ((pt.x - 14) / 32) + 14 * ((pt.y - 17) / 32);
        if (pt.x <= 13 || pt.x >= 462)
            return;

        int pItemID = pParty->activeCharacter().GetItemListAtInventoryIndex(invindex);
        if (pItemID) {
            ItemGen *item = &pParty->activeCharacter().pInventoryItemList[pItemID - 1];
            MERCHANT_PHRASE phrases_id = pParty->activeCharacter().SelectPhrasesTransaction(item, _buildingType, wData.val, 3);
            std::string str = BuildDialogueString(pMerchantsSellPhrases[phrases_id], pParty->activeCharacterIndex() - 1, item, wData.val, 3);
            int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - pFontArrus->CalcTextHeight(str, dialogwin.uFrameWidth, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
            dialogwin.DrawTitleText(pFontArrus, 0, vertMargin, colorTable.White, str, 3);
        }
    }
}

void GUIWindow_Shop::identifyDialogue() {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    draw_leather();
    CharacterUI_InventoryTab_Draw(&pParty->activeCharacter(), true);

    if (HouseUI_CheckIfPlayerCanInteract()) {
        GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_SELECT_ITEM_TO_IDENTIFY), Color());

        Pointi pt = EngineIocContainer::ResolveMouse()->GetCursorPos();

        int invindex = ((pt.x - 14) >> 5) + 14 * ((pt.y - 17) >> 5);
        if (pt.x <= 13 || pt.x >= 462)
            return;

        int pItemID = pParty->activeCharacter().GetItemListAtInventoryIndex(invindex);

        if (pItemID) {
            ItemGen *item = &pParty->activeCharacter().pInventoryItemList[pItemID - 1];

            std::string str;
            if (!item->IsIdentified()) {
                MERCHANT_PHRASE phrases_id = pParty->activeCharacter().SelectPhrasesTransaction(item, _buildingType, wData.val, 4);
                str = BuildDialogueString(pMerchantsIdentifyPhrases[phrases_id], pParty->activeCharacterIndex() - 1, item, wData.val, 4);
            } else {
                str = BuildDialogueString("%24", pParty->activeCharacterIndex() - 1, item, wData.val, 4);
            }

            int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - pFontArrus->CalcTextHeight(str, dialogwin.uFrameWidth, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
            dialogwin.DrawTitleText(pFontArrus, 0, vertMargin, colorTable.White, str, 3);
        }
    }
}

void GUIWindow_Shop::repairDialogue() {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    draw_leather();
    CharacterUI_InventoryTab_Draw(&pParty->activeCharacter(), true);

    if (HouseUI_CheckIfPlayerCanInteract()) {
        GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_SELECT_ITEM_TO_REPAIR), Color());

        Pointi pt = dialogwin.mouse->GetCursorPos();

        int invindex = ((pt.x - 14) >> 5) + 14 * ((pt.y - 17) >> 5);
        if (pt.x <= 13 || pt.x >= 462)
            return;

        int pItemID = pParty->activeCharacter().GetItemListAtInventoryIndex(invindex);
        if (pItemID == 0)
            return;

        if (pParty->activeCharacter().pOwnItems[pItemID - 1].uAttributes & ITEM_BROKEN) {
            ItemGen *item = &pParty->activeCharacter().pInventoryItemList[pItemID - 1];
            MERCHANT_PHRASE phrases_id = pParty->activeCharacter().SelectPhrasesTransaction(item, _buildingType, wData.val, 5);
            std::string str = BuildDialogueString(pMerchantsRepairPhrases[phrases_id], pParty->activeCharacterIndex() - 1, item, wData.val, 5);
            int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - pFontArrus->CalcTextHeight(str, dialogwin.uFrameWidth, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
            dialogwin.DrawTitleText(pFontArrus, 0, vertMargin, colorTable.White, str, 3);
        }
    }
}

void GUIWindow_Shop::learnSkillsDialogue() {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    if (!HouseUI_CheckIfPlayerCanInteract())
        return;

    int item_num = 0;
    int all_text_height = 0;
    int pPrice = PriceCalculator::skillLearningCostForPlayer(&pParty->activeCharacter(), buildingTable[wData.val - 1]);

    for (int i = pDialogueWindow->pStartingPosActiveItem; i < pDialogueWindow->pNumPresenceButton + pDialogueWindow->pStartingPosActiveItem; ++i) {
        auto skill = GetLearningDialogueSkill((DIALOGUE_TYPE)pDialogueWindow->GetControl(i)->msg_param);
        if (skillMaxMasteryPerClass[pParty->activeCharacter().classType][skill] != PLAYER_SKILL_MASTERY_NONE && !pParty->activeCharacter().pActiveSkills[skill]) {
            all_text_height += pFontArrus->CalcTextHeight(localization->GetSkillName(skill), dialogwin.uFrameWidth, 0);
            item_num++;
        }
    }

    SkillTrainingDialogue(&dialogwin, item_num, all_text_height, pPrice);
}

void GUIWindow_WeaponShop::shopWaresDialogue(bool isSpecial) {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    render->DrawTextureNew(8 / 640.0f, 8 / 480.0f, shop_ui_background);
    int item_X = 0;

    for (int i = 0; i < 6; ++i) {
        bool itemPresent = (isSpecial ? pParty->specialItemsInShops[houseId()][i].uItemID : pParty->standartItemsInShops[houseId()][i].uItemID) != ITEM_NULL;
        if (itemPresent) {
            render->DrawTextureNew(((60 - (shop_ui_items_in_store[i]->GetWidth() / 2)) + item_X) / 640.0f, (weapons_Ypos[i] + 30) / 480.0f, shop_ui_items_in_store[i]);
        }

        item_X += 70;
    }

    if (HouseUI_CheckIfPlayerCanInteract()) {
        int item_num = 0;
        for (int i = 0; i < 6; ++i) {
            item_num += (isSpecial ? pParty->specialItemsInShops[houseId()][i].uItemID : pParty->standartItemsInShops[houseId()][i].uItemID) != ITEM_NULL;
        }

        if (isStealingModeActive()) {
            GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_STEAL_ITEM), Color());
        } else {
            GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_SELECT_ITEM_TO_BUY), Color());
        }

        if (item_num) {
            Pointi pt = EngineIocContainer::ResolveMouse()->GetCursorPos();
            ItemGen *item;

            int testx = (pt.x - 30) / 70;
            if (testx >= 0 && testx < 6) {  // testx limits check
                if (isSpecial) {
                    item = &pParty->specialItemsInShops[houseId()][testx];
                } else {
                    item = &pParty->standartItemsInShops[houseId()][testx];
                }

                if (item->uItemID != ITEM_NULL) {  // item picking
                    int testpos = ((60 - (shop_ui_items_in_store[testx]->GetWidth() / 2)) + testx * 70);

                    if (pt.x >= testpos && pt.x < (testpos + shop_ui_items_in_store[testx]->GetWidth())) {
                        if (pt.y >= weapons_Ypos[testx] + 30 && pt.y < (weapons_Ypos[testx] + 30 + shop_ui_items_in_store[testx]->GetHeight())) {
                            std::string str;
                            if (!isStealingModeActive()) {
                                MERCHANT_PHRASE phrase = pParty->activeCharacter().SelectPhrasesTransaction(item, BuildingType_WeaponShop, wData.val, 2);
                                str = BuildDialogueString(pMerchantsBuyPhrases[phrase], pParty->activeCharacterIndex() - 1, item, wData.val, 2);
                            } else {
                                str = BuildDialogueString(localization->GetString(LSTR_STEAL_ITEM_FMT), pParty->activeCharacterIndex() - 1, item, wData.val, 2);
                            }
                            int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - pFontArrus->CalcTextHeight(str, dialogwin.uFrameWidth, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
                            dialogwin.DrawTitleText(pFontArrus, 0, vertMargin, colorTable.White, str, 3);
                        }
                    }
                }
            }
        } else {
            dialogwin.DrawShops_next_generation_time_string(pParty->PartyTimes.shopNextRefreshTime[houseId()] - pParty->GetPlayingTime());
        }
    }
}

void GUIWindow_ArmorShop::shopWaresDialogue(bool isSpecial) {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    render->DrawTextureNew(8 / 640.0f, 8 / 480.0f, shop_ui_background);
    int item_x = 0;

    for (int i = 0; i < 8; ++i) {
        bool itemPresent = (isSpecial ? pParty->specialItemsInShops[houseId()][i].uItemID : pParty->standartItemsInShops[houseId()][i].uItemID) != ITEM_NULL;

        if (itemPresent) {
            if (i >= 4) {
                render->DrawTextureNew(((90 - (shop_ui_items_in_store[i]->GetWidth() / 2)) + item_x - 420) / 640.0f, 126 / 480.0f, shop_ui_items_in_store[i]);
            } else {
                int x = ((86 - (shop_ui_items_in_store[i]->GetWidth() / 2)) + item_x);
                int y = (98 - shop_ui_items_in_store[i]->GetHeight());
                render->DrawTextureNew(x / 640.0f, y / 480.0f, shop_ui_items_in_store[i]);
            }
        }
        item_x += 105;
    }

    if (HouseUI_CheckIfPlayerCanInteract()) {
        int pItemCount = 0;
        for (int i = 0; i < 6; ++i) {
            pItemCount += (isSpecial ? pParty->specialItemsInShops[houseId()][i].uItemID : pParty->standartItemsInShops[houseId()][i].uItemID) != ITEM_NULL;
        }

        if (isStealingModeActive()) {
            GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_STEAL_ITEM), Color());
        } else {
            GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_SELECT_ITEM_TO_BUY), Color());
        }

        if (pItemCount) {
            Pointi pt = EngineIocContainer::ResolveMouse()->GetCursorPos();

            // testx limits check
            int testx = (pt.x - 40) / 105;
            if (testx >= 0 && testx < 4) {
                if (pt.y >= 126) {
                    testx += 4;
                }

                ItemGen *item;
                if (isSpecial) {
                    item = &pParty->specialItemsInShops[houseId()][testx];
                } else {
                    item = &pParty->standartItemsInShops[houseId()][testx];
                }

                if (item->uItemID != ITEM_NULL) {
                    int testpos;
                    if (testx >= 4) {
                        testpos = ((90 - (shop_ui_items_in_store[testx]->GetWidth() / 2)) + (testx * 105) - 420);
                    } else {
                        testpos = ((86 - (shop_ui_items_in_store[testx]->GetWidth() / 2)) + testx * 105);
                    }

                    if (pt.x >= testpos &&
                        pt.x <= testpos + shop_ui_items_in_store[testx]->GetWidth()) {
                        if ((pt.y >= 126 && pt.y < (126 + shop_ui_items_in_store[testx]->GetHeight())) ||
                            (pt.y <= 98 && pt.y >= (98 - shop_ui_items_in_store[testx]->GetHeight()))) {
                            // y is 126 to 126 + height low or 98-height to 98

                            std::string str;
                            if (!isStealingModeActive()) {
                                MERCHANT_PHRASE phrase = pParty->activeCharacter().SelectPhrasesTransaction(item, _buildingType, wData.val, 2);
                                str = BuildDialogueString(pMerchantsBuyPhrases[phrase], pParty->activeCharacterIndex() - 1, item, wData.val, 2);
                            } else {
                                str = BuildDialogueString(localization->GetString(LSTR_STEAL_ITEM_FMT), pParty->activeCharacterIndex() - 1, item, wData.val, 2);
                            }
                            int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - pFontArrus->CalcTextHeight(str, dialogwin.uFrameWidth, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
                            dialogwin.DrawTitleText(pFontArrus, 0, vertMargin, colorTable.White, str, 3);
                        }
                    }
                }
            }
        } else {
            dialogwin.DrawShops_next_generation_time_string(pParty->PartyTimes.shopNextRefreshTime[houseId()] - pParty->GetPlayingTime());
        }
    }
}

void GUIWindow_MagicAlchemyShop::shopWaresDialogue(bool isSpecial) {
    GUIWindow dialogwin = *this;
    dialogwin.uFrameX = SIDE_TEXT_BOX_POS_X;
    dialogwin.uFrameWidth = SIDE_TEXT_BOX_WIDTH;
    dialogwin.uFrameZ = SIDE_TEXT_BOX_POS_Z;

    render->DrawTextureNew(8 / 640.0f, 8 / 480.0f, shop_ui_background);

    for (uint i = 0; i < 12; ++i) {
        bool itemPresent = (isSpecial ? pParty->specialItemsInShops[houseId()][i].uItemID : pParty->standartItemsInShops[houseId()][i].uItemID) != ITEM_NULL;
        int itemx, itemy;

        if (itemPresent) {
            if (i >= 6) {
                itemy = 308 - shop_ui_items_in_store[i]->GetHeight();
                if (itemy < 0)
                    itemy = 0;
                itemx = 75 * i - shop_ui_items_in_store[i]->GetWidth() / 2 + 40 - 450;
            } else {
                itemy = 152 - shop_ui_items_in_store[i]->GetHeight();
                if (itemy < 0)
                    itemy = 0;
                itemx = 75 * i - shop_ui_items_in_store[i]->GetWidth() / 2 + 40;
            }

            if (i == 0 || i == 6) {
                if (itemx < 18)
                    itemx = 18;
            }

            if (i == 5 || i == 11) {
                if (itemx > 457 - shop_ui_items_in_store[i]->GetWidth()) {
                    itemx = 457 - shop_ui_items_in_store[i]->GetWidth();
                }
            }

            render->DrawTextureNew(itemx / 640.0f, itemy / 480.0f, shop_ui_items_in_store[i]);
        }
    }

    if (HouseUI_CheckIfPlayerCanInteract()) {
        int item_num = 0;

        for (int i = 0; i < 12; ++i) {
            item_num += (isSpecial ? pParty->specialItemsInShops[houseId()][i].uItemID : pParty->standartItemsInShops[houseId()][i].uItemID) != ITEM_NULL;
        }

        if (isStealingModeActive()) {
            GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_STEAL_ITEM), Color());
        } else {
            GameUI_StatusBar_DrawImmediate(localization->GetString(LSTR_SELECT_ITEM_TO_BUY), Color());
        }

        if (item_num) {
            Pointi pt = EngineIocContainer::ResolveMouse()->GetCursorPos();

            // testx limits check
            int testx = (pt.x) / 75;
            if (testx >= 0 && testx < 6) {
                if (pt.y > 152) {
                    testx += 6;
                }

                ItemGen *item;
                if (isSpecial) {
                    item = &pParty->specialItemsInShops[houseId()][testx];
                } else {
                    item = &pParty->standartItemsInShops[houseId()][testx];
                }

                if (item->uItemID != ITEM_NULL) {
                    int testpos;
                    if (pt.y > 152) {
                        testpos = 75 * testx - shop_ui_items_in_store[testx]->GetWidth() / 2 + 40 - 450;
                    } else {
                        testpos = 75 * testx - shop_ui_items_in_store[testx]->GetWidth() / 2 + 40;
                    }

                    if (pt.x >= testpos &&
                        pt.x <= testpos + shop_ui_items_in_store[testx]->GetWidth()) {
                        if ((pt.y <= 308 && pt.y >= (308 - shop_ui_items_in_store[testx]->GetHeight())) ||
                            (pt.y <= 152 && pt.y >= (152 - shop_ui_items_in_store[testx]->GetHeight()))) {
                            // y is 152-h to 152 or 308-height to 308

                            std::string str;
                            if (!isStealingModeActive()) {
                                MERCHANT_PHRASE phrase = pParty->activeCharacter().SelectPhrasesTransaction(item, _buildingType, wData.val, 2);
                                str = BuildDialogueString(pMerchantsBuyPhrases[phrase], pParty->activeCharacterIndex() - 1, item, wData.val, 2);
                            } else {
                                str = BuildDialogueString(localization->GetString(LSTR_STEAL_ITEM_FMT), pParty->activeCharacterIndex() - 1, item, wData.val, 2);
                            }
                            int vertMargin = (SIDE_TEXT_BOX_BODY_TEXT_HEIGHT - pFontArrus->CalcTextHeight(str, dialogwin.uFrameWidth, 0)) / 2 + SIDE_TEXT_BOX_BODY_TEXT_OFFSET;
                            dialogwin.DrawTitleText(pFontArrus, 0, vertMargin, colorTable.White, str, 3);
                        }
                    }
                }
            }
        } else {
            dialogwin.DrawShops_next_generation_time_string(pParty->PartyTimes.shopNextRefreshTime[houseId()] - pParty->GetPlayingTime());
        }
    }
}

void GUIWindow_Shop::houseSpecificDialogue() {
    switch (dialog_menu_id) {
      case DIALOGUE_MAIN:
        mainDialogue();
        break;
      case DIALOGUE_SHOP_BUY_STANDARD:
        shopWaresDialogue(false);
        break;
      case DIALOGUE_SHOP_BUY_SPECIAL:
        shopWaresDialogue(true);
        break;
      case DIALOGUE_SHOP_DISPLAY_EQUIPMENT:
        displayEquipmentDialogue();
        break;
      case DIALOGUE_SHOP_SELL:
        sellDialogue();
        break;
      case DIALOGUE_SHOP_IDENTIFY:
        identifyDialogue();
        break;
      case DIALOGUE_SHOP_REPAIR:
        repairDialogue();
        break;
      case DIALOGUE_LEARN_SKILLS:
        learnSkillsDialogue();
        break;
      default:
        break;
    }
}

void GUIWindow_Shop::houseDialogueOptionSelected(DIALOGUE_TYPE option) {
    // Nothing
}

void GUIWindow_Shop::processStealingResult(int stealingResult, int fineToAdd) {  // not working properly??
    int reputationDelta = 0;

    if (stealingResult == 0 || stealingResult == 1) {  // got caught
        pParty->uFine = std::clamp(pParty->uFine + fineToAdd, 0, 4000000);
        if (pParty->uFine) {
            for (Player &player : pParty->pPlayers) {
                if (!player._achievedAwardsBits[Award_Fine]) {
                    player._achievedAwardsBits.set(Award_Fine);
                }
            }
        }
        if (stealingResult == 1)
            reputationDelta = 2;
        else
            reputationDelta = 1;

        // only ban when caught
        pParty->PartyTimes.shopBanTimes[houseId()] = pParty->GetPlayingTime().AddDays(1);
    } else {
        reputationDelta = 2;
    }

    pParty->InTheShopFlags[std::to_underlying(houseId())] = 1;

    LocationInfo &loc = currentLocationInfo();
    loc.reputation += reputationDelta;
    if (loc.reputation > 10000)
        loc.reputation = 10000;
}

//----- (004BDB56) --------------------------------------------------------
void UIShop_Buy_Identify_Repair() {
    unsigned int pItemID;  // esi@20
    ItemGen *item;         // esi@21

    // int v18;                   // ecx@37
    float pPriceMultiplier;    // ST1C_4@38
    int taken_item;            // eax@40
    ItemGen *bought_item = nullptr;      // esi@51
    int party_reputation;      // eax@55
    int v39;                   // eax@63
    uint16_t *pSkill;  // esi@77
    int v55;                   // [sp+0h] [bp-B4h]@26
    int a6;                    // [sp+98h] [bp-1Ch]@57
    int a3;                    // [sp+9Ch] [bp-18h]@53
    unsigned int uNumSeconds;  // [sp+A4h] [bp-10h]@53
    unsigned int invindex;     // [sp+A8h] [bp-Ch]@9
    int uPriceItemService;     // [sp+ACh] [bp-8h]@12

    if (current_screen_type == CURRENT_SCREEN::SCREEN_SHOP_INVENTORY) {
        pParty->activeCharacter().OnInventoryLeftClick();
        return;
    }

    if (!HouseUI_CheckIfPlayerCanInteract()) {
        pAudioPlayer->playUISound(SOUND_error);
        return;
    }

    Pointi pt = EngineIocContainer::ResolveMouse()->GetCursorPos();

    switch (dialog_menu_id) {
        case DIALOGUE_SHOP_DISPLAY_EQUIPMENT: {
            current_character_screen_window = WINDOW_CharacterWindow_Inventory;
            pParty->activeCharacter().OnInventoryLeftClick();
            break;
        }

        case DIALOGUE_GUILD_BUY_BOOKS: {
            int testx = (pt.x - 32) / 70;
            if (testx >= 0 && testx < 6) {
                if (pt.y >= 250) {
                    testx += 6;
                }

                bought_item = &pParty->spellBooksInGuilds[window_SpeakInHouse->houseId()][testx];
                if (bought_item->uItemID != ITEM_NULL) {
                    int testpos;
                    if (pt.y >= 250) {
                        testpos = 32 + 70 * testx - 420;
                    } else {
                        testpos = 32 + 70 * testx;
                    }

                    if (pt.x >= testpos && pt.x <= testpos + shop_ui_items_in_store[testx]->GetWidth()) {
                        if ((pt.y >= 90 && pt.y <= (90 + shop_ui_items_in_store[testx]->GetHeight())) ||
                            (pt.y >= 250 && pt.y <= (250 + shop_ui_items_in_store[testx]->GetHeight()))) {
                            pPriceMultiplier = buildingTable[window_SpeakInHouse->wData.val - 1].fPriceMultiplier;
                            uPriceItemService = PriceCalculator::itemBuyingPriceForPlayer(&pParty->activeCharacter(),
                                                                                          bought_item->GetValue(), pPriceMultiplier);

                            if (pParty->GetGold() < uPriceItemService) {
                                PlayHouseSound(window_SpeakInHouse->wData.val, (HouseSoundID)2);
                                GameUI_SetStatusBar(LSTR_NOT_ENOUGH_GOLD);
                                return;
                            }

                            taken_item = pParty->activeCharacter().AddItem(-1, bought_item->uItemID);
                            if (taken_item) {
                                bought_item->SetIdentified();
                                pParty->activeCharacter().pInventoryItemList[taken_item - 1] = *bought_item;
                                dword_F8B1E4 = 1;
                                pParty->TakeGold(uPriceItemService);
                                bought_item->Reset();
                                render->ClearZBuffer();
                                pParty->activeCharacter().playReaction(SPEECH_ItemBuy);
                                return;
                            }

                            pParty->activeCharacter().playReaction(SPEECH_NoRoom);
                            GameUI_SetStatusBar(LSTR_INVENTORY_IS_FULL);
                            break;
                        }
                    }
                }
            }

            return;  // no item
            break;
        }

        case DIALOGUE_SHOP_SELL: {
            invindex = ((pt.x - 14) >> 5) + 14 * ((pt.y - 17) >> 5);
            if (pt.x <= 13 || pt.x >= 462 ||
                (pItemID = pParty->activeCharacter().GetItemListAtInventoryIndex(invindex),
                 !pItemID))
                return;

            if (pParty->activeCharacter()
                    .pInventoryItemList[pItemID - 1]
                    .MerchandiseTest(window_SpeakInHouse->wData.val)) {
                dword_F8B1E4 = 1;
                pParty->activeCharacter().SalesProcess(
                    invindex, pItemID - 1, window_SpeakInHouse->wData.val);
                render->ClearZBuffer();
                pParty->activeCharacter().playReaction(SPEECH_ItemSold);
                return;
            }

            pParty->activeCharacter().playReaction(SPEECH_WrongShop);
            pAudioPlayer->playUISound(SOUND_error);
            break;
        }

        case DIALOGUE_SHOP_IDENTIFY: {
            invindex = ((pt.x - 14) >> 5) + 14 * ((pt.y - 17) >> 5);
            if (pt.x <= 13 || pt.x >= 462 ||
                (pItemID = pParty->activeCharacter().GetItemListAtInventoryIndex(invindex),
                 !pItemID))
                return;

            uPriceItemService = PriceCalculator::itemIdentificationPriceForPlayer(
                &pParty->activeCharacter(), buildingTable[window_SpeakInHouse->wData.val - 1].fPriceMultiplier);
            item = &pParty->activeCharacter().pInventoryItemList[pItemID - 1];

            if (!(item->uAttributes & ITEM_IDENTIFIED)) {
                if (item->MerchandiseTest(window_SpeakInHouse->wData.val)) {
                    if (pParty->GetGold() >= uPriceItemService) {
                        dword_F8B1E4 = 1;
                        pParty->TakeGold(uPriceItemService);
                        item->uAttributes |= ITEM_IDENTIFIED;
                        pParty->activeCharacter().playReaction(SPEECH_ShopIdentify);
                        GameUI_SetStatusBar(LSTR_DONE);
                        return;
                    }

                    PlayHouseSound(window_SpeakInHouse->wData.val,
                                   (HouseSoundID)2);
                    return;
                }

                pAudioPlayer->playUISound(SOUND_error);
                pParty->activeCharacter().playReaction(SPEECH_WrongShop);
                return;
            }

            pParty->activeCharacter().playReaction(SPEECH_AlreadyIdentified);
            break;
        }

        case DIALOGUE_SHOP_REPAIR: {
            invindex = ((pt.x - 14) >> 5) + 14 * ((pt.y - 17) >> 5);
            if (pt.x <= 13 || pt.x >= 462 ||
                (pItemID = pParty->activeCharacter().GetItemListAtInventoryIndex(
                         invindex),
                 !pItemID))
                return;

            item = &pParty->activeCharacter().pInventoryItemList[pItemID - 1];
            pPriceMultiplier =
                buildingTable[window_SpeakInHouse->wData.val - 1]
                    .fPriceMultiplier;
            uPriceItemService = PriceCalculator::itemRepairPriceForPlayer(&pParty->activeCharacter(), item->GetValue(), pPriceMultiplier);

            if (item->uAttributes & ITEM_BROKEN) {
                if (item->MerchandiseTest(window_SpeakInHouse->wData.val)) {
                    if (pParty->GetGold() >= uPriceItemService) {
                        dword_F8B1E4 = 1;
                        pParty->TakeGold(uPriceItemService);
                        item->uAttributes =
                            (item->uAttributes & ~ITEM_BROKEN) | ITEM_IDENTIFIED;
                        pParty->activeCharacter().playReaction(SPEECH_ShopRepair);
                        GameUI_SetStatusBar(LSTR_GOOD_AS_NEW);
                        return;
                    }

                    PlayHouseSound(window_SpeakInHouse->wData.val,
                                   (HouseSoundID)2);
                    return;
                }

                pAudioPlayer->playUISound(SOUND_error);
                pParty->activeCharacter().playReaction(SPEECH_WrongShop);
                return;
            }

            pParty->activeCharacter().playReaction(SPEECH_AlreadyIdentified);
            break;
        }

        case DIALOGUE_SHOP_BUY_STANDARD:
        case DIALOGUE_SHOP_BUY_SPECIAL: {
            int testx;
            int testpos;

            switch (in_current_building_type) {
                case BuildingType_WeaponShop:

                    testx = (pt.x - 30) / 70;
                    if (testx >= 0 && testx < 6) {
                        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD)
                            bought_item = &pParty->standartItemsInShops[window_SpeakInHouse->houseId()][testx];
                        else
                            bought_item = &pParty->specialItemsInShops[window_SpeakInHouse->houseId()][testx];

                        if (bought_item->uItemID != ITEM_NULL) {
                            testpos =
                                ((60 -
                                  ((signed int)shop_ui_items_in_store[testx]
                                       ->GetWidth() /
                                   2)) +
                                 testx * 70);
                            if (pt.x >= testpos &&
                                pt.x <
                                    (testpos +
                                     (signed int)shop_ui_items_in_store[testx]
                                         ->GetWidth())) {
                                if (pt.y >= weapons_Ypos[testx] + 30 &&
                                    pt.y < (weapons_Ypos[testx] + 30 +
                                               shop_ui_items_in_store[testx]
                                                   ->GetHeight())) {
                                    break;  // good
                                }
                            } else {
                                bought_item = nullptr;
                                return;
                            }
                        } else {
                            return;
                        }
                    }
                    break;

                case BuildingType_ArmorShop:

                    testx = (pt.x - 40) / 105;
                    if (testx >= 0 && testx < 4) {
                        if (pt.y >= 126) {
                            testx += 4;
                        }

                        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD)
                            bought_item = &pParty->standartItemsInShops[window_SpeakInHouse->houseId()][testx];
                        else
                            bought_item = &pParty->specialItemsInShops[window_SpeakInHouse->houseId()][testx];

                        if (bought_item->uItemID != ITEM_NULL) {
                            if (testx >= 4) {
                                testpos = ((90 - (shop_ui_items_in_store[testx]
                                                      ->GetWidth() /
                                                  2)) +
                                           (testx * 105) - 420);  // low row
                            } else {
                                testpos = ((86 - (shop_ui_items_in_store[testx]
                                                      ->GetWidth() /
                                                  2)) +
                                           testx * 105);
                            }

                            if (pt.x >= testpos &&
                                pt.x <=
                                    testpos + shop_ui_items_in_store[testx]
                                                  ->GetWidth()) {
                                if ((pt.y >= 126 &&
                                    pt.y <
                                         (126 + shop_ui_items_in_store[testx]
                                                    ->GetHeight())) ||
                                    (pt.y <= 98 &&
                                        pt.y >=
                                         (98 - shop_ui_items_in_store[testx]
                                                   ->GetHeight()))) {
                                    break;  // good
                                }
                            } else {
                                bought_item = nullptr;
                                return;
                            }
                        } else {
                            return;
                        }
                    }
                    break;

                case BuildingType_AlchemistShop:
                case BuildingType_MagicShop:

                    testx = (pt.x) / 75;
                    if (testx >= 0 && testx < 6) {
                        if (pt.y > 152) {
                            testx += 6;
                        }

                        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD)
                            bought_item = &pParty->standartItemsInShops[window_SpeakInHouse->houseId()][testx];
                        else
                            bought_item = &pParty->specialItemsInShops[window_SpeakInHouse->houseId()][testx];

                        if (bought_item->uItemID != ITEM_NULL) {
                            if (pt.y > 152) {
                                testpos =
                                    75 * testx -
                                    shop_ui_items_in_store[testx]->GetWidth() /
                                        2 +
                                    40 - 450;
                            } else {
                                testpos =
                                    75 * testx -
                                    shop_ui_items_in_store[testx]->GetWidth() /
                                        2 +
                                    40;
                            }

                            if (pt.x >= testpos &&
                                pt.x <=
                                    testpos + shop_ui_items_in_store[testx]
                                                  ->GetWidth()) {
                                if ((pt.y <= 308 &&
                                    pt.y >=
                                         (308 - shop_ui_items_in_store[testx]
                                                    ->GetHeight())) ||
                                    (pt.y <= 152 &&
                                        pt.y >=
                                         (152 - shop_ui_items_in_store[testx]
                                                    ->GetHeight()))) {
                                    // y is 152-h to 152 or 308-height to 308
                                    break;  // good
                                }
                            } else {
                                bought_item = nullptr;
                                return;
                            }
                        } else {
                            return;
                        }
                    }
                    break;

                default:
                    return;
            }

            uPriceItemService = PriceCalculator::itemBuyingPriceForPlayer(&pParty->activeCharacter(), bought_item->GetValue(),
                                                                          buildingTable[window_SpeakInHouse->wData.val - 1].fPriceMultiplier);
            uNumSeconds = 0;
            a3 = 0;
            if (pMapStats->GetMapInfo(pCurrentMapName))
                a3 = pMapStats->pInfos[pMapStats->GetMapInfo(pCurrentMapName)]
                         ._steal_perm;
            party_reputation = pParty->GetPartyReputation();
            if (isStealingModeActive()) {
                uNumSeconds = pParty->activeCharacter().StealFromShop(bought_item, a3, party_reputation, 0, &a6);
                if (!uNumSeconds) {
                    // caught stealing no item
                    ((GUIWindow_Shop*)window_SpeakInHouse)->processStealingResult(0, a6);
                    return;
                }
            } else if (pParty->GetGold() < uPriceItemService) {
                PlayHouseSound(window_SpeakInHouse->wData.val, (HouseSoundID)2);
                GameUI_SetStatusBar(LSTR_NOT_ENOUGH_GOLD);
                return;
            }

            v39 = pParty->activeCharacter().AddItem(-1, bought_item->uItemID);
            if (v39) {
                bought_item->SetIdentified();
                pParty->activeCharacter().pInventoryItemList[v39 - 1] = *bought_item;
                if (uNumSeconds != 0) {  // stolen
                    pParty->activeCharacter().pInventoryItemList[v39 - 1].SetStolen();
                    ((GUIWindow_Shop*)window_SpeakInHouse)->processStealingResult(uNumSeconds, a6);
                } else {
                    dword_F8B1E4 = 1;
                    pParty->TakeGold(uPriceItemService);
                }
                bought_item->Reset();
                render->ClearZBuffer();
                pParty->activeCharacter().playReaction(SPEECH_ItemBuy);
                return;
            } else {
                pParty->activeCharacter().playReaction(SPEECH_NoRoom);
                GameUI_SetStatusBar(LSTR_INVENTORY_IS_FULL);
                return;
            }
            break;
        }
        default:  // if click video screen in shop
        {
            if (IsSkillLearningDialogue(dialog_menu_id)) {
                PLAYER_SKILL_TYPE skill = GetLearningDialogueSkill(dialog_menu_id);
                uPriceItemService =
                    PriceCalculator::skillLearningCostForPlayer(&pParty->activeCharacter(),
                                                                                buildingTable[window_SpeakInHouse->wData.val - 1]);
                if (skillMaxMasteryPerClass[pParty->activeCharacter().classType][skill] != PLAYER_SKILL_MASTERY_NONE) {
                    pSkill = &pParty->activeCharacter().pActiveSkills[skill];
                    if (!*pSkill) {
                        if (pParty->GetGold() < uPriceItemService) {
                            GameUI_SetStatusBar(LSTR_NOT_ENOUGH_GOLD);
                            if (in_current_building_type == BuildingType_Training)
                                v55 = HouseSound_Goodbye;
                            else
                                v55 = HouseSound_NotEnoughMoney;
                            PlayHouseSound(
                                window_SpeakInHouse->wData.val,
                                (HouseSoundID)v55);
                            return;
                        }
                        pParty->TakeGold(uPriceItemService);
                        dword_F8B1E4 = 1;
                        *pSkill = 1;
                        pParty->activeCharacter().playReaction(SPEECH_SkillLearned);
                        return;
                    }
                }
            }
            break;
        }
    }
}

void showSpellbookInfo(ITEM_TYPE spellItemId) {
    // TODO(captainurist): deal away with casts.
    SPELL_TYPE spellId = static_cast<SPELL_TYPE>(std::to_underlying(spellItemId) - 399);
    int spellLevel = (std::to_underlying(spellItemId) - 400) % 11 + 1;
    unsigned int spellSchool = 4 * (std::to_underlying(spellItemId) - 400) / 11;

    Pointi cursorPos = EngineIocContainer::ResolveMouse()->GetCursorPos();
    int popupVertPos = 30;
    if (cursorPos.y <= 320) {
        popupVertPos = cursorPos.y + 30;
    }

    GUIWindow popup;
    popup.Init();
    popup.uFrameY = popupVertPos;
    popup.uFrameWidth = 328;
    popup.uFrameHeight = 68;
    popup.uFrameX = 90;
    popup.uFrameZ = 417;
    popup.uFrameW = popupVertPos + 67;

    int maxLineWidth = std::max({
        pFontSmallnum->GetLineWidth(localization->GetString(LSTR_NORMAL)),
        pFontSmallnum->GetLineWidth(localization->GetString(LSTR_EXPERT)),
        pFontSmallnum->GetLineWidth(localization->GetString(LSTR_MASTER)),
        pFontSmallnum->GetLineWidth(localization->GetString(LSTR_GRAND))});

    std::string str = fmt::format("{}\n\n{}\t{:03}:\t{:03}{}\t000\n{}\t{:03}:\t{:03}{}\t000\n{}\t{:03}:\t{:03}{}\t000\n{}\t{:03}:\t{:03}{}",
                                  pSpellStats->pInfos[spellId].pDescription,
                                  localization->GetString(LSTR_NORMAL), maxLineWidth + 3, maxLineWidth + 10, pSpellStats->pInfos[spellId].pBasicSkillDesc,
                                  localization->GetString(LSTR_EXPERT), maxLineWidth + 3, maxLineWidth + 10, pSpellStats->pInfos[spellId].pExpertSkillDesc,
                                  localization->GetString(LSTR_MASTER), maxLineWidth + 3, maxLineWidth + 10, pSpellStats->pInfos[spellId].pMasterSkillDesc,
                                  localization->GetString(LSTR_GRAND), maxLineWidth + 3, maxLineWidth + 10, pSpellStats->pInfos[spellId].pGrandmasterSkillDesc);

    popup.uFrameHeight += pFontSmallnum->CalcTextHeight(str, popup.uFrameWidth, 0);
    if (popup.uFrameHeight < 150) {
        popup.uFrameHeight = 150;
    }
    popup.uFrameWidth = game_viewport_width;
    popup.DrawMessageBox(0);
    popup.uFrameWidth -= 12;
    popup.uFrameHeight -= 12;
    popup.uFrameZ = popup.uFrameX + popup.uFrameWidth - 1;
    popup.uFrameW = popup.uFrameHeight + popup.uFrameY - 1;
    popup.DrawTitleText(pFontArrus, 0x78u, 0xCu, colorTable.PaleCanary, pSpellStats->pInfos[spellId].name, 3u);
    popup.DrawText(pFontSmallnum, {120, 44}, Color(), str, 0, 0, Color());
    popup.uFrameZ = popup.uFrameX + 107;
    popup.uFrameWidth = 108;
    popup.DrawTitleText(pFontComic, 0xCu, 0x4Bu, Color(), localization->GetSkillName(static_cast<PLAYER_SKILL_TYPE>(spellSchool / 4 + 12)), 3u);

    str = fmt::format("{}\n{}", localization->GetString(LSTR_SP_COST), pSpellDatas[spellId].uNormalLevelMana);
    popup.DrawTitleText(pFontComic, 0xCu, popup.uFrameHeight - pFontComic->GetHeight() - 16, Color(), str, 3);
}

//----- new function
void ShowPopupShopSkills() {
    int pX = 0;
    int pY = 0;
    mouse->GetClickPos(&pX, &pY);

    if (pDialogueWindow) {
        for (GUIButton *pButton : pDialogueWindow->vButtons) {
            if (pX >= pButton->uX && pX < pButton->uZ && pY >= pButton->uY && pY < pButton->uW) {
                if (IsSkillLearningDialogue((DIALOGUE_TYPE)pButton->msg_param)) {
                    auto skill_id = GetLearningDialogueSkill((DIALOGUE_TYPE)pButton->msg_param);
                    if (skillMaxMasteryPerClass[pParty->activeCharacter().classType][skill_id] != PLAYER_SKILL_MASTERY_NONE &&
                        !pParty->activeCharacter().pActiveSkills[skill_id]) {
                        // is this skill visible
                        std::string pSkillDescText = CharacterUI_GetSkillDescText(pParty->activeCharacterIndex() - 1, skill_id);
                        CharacterUI_DrawTooltip(localization->GetSkillName(skill_id), pSkillDescText);
                    }
                }
            }
        }
    }
}


//----- (004B1A2D) --------------------------------------------------------
void ShowPopupShopItem() {
    ItemGen *item;  // ecx@13
    int invindex;
    int testpos;

    if (in_current_building_type == BuildingType_Invalid) return;
    if (dialog_menu_id < DIALOGUE_SHOP_BUY_STANDARD) return;

    Pointi pt = EngineIocContainer::ResolveMouse()->GetCursorPos();
    int testx;

    if (in_current_building_type <= BuildingType_AlchemistShop) {
        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD ||
            dialog_menu_id == DIALOGUE_SHOP_BUY_SPECIAL) {
            switch (in_current_building_type) {
                case BuildingType_WeaponShop: {
                    testx = (pt.x - 30) / 70;
                    if (testx >= 0 && testx < 6) {
                        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD)
                            item = &pParty->standartItemsInShops[window_SpeakInHouse->houseId()][testx];
                        else
                            item = &pParty->specialItemsInShops[window_SpeakInHouse->houseId()][testx];

                        if (item->uItemID != ITEM_NULL) {
                            testpos =
                                ((60 -
                                  (shop_ui_items_in_store[testx]->GetWidth() /
                                   2)) +
                                 testx * 70);
                            if (pt.x >= testpos &&
                                pt.x <
                                    (testpos + shop_ui_items_in_store[testx]
                                                   ->GetWidth())) {
                                if (pt.y >= weapons_Ypos[testx] + 30 &&
                                    pt.y < (weapons_Ypos[testx] + 30 +
                                               shop_ui_items_in_store[testx]
                                                   ->GetHeight())) {
                                    GameUI_DrawItemInfo(item);
                                }
                            } else {
                                return;
                            }
                        }
                    } else {
                        return;
                    }

                    break;
                }

                case BuildingType_ArmorShop:
                    testx = (pt.x - 40) / 105;
                    if (testx >= 0 && testx < 4) {
                        if (pt.y >= 126) {
                            testx += 4;
                        }

                        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD)
                            item = &pParty->standartItemsInShops[window_SpeakInHouse->houseId()][testx];
                        else
                            item = &pParty->specialItemsInShops[window_SpeakInHouse->houseId()][testx];

                        if (item->uItemID != ITEM_NULL) {
                            if (testx >= 4) {
                                testpos = ((90 - (shop_ui_items_in_store[testx]
                                                      ->GetWidth() /
                                                  2)) +
                                           (testx * 105) - 420);  // low row
                            } else {
                                testpos = ((86 - (shop_ui_items_in_store[testx]
                                                      ->GetWidth() /
                                                  2)) +
                                           testx * 105);
                            }

                            if (pt.x >= testpos &&
                                pt.x <=
                                    testpos + shop_ui_items_in_store[testx]
                                                  ->GetWidth()) {
                                if ((pt.y >= 126 &&
                                    pt.y <
                                         (126 + shop_ui_items_in_store[testx]
                                                    ->GetHeight())) ||
                                    (pt.y <= 98 &&
                                        pt.y >=
                                         (98 - shop_ui_items_in_store[testx]
                                                   ->GetHeight()))) {
                                    GameUI_DrawItemInfo(item);
                                } else {
                                    return;
                                }
                            }
                        } else {
                            return;
                        }
                    }
                    break;

                case BuildingType_AlchemistShop:
                case BuildingType_MagicShop:
                    testx = (pt.x) / 75;
                    // testx limits check
                    if (testx >= 0 && testx < 6) {
                        if (pt.y >= 152) {
                            testx += 6;
                        }

                        if (dialog_menu_id == DIALOGUE_SHOP_BUY_STANDARD)
                            item = &pParty->standartItemsInShops[window_SpeakInHouse->houseId()][testx];
                        else
                            item = &pParty->specialItemsInShops[window_SpeakInHouse->houseId()][testx];

                        if (item->uItemID != ITEM_NULL) {
                            if (pt.y > 152) {
                                testpos =
                                    75 * testx -
                                    shop_ui_items_in_store[testx]->GetWidth() /
                                        2 +
                                    40 - 450;
                            } else {
                                testpos =
                                    75 * testx -
                                    shop_ui_items_in_store[testx]->GetWidth() /
                                        2 +
                                    40;
                            }

                            if (pt.x >= testpos &&
                                pt.x <=
                                    testpos + shop_ui_items_in_store[testx]
                                                  ->GetWidth()) {
                                if ((pt.y <= 308 &&
                                    pt.y >=
                                         (308 - shop_ui_items_in_store[testx]
                                                    ->GetHeight())) ||
                                    (pt.y <= 152 &&
                                        pt.y >=
                                         (152 - shop_ui_items_in_store[testx]
                                                    ->GetHeight()))) {
                                    GameUI_DrawItemInfo(item);
                                } else {
                                    return;
                                }
                            }
                        } else {
                            return;
                        }
                    }
                    break;

                default:
                    // v3 = render->pActiveZBuffer[mouse.x +
                    // pSRZBufferLineOffsets[mouse.y]] & 0xFFFF; if (!v3)
                    // return;
                    // v7 = &pParty->StandartItemsInShops[(unsigned
                    // int)window_SpeakInHouse->ptr_1C][v3 - 1]; if
                    // (dialog_menu_id == DIALOGUE_SHOP_BUY_SPECIAL) v7 =
                    // &pParty->SpecialItemsInShops[(unsigned
                    // int)window_SpeakInHouse->ptr_1C][v3 - 1];
                    // GameUI_DrawItemInfo(v7);
                    return;
                    break;
            }
        }

        if (dialog_menu_id >= DIALOGUE_SHOP_SELL &&
                dialog_menu_id <= DIALOGUE_SHOP_REPAIR ||
            dialog_menu_id == DIALOGUE_SHOP_DISPLAY_EQUIPMENT) {
            invindex = ((pt.x - 14) >> 5) + 14 * ((pt.y - 17) >> 5);
            if (pt.x <= 13 || pt.x >= 462 ||
                !pParty->activeCharacter().GetItemListAtInventoryIndex(
                    invindex))
                return;

            GameUI_DrawItemInfo(
                pParty->activeCharacter().GetItemAtInventoryIndex(invindex));
            return;
        }
    }

    if (in_current_building_type <= BuildingType_MirroredPath && dialog_menu_id == DIALOGUE_GUILD_BUY_BOOKS) {
        int testx = (pt.x - 32) / 70;
        if (testx >= 0 && testx < 6) {
            if (pt.y >= 250) {
                testx += 6;
            }

            item = &pParty->spellBooksInGuilds[window_SpeakInHouse->houseId()][testx];

            if (item->uItemID != ITEM_NULL) {
                int testpos;
                if (pt.y >= 250) {
                    testpos = 32 + 70 * testx - 420;
                } else {
                    testpos = 32 + 70 * testx;
                }

                if (pt.x >= testpos && pt.x <= testpos + shop_ui_items_in_store[testx]->GetWidth()) {
                    if ((pt.y >= 90 && pt.y <= (90 + shop_ui_items_in_store[testx]->GetHeight())) || (pt.y >= 250 && pt.y <= (250 + shop_ui_items_in_store[testx]->GetHeight()))) {
                        showSpellbookInfo(pParty->spellBooksInGuilds[window_SpeakInHouse->houseId()][testx].uItemID);
                    }
                }
            }
        }
    }
}