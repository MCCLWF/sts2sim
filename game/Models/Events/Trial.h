// Models/Events/Trial.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"
#include "game/Runs/RunManager.h"

namespace sts2 {

struct Trial : EventModel {
    SIM_MODEL(Trial, EventModel)

    // The source's EntrantNumber is picked by Rng.Chaotic and is used only in
    // localized event text. Chaotic is presentation-only and is not represented
    // by the deterministic Env, so do not consume this event's gameplay RNG for it.
    void CalculateVars() const {}

    EventOptionList GenerateInitialOptions() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(Trial, Accept), InitialOptionKey("ACCEPT")));
        options.push_back(EventOption(SIM_OPTION(Trial, Reject), InitialOptionKey("REJECT")));
        return options;
    }

    void Accept(const EventOptionState&) const {
        switch (Rng().NextInt(3)) {
        case 0: ShowMerchantTrial(); return;
        case 1: ShowNobleTrial(); return;
        case 2: ShowNondescriptTrial(); return;
        default: throw GameError("Trial: NextInt(3) returned an invalid trial");
        }
    }

    void Reject(const EventOptionState&) const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(Trial, Accept), OptionKey("REJECT", "ACCEPT")));

        // Source constructor order: disableOnChosen:false, isProceed:true.
        EventOption double_down(SIM_OPTION(Trial, DoubleDown), OptionKey("REJECT", "DOUBLE_DOWN"),
                                /*isProceed=*/true, /*disableOnChosen=*/false);
        double_down.s.will_kill_player = register_will_kill_fn(
            STS2_NATIVE_CALLBACK(WillKillPlayer, Trial, (&AlwaysWarnPlayerDeath), "AlwaysWarnPlayerDeath"));
        options.push_back(double_down);
        SetEventState(L10NLookup("TRIAL.pages.REJECT.description"), options);
    }

    // NAbandonRunConfirmPopup._Ready / OnYesButtonPressed / OnNoButtonPressed.
    // Represent the modal input as an ordinary prefix choice; neither opening
    // it nor cancelling changes the event page. See deviations D-013.
    void DoubleDown(const EventOptionState&) const {
        ChoiceRequest request;
        request.kind = ChoiceKind::Option;
        request.option_count = 2;
        request.prompt = "ABANDON_RUN_CONFIRMATION.body";
        request.source = "NAbandonRunConfirmPopup";
        request.options.push_back({.kind = ChoiceOptionKind::Alternative, .key = "GENERIC_POPUP.confirm"});
        request.options.push_back({.kind = ChoiceOptionKind::Alternative, .key = "GENERIC_POPUP.cancel"});
        const int32_t answer = sim().Choose(request);
        if (answer == 0) RunManager::Abandon(&sim());
        else if (answer != 1) throw InvalidRequest("abandon confirmation: answer out of range");
    }

    void MerchantGuilty(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::REGRET, Owner());
        for (int i = 0; i < 2; ++i) {
            RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        }
        SetTrialFinished("TRIAL.pages.MERCHANT_GUILTY.description");
    }

    void MerchantInnocent(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::SHAME, Owner());
        CardList selected = CardSelectCmd::FromDeckForUpgrade(Owner(), CardSelectorPrefs(2));
        for (CardModel card : selected) CardCmd::Upgrade(card);
        SetTrialFinished("TRIAL.pages.MERCHANT_INNOCENT.description");
    }

    void NobleGuilty(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), Dec(10));
        SetTrialFinished("TRIAL.pages.NOBLE_GUILTY.description");
    }

    void NobleInnocent(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::REGRET, Owner());
        PlayerCmd::GainGold(Dec(300), Owner());
        SetTrialFinished("TRIAL.pages.NOBLE_INNOCENT.description");
    }

    void NondescriptGuilty(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::DOUBT, Owner());
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithDefaultOdds(
            {sts2::data(Owner().Character()).card_pool});
        StaticVec<Reward, 8> rewards;
        for (int i = 0; i < 2; ++i) rewards.push_back(Reward::NewCardReward(Owner(), options, 3));
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetTrialFinished("TRIAL.pages.NONDESCRIPT_GUILTY.description");
    }

    void NondescriptInnocent(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::DOUBT, Owner());
        CardList selected = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(2));
        for (CardModel card : selected) CardCmd::TransformToRandom(card, Rng());
        SetTrialFinished("TRIAL.pages.NONDESCRIPT_INNOCENT.description");
    }

private:
    static bool AlwaysWarnPlayerDeath(Player, Dec) { return true; }

    void ShowMerchantTrial() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(Trial, MerchantGuilty), OptionKey("MERCHANT", "GUILTY")));
        options.push_back(EventOption(SIM_OPTION(Trial, MerchantInnocent), OptionKey("MERCHANT", "INNOCENT")));
        SetTrialStory("TRIAL.pages.MERCHANT.description", options);
    }

    void ShowNobleTrial() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(Trial, NobleGuilty), OptionKey("NOBLE", "GUILTY")));
        options.push_back(EventOption(SIM_OPTION(Trial, NobleInnocent), OptionKey("NOBLE", "INNOCENT")));
        SetTrialStory("TRIAL.pages.NOBLE.description", options);
    }

    void ShowNondescriptTrial() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(Trial, NondescriptGuilty), OptionKey("NONDESCRIPT", "GUILTY")));
        options.push_back(EventOption(SIM_OPTION(Trial, NondescriptInnocent), OptionKey("NONDESCRIPT", "INNOCENT")));
        SetTrialStory("TRIAL.pages.NONDESCRIPT.description", options);
    }

    void SetTrialFinished(std::string_view resultKey) const {
        // Save the nested string key for a future LocString interpolation API;
        // the EventState description remains the source's outer LocString key.
        extra(0) = static_cast<int32_t>(MakeLocKey(resultKey).hash);
        SetEventFinished(L10NLookup("TRIAL.trialResult"));
    }

    void SetTrialStory(std::string_view storyKey, const EventOptionList& options) const {
        // The source interpolates this raw localized story into trialFormat.
        // Keep the outer description key and retain the nested key in event
        // scratch state while the Env has no LocString StringVar support.
        extra(0) = static_cast<int32_t>(MakeLocKey(storyKey).hash);
        SetEventState(L10NLookup("TRIAL.trialFormat"), options);
    }
};

}  // namespace sts2
