#include <fstream>
#include <iostream>
#include <iterator>

#include "env/combat_prediction_queries.h"
#include "env/snapshot_json.h"
#include "game/All.h"

using namespace sts2;

int main(int argc, char** argv) {
    if (argc != 2 && !(argc == 3 && (std::string_view(argv[2]) == "--downgrade" || std::string_view(argv[2]) == "--calculated" || std::string_view(argv[2]) == "--orbs" || std::string_view(argv[2]) == "--powers")))
        throw GameError("expected root snapshot path and optional --downgrade/--calculated/--orbs/--powers");
    std::ifstream file(argv[1]);
    if (!file) throw GameError("cannot read root snapshot");
    std::string document((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // The native getter fixture and this executable are separate processes.
    // Prime the identical static Cultists registry and rebind only runtime_id;
    // ordinary production imports still reject cross-runtime documents.
    Env metadata;
    metadata.ResetCombat(NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    const auto source = snapshot_detail::Parser(document).parse();
    const auto local = snapshot_detail::Parser(metadata.ExportSnapshotJson()).parse();
    for (const char* field : {"schema_sha256", "model_id_hash", "registry_scope"}) {
        const auto& left = source.at(field);
        const auto& right = local.at(field);
        if (left.kind != right.kind || left.text != right.text)
            throw GameError(std::string("query fixture incompatible ") + field);
    }
    const std::string from = "\"runtime_id\":\"" + source.at("runtime_id").text + "\"";
    const std::string to = "\"runtime_id\":\"" + local.at("runtime_id").text + "\"";
    auto runtime_at = document.find(from);
    if (runtime_at == std::string::npos || document.find(from, runtime_at + 1) != std::string::npos)
        throw GameError("query fixture runtime identity location invalid");
    document.replace(runtime_at, from.size(), to);
    auto root = CombatPredictionSession::FromGameSnapshot(document);
    if (argc == 3 && std::string_view(argv[2]) == "--downgrade") {
        for (CardModel card : Player(&root->environment().sim(), 1).PlayerCombatState().DiscardPile().Cards())
            if (card.CardId_() == CardId::RAMPAGE || card.CardId_() == CardId::THRASH) card.DowngradeInternal();
        std::cout << root->environment().ExportSnapshotJson() << '\n';
        return 0;
    }
    const auto hash = root->Hash();
    const auto before = root->ExportSnapshotJson();
    const bool calculated = argc == 3 && std::string_view(argv[2]) == "--calculated";
    const bool orbs = argc == 3 && std::string_view(argv[2]) == "--orbs";
    const bool powers = argc == 3 && std::string_view(argv[2]) == "--powers";
    auto read = [&](const CombatPredictionSession& session) { return powers ? ExportCombatPredictionPowers(session) : orbs ? ExportCombatPredictionOrbs(session) : calculated ? ExportCombatPredictionCalculatedVars(session)
        : ExportCombatPredictionQueries(session); };
    const auto query = read(*root);
    if (read(*root) != query
        || root->Hash() != hash || root->ExportSnapshotJson() != before)
        throw GameError("query changed source/hash/snapshot or was nondeterministic");
    auto clone = root->Clone();
    if (read(*clone) != query)
        throw GameError("query clone differs");
    if (!calculated && !orbs && !powers) {
        // Generic getters keep actual attachment identity and ordinary card
        // fields; search coverage refusal now belongs to the Mod adapter.
        auto special = root->Clone();
        CardRef ref = special->environment().sim().g->player_combat.piles[0][0];
        special->environment().sim().g->cards[ref].id = CardId::RAMPAGE;
        special->environment().sim().g->cards[ref].enchantment = EnchantmentId::STEADY;
        const auto special_before = special->ExportSnapshotJson();
        const auto special_query = snapshot_detail::Parser(ExportCombatPredictionQueries(*special)).parse();
        const auto& first = special_query.at("piles").array[0].at("cards").array[0];
        if (first.at("card_state").at("enchantment_id").text != "STEADY"
            || first.object.contains("fingerprint_input_error")
            || first.object.contains("fingerprint_input")
            || special->ExportSnapshotJson() != special_before || root->ExportSnapshotJson() != before)
            throw GameError("raw attachment identity missing or query changed source");
    }
    // An unsupported query remains a refusal with no source/sibling writes.
    auto rejected = root->Clone();
    CardRef ref = rejected->environment().sim().g->player_combat.piles[0][0];
    rejected->environment().sim().g->cards[ref].id = CardId::STARDUST; // Star-X is not ported in this baseline.
    const auto bad_before = rejected->ExportSnapshotJson();
    bool unsupported = false;
    try {
        read(*rejected);
    } catch (const Unported&) { unsupported = true; }
    if (!unsupported || rejected->ExportSnapshotJson() != bad_before || root->ExportSnapshotJson() != before)
        throw GameError("unsupported query accepted or changed a source");
    std::cout << query << '\n';
}
